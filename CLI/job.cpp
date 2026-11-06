#include "job.hpp"
#include "segel/core.hpp"
#include "opencl_backend.hpp"
#include "progress.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <thread>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#else
#include <unistd.h>
#endif
namespace cli {
std::atomic<bool> cancelled{false};
namespace {
namespace fs=std::filesystem;
using Clock=std::chrono::steady_clock;
constexpr const char* quality="ingenieurmodell_nicht_validiert";
constexpr const char* model="H1";
struct Logger {
    Clock::time_point start=Clock::now();
    bool terminal=false;
    mutable bool visible=false;
    mutable size_t packages=0,units=0,total_units=0;
    mutable std::string current="Eingabepruefung";
    mutable Clock::time_point section_start=Clock::now();
#ifdef _WIN32
    HANDLE handle=GetStdHandle(STD_ERROR_HANDLE);
    DWORD original_mode=0;
#endif
    Logger() {
#ifdef _WIN32
        terminal=GetConsoleMode(handle,&original_mode)&&
            SetConsoleMode(handle,original_mode|ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#else
        terminal=isatty(STDERR_FILENO)!=0;
#endif
    }
    ~Logger() {
        if(visible)std::cerr<<"\x1b[0m"<<std::flush;
#ifdef _WIN32
        if(terminal)SetConsoleMode(handle,original_mode);
#endif
    }
    void clear() const {
        if(visible) {
            std::cerr<<"\x1b[2A\r\x1b[2K\n\x1b[2K\r\x1b[1A";
            visible=false;
        }
    }
    void draw() const {
        if(!terminal)return;
        std::cerr<<progress_line("Gesamt",packages,6,"\x1b[94m",current)<<"\n"
                 <<progress_line("Paket ",units,total_units,"\x1b[92m",
                     progress_time(std::chrono::duration<double>(Clock::now()-section_start).count(),units,total_units))
                 <<"\n"<<std::flush;
        visible=true;
    }
    void progress(const std::string& phase,size_t done,size_t total,
                  const std::string& fallback) const {
        if(phase!=current||done<units||total!=total_units||done==0) section_start=Clock::now();
        if(!terminal) {
            current=phase;units=done;total_units=total;
            log(phase,fallback+" | "+progress_time(
                std::chrono::duration<double>(Clock::now()-section_start).count(),done,total));
            return;
        }
        clear();current=phase;units=done;total_units=total;draw();
    }
    void log(const std::string& phase,const std::string& text,const char* severity="INFO") const {
        if(terminal) {
            clear();
            const std::array<const char*,6> phases={"Eingabepruefung","Geometrieaufbereitung",
                "Profilberechnung","Gesamtsegelintegration","Qualitaetspruefung","Export"};
            for(size_t i=0;i<phases.size();++i)if(phase==phases[i]&&current!=phase) {
                packages=i;current=phase;units=0;total_units=0;section_start=Clock::now();
            }
            if(phase=="Export"&&text.find("Gesamt: 100 %")!=std::string::npos) {
                packages=6;units=total_units;current="Abgeschlossen";
            }
        }
        auto now=std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        std::tm tm{};
#ifdef _WIN32
        localtime_s(&tm,&now);
#else
        localtime_r(&now,&tm);
#endif
        std::cerr<<std::put_time(&tm,"%H:%M:%S")<<" "<<severity<<" ["<<phase<<"] "<<text
                 <<" | Laufzeit: "<<std::chrono::duration_cast<std::chrono::seconds>(Clock::now()-start).count()<<" s\n";
        if(terminal&&std::string(severity)!="FEHLER")draw();
    }
};
void ensure(bool b,int code,const std::string& text) { if(!b)throw Error(code,text); }
void fields(const J& j,std::initializer_list<const char*> names,const std::string& where) {
    ensure(j.is_object(),2,where+": Objekt erforderlich");
    std::set<std::string> allowed;for(auto n:names)allowed.insert(n);
    for(auto it=j.begin();it!=j.end();++it)ensure(allowed.count(it.key())!=0,2,where+"."+it.key()+": unbekanntes Feld");
}
double numeric(const J& j,const std::string& key) {
    ensure(j.contains(key)&&j.at(key).is_number(),2,key+": endliche Zahl erforderlich");
    double v=j.at(key).get<double>();ensure(std::isfinite(v),2,key+": nicht endlich");return v;
}
void write(const fs::path& path,const J& value) {
    std::ofstream out(path,std::ios::binary);out.exceptions(std::ios::badbit|std::ios::failbit);
    out<<value.dump(2)<<'\n';out.close();
}
unsigned processors() {
    unsigned n=std::max(1u,std::thread::hardware_concurrency());
#ifdef _WIN32
    DWORD_PTR proc=0,system=0;
    if(GetProcessAffinityMask(GetCurrentProcess(),&proc,&system)) {
        unsigned count=0;while(proc){count+=static_cast<unsigned>(proc&1);proc>>=1;}if(count)n=std::min(n,count);
    }
#endif
    return n;
}
struct Input {
    fs::path path,geometrypath,conventionpath;
    J job,geom,conv,parameters,grid;
    J original_hashes;
    std::vector<double> angles,elevations,res;
    int budget=1,requested=1,cli_limit=0;
    mutable std::atomic<int> active{0},peak{0};
    double atol=0,rtol=0;
    std::unique_ptr<segel::Context> context;
    std::unique_ptr<OpenClPolarBackend> gpu;
    std::string gpu_reason;
    bool gpu_required=false;
    bool gpu_auto=false;
    Input(fs::path p,int limit):path(fs::absolute(p)),cli_limit(limit) {
        job=read(path);
        fields(job,{"schema_version","status","eingabe","berechnung","betriebspunkte","formvarianten","numerik","qualitaetspruefung","ausgabe","ressourcen"},"auftrag");
        ensure(job.at("schema_version")=="0.1",2,"auftrag.schema_version nicht unterstuetzt");
        fields(job.at("eingabe"),{"segeldatei","konventionen"},"eingabe");
        geometrypath=(path.parent_path()/fs::u8path(job.at("eingabe").at("segeldatei").get<std::string>())).lexically_normal();
        conventionpath=(path.parent_path()/fs::u8path(job.at("eingabe").at("konventionen").get<std::string>())).lexically_normal();
        geom=read(geometrypath);conv=read(conventionpath);
        original_hashes={{"auftrag",hash(path)},{"geometrie",hash(geometrypath)},{"konventionen",hash(conventionpath)}};
        ensure(conv.at("schema_version")=="0.1"&&geom.at("schema_version")=="0.1",2,"Geometrie/Konventionen.schema_version unpassend");
        const auto& conventions=conv.at("konvention");
        ensure(conventions.at("laengeneinheit")=="m"&&conventions.at("winkeleinheit")=="deg"&&
               conventions.at("lokales_koordinatensystem").at("ursprung")=="hals"&&
               conventions.at("lokales_koordinatensystem").at("x_positiv")=="voraus"&&
               conventions.at("lokales_koordinatensystem").at("y_positiv")=="backbord"&&
               conventions.at("lokales_koordinatensystem").at("z_positiv")=="oben",2,"Nicht unterstuetzte Koordinaten/Einheiten");
        ensure(conventions.at("bezuege").at("flaeche")=="ebene_grundkonturflaeche"&&
               conventions.at("bezuege").at("laenge")=="ebene_grundkonturflaeche_geteilt_durch_konturhoehe"&&
               conventions.at("bezuege").at("momentenursprung")=="hals"&&
               conventions.at("bezuege").at("profilmoment_sehnenanteil")==0.25,2,"Nicht unterstuetzte Bezuege");
        fields(job.at("ressourcen"),{"max_prozessorzahl","gpu"},"ressourcen");
        ensure(job.at("ressourcen").at("max_prozessorzahl").is_number_integer(),2,"ressourcen.max_prozessorzahl: positive Ganzzahl erforderlich");
        auto requested64=job.at("ressourcen").at("max_prozessorzahl").get<long long>();
        ensure(requested64>0&&requested64<=100000,2,"ressourcen.max_prozessorzahl ausserhalb 1..100000");
        requested=static_cast<int>(requested64);budget=std::min(requested,static_cast<int>(processors()));
        if(limit>0)budget=std::min(budget,limit);
        std::string gpu_mode=job.at("ressourcen").at("gpu");
        ensure(gpu_mode=="aus"||gpu_mode=="automatisch"||gpu_mode=="erforderlich",2,"ressourcen.gpu ungueltig");
        gpu_required=gpu_mode=="erforderlich";
        gpu_auto=gpu_mode=="automatisch";
        if(gpu_mode=="aus")gpu_reason="GPU im Auftrag ausgeschaltet";
        else {
            gpu=OpenClPolarBackend::create(gpu_reason);
            if(gpu_required)ensure(static_cast<bool>(gpu),3,"GPU erforderlich: "+gpu_reason);
        }
        const auto& c=job.at("berechnung");
        fields(c,{"produkte","modell","profilverfahren","gesamtsegelverfahren","abgeloeste_stroemung","modellstand","modellfestlegung","dreidimensionale_effekte","lokale_reynoldszahlen","gueltigkeit","parameter"},"berechnung");
        ensure(c.at("modell")=="physikalisches_hybridmodell"&&c.at("modellstand")==model,3,"berechnung.modellstand: ausfuehrbare Modellfestlegung H1 erforderlich");
        ensure(c.at("profilverfahren")=="duennprofil_fourier"&&c.at("gesamtsegelverfahren")=="prandtl_spektral_crossflow"&&
               c.at("abgeloeste_stroemung")=="mittelflaechen_impuls",3,"berechnung: nicht unterstuetztes Verfahren");
        ensure(c.at("produkte")==J::array({"profilpolaren","gesamtsegel_lookup"}),3,"berechnung.produkte: H1 erfordert beide Produkte");
        ensure(c.at("modellfestlegung").at("status")=="festgelegt",3,"berechnung.modellfestlegung: offen");
        fields(c.at("modellfestlegung"),{"status"},"modellfestlegung");
        if(c.contains("lokale_reynoldszahlen")) {
            fields(c.at("lokale_reynoldszahlen"),{"bereich","reynoldsabhaengigkeit","ausserhalb_referenzraster","bei_ungueltigen_werten"},"lokale_reynoldszahlen");
            ensure(c.at("lokale_reynoldszahlen").at("reynoldsabhaengigkeit")=="modellieren"&&
                   c.at("lokale_reynoldszahlen").at("bei_ungueltigen_werten")=="abbrechen",3,"Nicht unterstuetzte Reynolds-Regel");
        }
        const auto& d=c.at("dreidimensionale_effekte");
        fields(d,{"induzierte_stroemung","induzierter_widerstand","mast_und_segelinterferenz"},"dreidimensionale_effekte");
        ensure(d.at("induzierte_stroemung")=="modellieren"&&d.at("induzierter_widerstand")=="modellieren_ohne_doppelzaehlung"&&
               d.at("mast_und_segelinterferenz")=="vernachlaessigt",3,"dreidimensionale_effekte nicht unterstuetzt");
        parameters=c.at("parameter");
        fields(parameters,{"stall_start_deg","stall_end_deg","stall_re_shift_deg","transition_re","transition_width","viscous_cl2","normal_pressure","re_min","re_max"},"parameter");
        fields(job.at("formvarianten"),{"modus"},"formvarianten");
        ensure(job.at("formvarianten").at("modus")=="unveraendert_aus_segeldatei",3,"Formvariantenmodus nicht unterstuetzt");
        fields(job.at("betriebspunkte"),{"anstellwinkel_deg","reynoldszahlen_referenz","windneigung_deg"},"betriebspunkte");
        const auto& ar=job.at("betriebspunkte").at("anstellwinkel_deg");
        fields(ar,{"start","ende_exklusiv","schritt"},"anstellwinkel_deg");
        double start=numeric(ar,"start"),end=numeric(ar,"ende_exklusiv"),step=numeric(ar,"schritt");
        ensure(start==-180&&end==180&&step>0&&step<=30&&std::abs(360/step-std::round(360/step))<1e-10,2,"anstellwinkel_deg: zyklisches Raster -180..180 mit Schritt <=30 erforderlich");
        ensure(360/step<=10000,3,"Azimutraster zu gross");
        for(int i=0;i<static_cast<int>(std::llround(360/step));++i)angles.push_back(start+i*step);
        elevations=job.at("betriebspunkte").at("windneigung_deg").get<std::vector<double>>();
        ensure(elevations==std::vector<double>({0,10,20,30,40,50,60,70,80,90}),2,"windneigung_deg muss [0,10,...,90] sein");
        res=job.at("betriebspunkte").at("reynoldszahlen_referenz").get<std::vector<double>>();
        ensure(!res.empty()&&res.size()<=100,2,"reynoldszahlen_referenz leer/zu gross");
        ensure(angles.size()*elevations.size()*res.size()<=500000,3,"Rechenplan ueber Ressourcenlimit von 500000 Summenzeilen");
        for(size_t i=0;i<res.size();++i)ensure(std::isfinite(res[i])&&res[i]>0&&(i==0||res[i]>res[i-1]),2,"Re-Stufen muessen positiv/streng aufsteigend sein");
        const auto& q=job.at("qualitaetspruefung");
        fields(q,{"rasterverfeinerungsfaktoren","verfeinerung","vergleich","winkelrasterpruefung","relative_toleranz","absolute_toleranz_beiwerte","akzeptanzregel","bei_nicht_erreichter_toleranz"},"qualitaetspruefung");
        ensure(q.at("rasterverfeinerungsfaktoren")==J::array({1,2,4}),2,"Rasterfaktoren 1/2/4 erforderlich");
        ensure(q.at("winkelrasterpruefung").at("zwischenpunkte_pro_intervall")==1,2,"Ein Winkelzwischenpunkt erforderlich");
        atol=numeric(q,"absolute_toleranz_beiwerte");rtol=numeric(q,"relative_toleranz");
        ensure(atol>0&&rtol>0&&atol<=1&&rtol<=1,2,"Qualitaetstoleranzen ausserhalb (0,1]");
        fields(job.at("ausgabe"),{"vorhandene_ergebnisse"},"ausgabe");
        ensure(job.at("ausgabe").at("vorhandene_ergebnisse")=="nicht_ueberschreiben",2,"Ausgabeueberschreiben nicht erlaubt");
        fields(job.at("numerik"),{"streifenintegration","hoehenraster"},"numerik");
        fields(job.at("numerik").at("hoehenraster"),{"schritt_m","verfeinern_ab_eta","verfeinerter_schritt_m","kontur_und_profilstationen_einfuegen","nullsehnen_als_punkte_behandeln"},"hoehenraster");
        try {
            context=std::make_unique<segel::Context>(J{{"geometrie",geom},{"numerik",job.at("numerik")},{"parameter",parameters}}.dump());
            grid=J::parse(context->geometry());
            for(const auto& row:grid.at("grids").at("4"))
                for(double re:res) {
                    double local=re*row.at("chord").get<double>()/grid.at("c_ref_m").get<double>();
                    ensure(local>=numeric(parameters,"re_min")&&local<=numeric(parameters,"re_max"),3,"Lokale Re ausser Modellbereich, Profil "+row.at("profil_id").get<std::string>());
                }
        } catch(const Error&) {throw;} catch(const std::bad_alloc&){throw Error(3,"Geometrieaufbereitung: Speicher erschoepft");}
        catch(const std::exception& e){throw Error(2,geometrypath.u8string()+" / Auftrag.parameter: "+e.what());}
    }
};
struct Case { double a,b,re; int factor; int panels=48; };
using R=std::array<double,6>;
R vector(const J& j) {
    R r{};for(int i=0;i<3;++i){r[i]=j.at("CF")[i].get<double>();r[i+3]=j.at("CM")[i].get<double>();}return r;
}
R average(R a,R b) { R r{};for(int i=0;i<6;++i)r[i]=(a[i]+b[i])/2;return r; }
R weighted(std::initializer_list<std::pair<double,R>> terms) {
    R result{};
    for(const auto& term:terms)
        for(size_t i=0;i<result.size();++i)result[i]+=term.first*term.second[i];
    return result;
}
R slope_at(const std::vector<double>& axis,const std::vector<R>& values,size_t i,bool periodic) {
    size_t n=axis.size();
    if(n==2)return weighted({{1/(axis[1]-axis[0]),values[1]},{-1/(axis[1]-axis[0]),values[0]}});
    if(periodic) {
        size_t prev=(i+n-1)%n,next=(i+1)%n;
        double x0=axis[prev],x1=axis[next];
        if(prev>i)x0-=360;
        if(next<i)x1+=360;
        double h0=axis[i]-x0,h1=x1-axis[i];
        return weighted({{-h1/(h0*(h0+h1)),values[prev]},
                         {(h1-h0)/(h0*h1),values[i]},
                         {h0/(h1*(h0+h1)),values[next]}});
    }
    if(i==0) {
        double h0=axis[1]-axis[0],h1=axis[2]-axis[1];
        return weighted({{-(2*h0+h1)/(h0*(h0+h1)),values[0]},
                         {(h0+h1)/(h0*h1),values[1]},
                         {-h0/(h1*(h0+h1)),values[2]}});
    }
    if(i+1==n) {
        double h0=axis[n-2]-axis[n-3],h1=axis[n-1]-axis[n-2];
        return weighted({{h1/(h0*(h0+h1)),values[n-3]},
                         {-(h0+h1)/(h0*h1),values[n-2]},
                         {(h0+2*h1)/(h1*(h0+h1)),values[n-1]}});
    }
    double h0=axis[i]-axis[i-1],h1=axis[i+1]-axis[i];
    return weighted({{-h1/(h0*(h0+h1)),values[i-1]},
                     {(h1-h0)/(h0*h1),values[i]},
                     {h0/(h1*(h0+h1)),values[i+1]}});
}
R cubic(const std::vector<double>& axis,const std::vector<R>& values,size_t left,double x,bool periodic=false) {
    size_t right=(left+1)%axis.size();
    double x0=axis[left],x1=axis[right];
    if(periodic&&right==0)x1+=360;
    double h=x1-x0,t=(x-x0)/h,t2=t*t,t3=t2*t;
    R d0=slope_at(axis,values,left,periodic),d1=slope_at(axis,values,right,periodic);
    return weighted({{2*t3-3*t2+1,values[left]},
                     {(t3-2*t2+t)*h,d0},
                     {-2*t3+3*t2,values[right]},
                     {(t3-t2)*h,d1}});
}
struct Quality {
    struct Statistics {
        long double error_squared=0,reference_squared=0;
        double max_error=0,max_reference=0;
        void add(double error,double reference) {
            error_squared+=static_cast<long double>(error)*error;
            reference_squared+=static_cast<long double>(reference)*reference;
            max_error=std::max(max_error,error);
            max_reference=std::max(max_reference,reference);
        }
        void merge(const Statistics& other) {
            error_squared+=other.error_squared;reference_squared+=other.reference_squared;
            max_error=std::max(max_error,other.max_error);
            max_reference=std::max(max_reference,other.max_reference);
        }
        J report(size_t count) const {
            return {{"vergleiche",count},
                {"rms_abweichung",count?std::sqrt(static_cast<double>(error_squared/count)):0.0},
                {"rms_referenz",count?std::sqrt(static_cast<double>(reference_squared/count)):0.0},
                {"max_abweichung",max_error},{"max_referenz",max_reference},
                {"rms_relativ_prozent",reference_squared>0?J(100*std::sqrt(static_cast<double>(error_squared/reference_squared))):J(nullptr)},
                {"max_abweichung_zu_spitzenlast_prozent",max_reference>0?J(100*max_error/max_reference):J(nullptr)}};
        }
    };
    double atol,rtol,maxabs=0,maxratio=0,maxrel=0;
    Statistics force,moment;
    size_t comparisons=0,failures=0;
    std::map<std::string,size_t> by_kind;
    J failed=J::array();
    void compare(R fine,R other,const Case& c,const char* kind) {
        ++comparisons;
        bool bad=false;
        for(int part:{0,3}) {
            double d=0,n=0;for(int k=part;k<part+3;++k){d+=(fine[k]-other[k])*(fine[k]-other[k]);n+=fine[k]*fine[k];}
            d=std::sqrt(d);n=std::sqrt(n);
            (part==0?force:moment).add(d,n);
            maxabs=std::max(maxabs,d);maxratio=std::max(maxratio,d/(atol+rtol*n));
            if(n>1e-14)maxrel=std::max(maxrel,d/n);
            if(d>atol+rtol*n)bad=true;
        }
        if(bad) { ++failures;++by_kind[kind];if(failed.size()<100)failed.push_back({{"art",kind},{"alpha",c.a},{"beta",c.b},{"re",c.re}}); }
    }
    void merge(const Quality& other) {
        comparisons+=other.comparisons;failures+=other.failures;
        maxabs=std::max(maxabs,other.maxabs);maxratio=std::max(maxratio,other.maxratio);
        maxrel=std::max(maxrel,other.maxrel);
        force.merge(other.force);moment.merge(other.moment);
        for(const auto& entry:other.by_kind)by_kind[entry.first]+=entry.second;
        for(const auto& item:other.failed)if(failed.size()<100)failed.push_back(item);
    }
    J report() const { return {{"absolute_toleranz",atol},{"relative_toleranz",rtol},
        {"vergleiche",comparisons},{"fehler",failures},{"max_absolute_abweichung",maxabs},
        {"max_toleranzquotient",maxratio},{"fehler_nach_art",by_kind},{"max_relative_abweichung_bei_norm_ueber_1e-14",maxrel},
        {"fehlgeschlagene_faelle_erste_100",failed},
        {"prozentkennzahlen",{{"bezug","Numerische Vergleichsfaelle; Referenz ist jeweils fine, keine physikalische Fehlergarantie. Vektordifferenzen, nicht Differenzen der Betraege. Prozentwerte bei Nullreferenz sind null."},
            {"kraft",force.report(comparisons)},{"moment",moment.report(comparisons)}}}}; }
};
std::vector<R> batch(const std::vector<Case>& cases,const Input& input,const Logger& log,
                     const std::string& phase,size_t& done,size_t total,int phase_no) {
    std::vector<R> results(cases.size());std::atomic<size_t> next{0},finished{0};
    std::atomic<bool> failed{false};std::mutex error_mutex;std::string error;int errorcode=4;
    auto worker=[&] {
        try {
            while(!cancelled&&!failed) {
                size_t index=next.fetch_add(1);if(index>=cases.size())break;
                const auto& c=cases[index];
                struct Slot {
                    const Input& input;
                    explicit Slot(const Input& i):input(i) {
                        int now=++input.active,old=input.peak.load();
                        while(now>old&&!input.peak.compare_exchange_weak(old,now)) {}
                    }
                    ~Slot(){--input.active;}
                } slot(input);
                results[index]=vector(J::parse(input.context->solve(c.a,c.b,c.re,c.factor,false,c.panels)));
                finished.fetch_add(1);
            }
        } catch(const std::exception& e) {
            std::lock_guard<std::mutex> guard(error_mutex);error=e.what();
            if(dynamic_cast<const std::bad_alloc*>(&e))errorcode=3;failed=true;
        }
    };
    std::vector<std::thread> threads;
    try {for(int i=0;i<input.budget;++i)threads.emplace_back(worker);}
    catch(...) {failed=true;for(auto& t:threads)t.join();throw Error(3,"CPU-Worker konnten nicht gestartet werden");}
    auto last=Clock::now();
    log.progress(phase,0,cases.size(),"Beginn Betriebspunktgruppe | Teil: 0/"+std::to_string(cases.size()));
    while(finished<cases.size()&&!failed&&!cancelled) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if(Clock::now()-last>=std::chrono::seconds(1)) {
            auto n=finished.load();
            log.progress(phase,n,cases.size(),"Form eingabe, Raster/Betriebspunktgruppe | Teil: "+std::to_string(n)+"/"+std::to_string(cases.size())+
                " ("+std::to_string(100*n/cases.size())+" %) | Gesamt: "+std::to_string(90*(done+n)/total)+
                " % | Phasen: "+std::to_string(phase_no)+"/6");
            last=Clock::now();
        }
    }
    for(auto& t:threads)t.join();
    ensure(!cancelled,130,"Benutzerabbruch: eigene Worker beendet");
    ensure(!failed,errorcode,"Berechnung: "+error);
    done+=cases.size();
    log.progress(phase,cases.size(),cases.size(),"Abgeschlossen | Teil: "+std::to_string(cases.size())+"/"+std::to_string(cases.size())+
        " (100 %) | Gesamt: "+std::to_string(90*done/total)+" % | Phasen: "+std::to_string(phase_no)+"/6");
    return results;
}
J success(const fs::path& folder,const std::string& message) {
    return {{"status","erfolgreich"},{"exitcode",0},{"ergebnisordner",folder.empty()?J(nullptr):J(folder.u8string())},
        {"pruefbericht",folder.empty()?J(nullptr):J((folder/"pruefbericht.json").u8string())},{"meldung",message}};
}
}
J read(const fs::path& path) {
    try {
        std::ifstream in(path,std::ios::binary);ensure(in.good(),2,"Eingabepfad nicht lesbar: "+path.u8string());
        ensure(std::filesystem::file_size(path)<=16*1024*1024,3,"JSON-Eingabe groesser als 16 MiB: "+path.u8string());
        std::vector<std::set<std::string>> keys;
        return J::parse(in,[&keys](int,J::parse_event_t event,J& v) {
            if(event==J::parse_event_t::object_start)keys.emplace_back();
            if(event==J::parse_event_t::key)
                ensure(keys.back().insert(v.get<std::string>()).second,2,"Doppelter JSON-Schluessel: "+v.get<std::string>());
            if(event==J::parse_event_t::object_end)keys.pop_back();
            return true;
        });
    } catch(const Error& e){throw Error(e.code,path.u8string()+": "+e.what());}
    catch(const std::bad_alloc&){throw Error(3,"JSON-Eingabe: Speicher erschoepft");}
    catch(const std::exception& e) {throw Error(2,path.u8string()+": "+e.what());}
}
std::string hash(const fs::path& path) {
#ifdef _WIN32
    BCRYPT_ALG_HANDLE alg=nullptr;BCRYPT_HASH_HANDLE h=nullptr;
    ensure(BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0,5,"SHA256-Provider nicht verfuegbar");
    DWORD len=0,got=0;
    auto status=BCryptGetProperty(alg,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&len),sizeof(len),&got,0);
    std::vector<unsigned char> obj(len),digest(32);
    if(status>=0)status=BCryptCreateHash(alg,&h,obj.data(),len,nullptr,0,0);
    std::ifstream in(path,std::ios::binary);char data[65536];
    bool readable=in.good();
    while(status>=0&&in) {
        in.read(data,sizeof(data));
        if(in.gcount()>0)status=BCryptHashData(h,reinterpret_cast<PUCHAR>(data),static_cast<ULONG>(in.gcount()),0);
    }
    if(status>=0)status=BCryptFinishHash(h,digest.data(),32,0);
    if(h)BCryptDestroyHash(h);BCryptCloseAlgorithmProvider(alg,0);
    ensure(readable&&!in.bad()&&status>=0,5,"SHA256 fehlgeschlagen: "+path.u8string());
    std::ostringstream s;s<<std::hex<<std::setfill('0');for(auto x:digest)s<<std::setw(2)<<static_cast<int>(x);return s.str();
#else
    throw Error(3,"CLI-SHA256 derzeit nur Windows BCrypt; Core bleibt plattformunabhaengig");
#endif
}
J run(const fs::path& path,bool check,int limit) {
    Logger log;log.log("Eingabepruefung","Start: "+path.u8string()+" | Gesamt: wird ermittelt | Phasen: 0/6");
    Input input(path,limit);
    log.log("Eingabepruefung","Abgeschlossen, Segel "+input.grid.at("segel_id").get<std::string>());
    log.log("Ressourcen","CPU-Limit effektiv "+std::to_string(input.budget)+
        (input.gpu?"; OpenCL-GPU: "+input.gpu->device()+"; keine Speedpruefung":
                    "; GPU-Auswahl CPU: "+input.gpu_reason+"; keine Speedpruefung"));
    if(check) {
        ensure(input.job.at("status")=="bereit"||input.job.at("status")=="entwurf_rechenparameter_nicht_freigegeben",2,"auftrag.status unbekannt");
        return success({},input.job.at("status")=="bereit"?"Eingaben ausfuehrbar; ingenieurmodell_nicht_validiert":"Entwurf lesbar, lookup gesperrt");
    }
    ensure(input.job.at("status")=="bereit",2,"auftrag.status muss bereit sein; Entwurfsauftrag nicht ausfuehrbar");
    fs::path parent=input.path.parent_path(),output=parent/"LookUp",lock=parent/"LookUp.lock";
    ensure(!fs::exists(output),5,"LookUp bereits vorhanden");
    std::error_code ec;
    bool locked=fs::create_directory(lock,ec);
    ensure(locked&&!ec,5,"Ausgabe gesperrt oder nicht beschreibbar: "+lock.u8string());
    struct Unlock { fs::path p;~Unlock(){std::error_code e;fs::remove(p,e);} } unlock{lock};
    std::string token=std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    fs::path stage=parent/fs::u8path("LookUp.arbeiten."+token);
    ensure(fs::create_directory(stage,ec)&&!ec,5,"Arbeitsordner nicht anlegbar");
    J report={{"status","laufend"},{"physikalische_validierung",quality},{"modellstand",model},
        {"cpu_angefordert",input.requested},{"cpu_effektiv",input.budget},{"cli_limit",limit},
        {"gpu",{{"backend",input.gpu?"OpenCL":"CPU"},{"auswahl",input.gpu?"gpu":"cpu"},
                {"grund",input.gpu?"OpenCL-GPU mit fp64 fuer gebuendelte Profilpolaren":input.gpu_reason},
                {"plattform",input.gpu?J(input.gpu->platform()):J(nullptr)},
                {"geraet",input.gpu?J(input.gpu->device()):J(nullptr)},
                {"geeignete_funktionen",J::array({"profilpolaren"})},{"ausgefuehrte_polarwerte",0},
                {"cpu_gpu_vergleiche",0},{"max_cpu_gpu_abweichung",0.0},{"speedbenchmark",false}}},
        {"warnungen",J::array({"Keine vermessenen Segelpolaren; Abriss- und Hochwinkelschliessung nicht validiert",
                              "Keine Mast-/Rumpf-/Segelinterferenz",
                              "GPU beschleunigt nur Profilpolaren; Traglinie und Streifenintegration laufen auf CPU"})}};
    try {
        log.log("Geometrieaufbereitung","Beginn: Konturschnitt, Woelbung/Twist, Raster 1/2/4");
        const auto& grid=input.grid;
        log.log("Geometrieaufbereitung","Abgeschlossen: S="+std::to_string(grid.at("S_m2").get<double>())+" m2");
        const std::vector<double>& lookup_angles=input.angles;
        std::vector<double> lookup_elevations=input.elevations;
        std::vector<double> added_elevations;
        for(int interval=1;interval<16;++interval) {
            double beta=50+2.5*interval;
            if(std::find(lookup_elevations.begin(),lookup_elevations.end(),beta)==lookup_elevations.end()) {
                lookup_elevations.insert(std::lower_bound(lookup_elevations.begin(),lookup_elevations.end(),beta),beta);
                added_elevations.push_back(beta);
            }
        }
        lookup_elevations.insert(std::lower_bound(lookup_elevations.begin(),lookup_elevations.end(),88.75),88.75);
        added_elevations.push_back(88.75);
        std::vector<Case> base;
        for(double re:input.res)for(double a:lookup_angles)for(double b:lookup_elevations)base.push_back({a,b,re,4});
        size_t count=base.size(),total=count*10,done=0;
        log.log("Profilberechnung","Beginn: Fourier-Providerkonvergenz 256/512 geprueft; lokale Re geprueft");
        log.log("Profilberechnung","Abgeschlossen | Phasen: 3/6 | Gesamt: 0 %");
        log.log("Gesamtsegelintegration","Beginn: Grundraster; periodische und zweidimensionale Hermite-Interpolation");
        auto fine=batch(base,input,log,"Gesamtsegelintegration",done,total,3);
        auto coarsecases=base;for(auto& c:coarsecases)c.factor=2;
        auto coarse=batch(coarsecases,input,log,"Gesamtsegelintegration",done,total,3);
        for(auto& c:coarsecases)c.factor=1;
        auto rough=batch(coarsecases,input,log,"Gesamtsegelintegration",done,total,3);
        log.log("Qualitaetspruefung","Beginn: Hoehenraster 1/2/4, Wirbelprovider 32/48, kubische Zwischenpunkte");
        Quality q{input.atol,input.rtol};
        for(size_t i=0;i<count;++i) {
            q.compare(fine[i],coarse[i],base[i],"hoehenraster_2_4");
            q.compare(coarse[i],rough[i],base[i],"hoehenraster_1_2");
        }
        auto providercases=base;for(auto& c:providercases)c.panels=32;
        auto provider=batch(providercases,input,log,"Qualitaetspruefung",done,total,4);
        Quality provider_quality{input.atol,input.rtol};
        for(size_t i=0;i<count;++i)provider_quality.compare(fine[i],provider[i],base[i],"wirbelprovider_32_48");
        report["wirbel_provider"]=provider_quality.report();
        size_t na=lookup_angles.size(),nb=lookup_elevations.size();
        auto at=[&](size_t ir,size_t ia,size_t ib){return (ir*na+ia)*nb+ib;};
        std::vector<double> azmids(na);
        for(size_t ia=0;ia<na;++ia) {
            double next=ia+1<na?lookup_angles[ia+1]:lookup_angles[0]+360;
            azmids[ia]=(lookup_angles[ia]+next)/2;
        }
        std::vector<Case> azcases,betacases,cellcases;
        std::vector<std::tuple<size_t,size_t,size_t>> azrefs,betarefs,cellrefs;
        for(size_t ir=0;ir<input.res.size();++ir)
            for(size_t ia=0;ia<na;++ia)
                for(size_t ib=0;ib<nb;++ib) {
                    azcases.push_back({azmids[ia],lookup_elevations[ib],input.res[ir],4});
                    azrefs.emplace_back(ir,ia,ib);
                }
        for(size_t ir=0;ir<input.res.size();++ir)
            for(size_t ia=0;ia<na;++ia)
                for(size_t ib=0;ib+1<nb;++ib) {
                    double beta=(lookup_elevations[ib]+lookup_elevations[ib+1])/2;
                    betacases.push_back({lookup_angles[ia],beta,input.res[ir],4});
                    betarefs.emplace_back(ir,ia,ib);
                    cellcases.push_back({azmids[ia],beta,input.res[ir],4});
                    cellrefs.emplace_back(ir,ia,ib);
                }
        auto azvalues=batch(azcases,input,log,"Qualitaetspruefung",done,total,4);
        auto betavalues=batch(betacases,input,log,"Qualitaetspruefung",done,total,4);
        auto cellvalues=batch(cellcases,input,log,"Qualitaetspruefung",done,total,4);
        std::vector<R> azimuth_predictions(count);
        for(size_t ir=0;ir<input.res.size();++ir)
            for(size_t ia=0;ia<na;++ia)
                for(size_t ib=0;ib<nb;++ib) {
                    std::vector<R> values;values.reserve(na);
                    for(size_t j=0;j<na;++j)values.push_back(fine[at(ir,j,ib)]);
                    azimuth_predictions[at(ir,ia,ib)]=cubic(lookup_angles,values,ia,azmids[ia],true);
                }
        for(size_t k=0;k<azcases.size();++k) {
            auto [ir,ia,ib]=azrefs[k];
            q.compare(azvalues[k],azimuth_predictions[at(ir,ia,ib)],azcases[k],"azimut_cubic_hermite");
        }
        for(size_t k=0;k<betacases.size();++k) {
            auto [ir,ia,ib]=betarefs[k];
            std::vector<R> values;values.reserve(nb);
            for(size_t j=0;j<nb;++j)values.push_back(fine[at(ir,ia,j)]);
            q.compare(betavalues[k],cubic(lookup_elevations,values,ib,betacases[k].b),
                      betacases[k],"neigung_cubic_hermite");
        }
        for(size_t k=0;k<cellcases.size();++k) {
            auto [ir,ia,ib]=cellrefs[k];
            std::vector<R> elevation_values; elevation_values.reserve(nb);
            for(size_t j=0;j<nb;++j)elevation_values.push_back(azimuth_predictions[at(ir,ia,j)]);
            q.compare(cellvalues[k],cubic(lookup_elevations,elevation_values,ib,cellcases[k].b),
                      cellcases[k],"zelle_tensor_cubic_hermite");
        }
        for(size_t ir=0;ir<input.res.size();++ir)
            for(size_t ia=0;ia<na;++ia) {
                size_t pole=(ir*na+ia)*nb+nb-1,first=(ir*na)*nb+nb-1;
                q.compare(fine[pole],fine[first],base[pole],"pol_identitaet");
            }
        std::vector<Case> nearpole;std::vector<size_t> poleindexes;
        for(size_t ir=0;ir<input.res.size();++ir)
            for(size_t ia=0;ia<na;++ia) {
                size_t i=(ir*na+ia)*nb+nb-1;
                auto c=base[i];c.b=89.999;nearpole.push_back(c);poleindexes.push_back(i);
            }
        auto limitvalues=batch(nearpole,input,log,"Qualitaetspruefung",done,total,4);
        for(size_t k=0;k<poleindexes.size();++k)q.compare(fine[poleindexes[k]],limitvalues[k],nearpole[k],"pol_stetigkeit_89_999");
        report["numerik"]=q.report();
        report["interpolation_stuetzstellen"]={{"basis_azimut_deg",input.angles},{"basis_windneigung_deg",input.elevations},
            {"ausgegeben_azimut_deg",lookup_angles},{"ausgegeben_windneigung_deg",lookup_elevations},
            {"verfahren","periodische_tensorprodukt_cubic_hermite"},
            {"zusaetzliche_stuetzstellen",{{"azimut_deg",J::array()},{"windneigung_deg",added_elevations}}}};
        report["fourier_provider"]={{"punkte",J::array({256,512})},{"absolute_toleranz",1e-5},{"status","bestanden"}};
        ensure(q.failures==0&&provider_quality.failures==0,4,"Qualitaetspruefung fehlgeschlagen: "+
            std::to_string(q.failures)+" Raster/Winkel- und "+std::to_string(provider_quality.failures)+
            " Providervergleiche; Toleranzen bleiben unveraendert");
        log.log("Qualitaetspruefung","Abgeschlossen | Phasen: 5/6");
        log.log("Export","Beginn: sechs Produkte und SHA256");
        std::ofstream sums(stage/"summen.csv",std::ios::binary),forces(stage/"kraefte.csv",std::ios::binary),
                      profiles(stage/"profile.csv",std::ios::binary);
        for(auto s:{&sums,&forces,&profiles}) {s->exceptions(std::ios::badbit|std::ios::failbit);*s<<std::setprecision(17);}
        const std::string sumheader="form_id,alpha_deg,windneigung_deg,re_ref,CFx,CFy,CFz,CMx,CMy,CMz,angriff_x_m,angriff_y_m,angriff_z_m,angriff_gueltig,CM_rest_x,CM_rest_y,CM_rest_z,qualitaet";
        const std::string forceheader="form_id,alpha_deg,windneigung_deg,re_ref,beitrag_id,CFx,CFy,CFz,ansatz_x_m,ansatz_y_m,ansatz_z_m,CM_eigen_x,CM_eigen_y,CM_eigen_z,qualitaet";
        const std::string profileheader="profil_id,alpha_deg,re,CL,CD,CM,qualitaet";
        sums<<sumheader<<'\n';forces<<forceheader<<'\n';profiles<<profileheader<<'\n';
        auto components=[](std::ostream& out,const J& arr){for(const auto& x:arr)out<<','<<x.get<double>();};
        const size_t export_total=count+grid.at("grids").at("4").size()+1;
        size_t exported=0;auto last=Clock::now();
        log.progress("Export",0,export_total,"Beginn Tabellenexport | Teil: 0/"+std::to_string(export_total));
        for(double a:lookup_angles)for(double b:lookup_elevations)for(double re:input.res) {
            ensure(!cancelled,130,"Benutzerabbruch beim Export");
            J r=J::parse(input.context->solve(a,b,re,4,true));
            sums<<"eingabe,"<<a<<','<<b<<','<<re;
            components(sums,r.at("CF"));components(sums,r.at("CM"));
            if(r.at("valid").get<bool>())components(sums,r.at("point"));else sums<<",,,";
            sums<<','<<(r.at("valid").get<bool>()?1:0);components(sums,r.at("rest"));sums<<','<<quality<<'\n';
            for(const auto& l:r.at("loads")) {
                forces<<"eingabe,"<<a<<','<<b<<','<<re<<','<<l.at("id").get<int>();
                components(forces,l.at("f"));components(forces,l.at("r"));components(forces,l.at("m"));forces<<','<<quality<<'\n';
            }
            ++exported;
            if(Clock::now()-last>=std::chrono::seconds(1)) {
                log.progress("Export",exported,export_total,"Teil: "+std::to_string(exported)+"/"+std::to_string(export_total)+
                    " | Gesamt: "+std::to_string(90+8*exported/count)+" % | Phasen: 5/6");last=Clock::now();
            }
        }
        std::vector<J> profile_rows;
        for(const auto& row:grid.at("grids").at("4"))profile_rows.push_back(row);
        std::sort(profile_rows.begin(),profile_rows.end(),[](const J& a,const J& b){return a.at("profil_id")<b.at("profil_id");});
        const auto& p=input.parameters;
        PolarBatchParameters polar_parameters{
            p.at("stall_start_deg").get<double>(),p.at("stall_end_deg").get<double>(),
            p.at("stall_re_shift_deg").get<double>(),p.at("transition_re").get<double>(),
            p.at("transition_width").get<double>(),p.at("viscous_cl2").get<double>(),
            p.at("normal_pressure").get<double>()};
        size_t polar_values=0,cpu_gpu_checks=0;
        double max_cpu_gpu_error=0;
        bool gpu_fallback=false;
        bool gpu_used=false;
        const std::string gpu_platform=input.gpu?input.gpu->platform():"";
        const std::string gpu_device=input.gpu?input.gpu->device():"";
        for(const auto& row:profile_rows) {
            std::set<double> localangles;
            // Include the actual incidence queries (also the quality midpoints), not only a generic polar grid.
            for(int ai=0;ai<1440;++ai)localangles.insert(-180+ai*.25);
            auto insert_local=[&](double angle) {
                double local=std::remainder(angle-row.at("twist").get<double>(),360.);
                if(local==180)local=-180;
                localangles.insert(local);
            };
            for(double angle:lookup_angles)insert_local(angle);
            for(size_t ai=0;ai<lookup_angles.size();++ai) {
                double next=ai+1<lookup_angles.size()?lookup_angles[ai+1]:lookup_angles[0]+360;
                insert_local((lookup_angles[ai]+next)/2);
            }
            double coefficient_re=input.res.front()*row.at("chord").get<double>()/grid.at("c_ref_m").get<double>();
            J coefficients=J::parse(segel::polar(J{{"m",row.at("m")},{"p",row.at("p")},
                {"alpha_deg",0.0},{"re",coefficient_re},{"parameter",input.parameters}}.dump()));
            std::vector<PolarBatchInput> queries;
            queries.reserve(localangles.size()*input.res.size());
            for(double a:localangles)for(double re:input.res) {
                ensure(!cancelled,130,"Benutzerabbruch beim Profilexport");
                double localre=re*row.at("chord").get<double>()/grid.at("c_ref_m").get<double>();
                queries.push_back({coefficients.at("a0").get<double>(),coefficients.at("A1").get<double>(),
                        coefficients.at("A2").get<double>(),a,localre});
                if(Clock::now()-last>=std::chrono::seconds(1)) {
                    log.progress("Export",exported,export_total,"Profilpolaren schreiben | Gesamt: 99 % | Phasen: 5/6");last=Clock::now();
                }
            }
            std::vector<PolarBatchOutput> values;
            if(input.gpu) {
                try {
                    values=input.gpu->evaluate(queries,polar_parameters);
                } catch(const std::exception& e) {
                    if(input.gpu_required)throw Error(3,std::string("Erforderliche GPU-Ausfuehrung fehlgeschlagen: ")+e.what());
                    input.gpu_reason=std::string("OpenCL-Ausfuehrung fehlgeschlagen; CPU-Fallback: ")+e.what();
                    log.log("Ressourcen",input.gpu_reason,"WARNUNG");
                    input.gpu.reset();
                    gpu_fallback=true;
                    values=evaluate_polar_cpu(queries,polar_parameters);
                }
                if(input.gpu) {
                    ensure(values.size()==queries.size(),4,"OpenCL lieferte eine unpassende Profilpolar-Zeilenzahl");
                    auto reference=evaluate_polar_cpu(queries,polar_parameters);
                    for(size_t i=0;i<values.size();++i) {
                        const double differences[]={std::abs(values[i].cl-reference[i].cl),
                            std::abs(values[i].cd-reference[i].cd),std::abs(values[i].cm-reference[i].cm)};
                        const double magnitudes[]={std::abs(reference[i].cl),std::abs(reference[i].cd),std::abs(reference[i].cm)};
                        for(size_t k=0;k<3;++k) {
                            ensure(std::isfinite(differences[k])&&differences[k]<=2e-12+2e-12*magnitudes[k],4,
                                "CPU/GPU-Profilpolaren weichen ausserhalb der fp64-Toleranz ab");
                            max_cpu_gpu_error=std::max(max_cpu_gpu_error,differences[k]);
                        }
                    }
                    gpu_used=true;
                    polar_values+=values.size();
                    cpu_gpu_checks+=values.size()*3;
                }
            } else values=evaluate_polar_cpu(queries,polar_parameters);
            ensure(values.size()==queries.size(),4,"Profilpolar-Backend lieferte eine unpassende Zeilenzahl");
            size_t index=0;
            for(double a:localangles)for(double re:input.res) {
                double localre=re*row.at("chord").get<double>()/grid.at("c_ref_m").get<double>();
                const auto& value=values[index++];
                ensure(std::isfinite(value.cl)&&std::isfinite(value.cd)&&std::isfinite(value.cm),4,
                    "Profilpolar-Backend lieferte nicht-endliche Werte");
                profiles<<row.at("profil_id").get<std::string>()<<','<<a<<','<<localre<<','<<value.cl<<','
                        <<value.cd<<','<<value.cm<<','<<quality<<'\n';
            }
            ++exported;
            if(Clock::now()-last>=std::chrono::seconds(1)) {
                log.progress("Export",exported,export_total,"Profilgruppe geschrieben | Gesamt: 99 % | Phasen: 5/6");
                last=Clock::now();
            }
        }
        log.progress("Export",export_total-1,export_total,"Tabellen geschrieben | Gesamt: 99 % | Phasen: 5/6");
        sums.close();forces.close();profiles.close();
        J inputhash={{"auftrag",hash(input.path)},{"geometrie",hash(input.geometrypath)},{"konventionen",hash(input.conventionpath)}};
        ensure(inputhash==input.original_hashes,2,"Eingabedatei waehrend der Berechnung geaendert");
        std::string gpu_backend=gpu_used?(gpu_fallback?"OpenCL+CPU-Fallback":"OpenCL"):"CPU";
        std::string gpu_selection=gpu_used?(gpu_fallback?"gpu_mit_cpu_fallback":"gpu"):
            (gpu_fallback||input.gpu_auto)?"cpu_fallback":"cpu";
        report["gpu"]={{"backend",gpu_backend},{"auswahl",gpu_selection},
            {"grund",input.gpu?"OpenCL-GPU mit fp64 fuer gebuendelte Profilpolaren":input.gpu_reason},
            {"plattform",!gpu_platform.empty()?J(gpu_platform):J(nullptr)},
            {"geraet",!gpu_device.empty()?J(gpu_device):J(nullptr)},
            {"geeignete_funktionen",J::array({"profilpolaren"})},{"ausgefuehrte_polarwerte",polar_values},
            {"cpu_gpu_vergleiche",cpu_gpu_checks},{"max_cpu_gpu_abweichung",max_cpu_gpu_error},{"speedbenchmark",false}};
        write(stage/"auftrag_ausgefuehrt.json",{{"originalauftrag",input.job},{"geometrie",input.geom},{"konventionen",input.conv},
            {"effektiv",{{"cpu_limit",input.budget},{"cli_limit",limit},{"backend",report["gpu"].at("backend")},
                {"gpu_plattform",!gpu_platform.empty()?J(gpu_platform):J(nullptr)},
                {"gpu_geraet",!gpu_device.empty()?J(gpu_device):J(nullptr)},
                {"modellstand",model},{"toolversion","0.1.0"},
                {"interpolation_stuetzstellen",report.at("interpolation_stuetzstellen")}}},{"sha256",inputhash}});
        report["status"]="erfolgreich";report["summenzeilen"]=count;
        report["cpu_peak"]=input.peak.load();
        report["dauer_s"]=std::chrono::duration<double>(Clock::now()-log.start).count();
        write(stage/"pruefbericht.json",report);
        J hashes;
        for(const auto& name:{"summen.csv","kraefte.csv","profile.csv","pruefbericht.json","auftrag_ausgefuehrt.json"})hashes[name]=hash(stage/name);
        write(stage/"lookup.json",{{"formatversion","1"},{"status","erfolgreich"},{"qualitaet",quality},{"modellstand",model},
            {"parameter",input.parameters},{"segel_id",grid.at("segel_id")},{"geometrieschema","0.1"},{"geometrie",grid},
            {"konventionen",input.conv},{"formvarianten",input.job.at("formvarianten")},{"betriebspunkte",input.job.at("betriebspunkte")},
            {"interpolation",{{"verfahren","periodische_tensorprodukt_cubic_hermite"},
                {"stuetzstellen",report.at("interpolation_stuetzstellen")}}},
            {"summenzeilen",count},{"csv",{{"praezision",17},{"encoding","UTF-8"},{"trenner",","},{"summen.csv",sumheader},
                {"kraefte.csv",forceheader},{"profile.csv",profileheader},
                {"einheiten",{{"alpha_deg","deg"},{"windneigung_deg","deg"},{"re_ref","1"},{"re","1"},
                    {"CF_und_CM","1"},{"CL_CD_CM_profil","1"},{"angriff_und_ansatz","m"},{"IDs_Qualitaet","Text"}}}}},
            {"bezuege",{{"S_m2",grid.at("S_m2")},{"c_ref_m",grid.at("c_ref_m")},{"kraftschwelle",1e-10},{"momentenursprung","hals"}}},
            {"lastmodell",{{"gebunden","Viertelsehne plus freies Fourier-Profilmoment"},{"abgeloest","4 Mittelflaechen-Impulsbeitraege pro Streifen"}}},
            {"grenzen",report.at("warnungen")},{"interferenz","nicht_berechnet"},{"eingaben_sha256",inputhash},{"dateien_sha256",hashes},
            {"pruefbericht","pruefbericht.json"}});
        ensure(!cancelled,130,"Benutzerabbruch vor Veroeffentlichung");
        ensure(!fs::exists(output),5,"LookUp zwischenzeitlich angelegt");
        fs::rename(stage,output,ec);ensure(!ec,5,"Veroeffentlichung fehlgeschlagen: "+ec.message());
        log.log("Export","Abgeschlossen | Gesamt: 100 % | Phasen: 6/6 | "+output.u8string());
        return success(output,quality);
    } catch(const std::exception& e) {
        int code=4;if(auto err=dynamic_cast<const Error*>(&e))code=err->code;
        if(dynamic_cast<const std::ios_base::failure*>(&e))code=5;
        if(dynamic_cast<const fs::filesystem_error*>(&e))code=5;
        if(dynamic_cast<const std::bad_alloc*>(&e))code=3;
        report["status"]="fehlgeschlagen";report["fehler"]=e.what();report["exitcode"]=code;
        report["cpu_peak"]=input.peak.load();
        fs::path diag=parent/fs::u8path("LookUp.Diagnose."+token);
        try {write(stage/"pruefbericht.json",report);fs::rename(stage,diag);} catch(...) {}
        log.log("Abschluss",std::string(e.what())+" | Diagnose: "+diag.u8string(),"FEHLER");
        J failure={{"status","fehlgeschlagen"},{"exitcode",code},{"ergebnisordner",nullptr},
            {"pruefbericht",fs::exists(diag/"pruefbericht.json")?J((diag/"pruefbericht.json").u8string()):J(nullptr)},{"meldung",e.what()}};
        return failure;
    }
}
J dispatch(const std::vector<std::string>& args) {
    try {
        if(args.size()==1&&(args[0]=="--hilfe"||args[0]=="--version"))
            return success({},args[0]=="--version"?"SegelMacher 0.1.0, H1":
                "SegelMacher lookup|pruefen --auftrag <UTF-8-Pfad> [--max-prozessorzahl N]; "
                "OpenCL-FP64-GPU fuer Profilpolaren, CPU-Fallback bei gpu=automatisch");
        ensure(!args.empty()&&(args[0]=="lookup"||args[0]=="pruefen"),2,"Befehl lookup oder pruefen erforderlich; --hilfe");
        fs::path path;int limit=0;bool seenlimit=false;
        for(size_t i=1;i<args.size();++i) {
            auto option=args[i];
            ensure((option=="--auftrag"||option=="--max-prozessorzahl")&&i+1<args.size(),2,"Ungueltige oder unvollstaendige Option: "+option);
            auto value=args[++i];
            if(option=="--auftrag") {ensure(path.empty(),2,"--auftrag doppelt");path=fs::u8path(value);}
            else {
                ensure(!seenlimit,2,"--max-prozessorzahl doppelt");seenlimit=true;
                ensure(!value.empty()&&value.find_first_not_of("0123456789")==std::string::npos,2,"--max-prozessorzahl positive Ganzzahl erforderlich");
                try {limit=std::stoi(value);}catch(...){throw Error(2,"--max-prozessorzahl ausserhalb Wertebereich");}
                ensure(limit>0,2,"--max-prozessorzahl muss positiv sein");
            }
        }
        ensure(!path.empty(),2,"--auftrag fehlt");return run(path,args[0]=="pruefen",limit);
    } catch(const std::exception& e) {
        int code=4;
        if(dynamic_cast<const J::exception*>(&e)||dynamic_cast<const std::invalid_argument*>(&e))code=2;
        if(dynamic_cast<const std::bad_alloc*>(&e))code=3;
        if(dynamic_cast<const fs::filesystem_error*>(&e))code=5;
        if(auto err=dynamic_cast<const Error*>(&e))code=err->code;
        Logger{}.log("Abschluss",e.what(),"FEHLER");
        return {{"status","fehlgeschlagen"},{"exitcode",code},{"ergebnisordner",nullptr},{"pruefbericht",nullptr},{"meldung",e.what()}};
    }
}
}
