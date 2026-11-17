#include "job.hpp"
#include "parquet_internal.hpp"
#include "segel/core.hpp"
#include "segel/modell.hpp"
#include "../Core/src/segelform_internal.hpp"
#include "../Core/src/segelform_test_winds.hpp"
#include "../Core/src/segelform_uniform_check.hpp"
#include "../Core/src/segelform_load_compare.hpp"
#include "../Core/src/segelform_edge_check.hpp"
#include "../Core/src/segelform_re_check.hpp"
#include "opencl_backend.hpp"
#include "progress.hpp"
#include <segelphysik/modell_fingerprints.hpp>
#include <arrow/api.h>
#include <arrow/config.h>
#include <arrow/io/api.h>
#include <parquet/arrow/writer.h>
#include <parquet/properties.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <exception>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <string_view>
#include <system_error>
#include <thread>
#include <tuple>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <psapi.h>
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
using detail::ensure;
using detail::fields;
using detail::numeric;
using detail::write;
using detail::number_scalar;
using detail::parquet_row_group_rows;
using detail::ParquetTableWriter;
std::shared_ptr<arrow::Schema> sums_schema() {
    return arrow::schema({
        arrow::field("form_id",arrow::utf8(),false),arrow::field("alpha_deg",arrow::float64(),false),
        arrow::field("windneigung_deg",arrow::float64(),false),arrow::field("re_ref",arrow::float64(),false),
        arrow::field("CFx",arrow::float64(),false),arrow::field("CFy",arrow::float64(),false),
        arrow::field("CFz",arrow::float64(),false),arrow::field("CMx",arrow::float64(),false),
        arrow::field("CMy",arrow::float64(),false),arrow::field("CMz",arrow::float64(),false),
        arrow::field("angriff_x_m",arrow::float64()),arrow::field("angriff_y_m",arrow::float64()),
        arrow::field("angriff_z_m",arrow::float64()),arrow::field("angriff_gueltig",arrow::boolean(),false),
        arrow::field("CM_rest_x",arrow::float64(),false),arrow::field("CM_rest_y",arrow::float64(),false),
        arrow::field("CM_rest_z",arrow::float64(),false),arrow::field("qualitaet",arrow::utf8(),false)});
}
std::shared_ptr<arrow::Schema> forces_schema() {
    return arrow::schema({
        arrow::field("form_id",arrow::utf8(),false),arrow::field("alpha_deg",arrow::float64(),false),
        arrow::field("windneigung_deg",arrow::float64(),false),arrow::field("re_ref",arrow::float64(),false),
        arrow::field("beitrag_id",arrow::int64(),false),arrow::field("CFx",arrow::float64(),false),
        arrow::field("CFy",arrow::float64(),false),arrow::field("CFz",arrow::float64(),false),
        arrow::field("ansatz_x_m",arrow::float64(),false),arrow::field("ansatz_y_m",arrow::float64(),false),
        arrow::field("ansatz_z_m",arrow::float64(),false),arrow::field("CM_eigen_x",arrow::float64(),false),
        arrow::field("CM_eigen_y",arrow::float64(),false),arrow::field("CM_eigen_z",arrow::float64(),false),
        arrow::field("qualitaet",arrow::utf8(),false)});
}
std::shared_ptr<arrow::Schema> profiles_schema() {
    return arrow::schema({
        arrow::field("profil_id",arrow::utf8(),false),arrow::field("alpha_deg",arrow::float64(),false),
        arrow::field("re",arrow::float64(),false),arrow::field("CL",arrow::float64(),false),
        arrow::field("CD",arrow::float64(),false),arrow::field("CM",arrow::float64(),false),
        arrow::field("qualitaet",arrow::utf8(),false)});
}
std::shared_ptr<arrow::Scalar> text_scalar(const std::string& value) {
    return std::make_shared<arrow::StringScalar>(value);
}
std::shared_ptr<arrow::Scalar> integer_scalar(int64_t value) {
    return std::make_shared<arrow::Int64Scalar>(value);
}
std::shared_ptr<arrow::Scalar> boolean_scalar(bool value) {
    return std::make_shared<arrow::BooleanScalar>(value);
}
std::shared_ptr<arrow::Scalar> null_number_scalar() {
    return std::make_shared<arrow::DoubleScalar>();
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
        ensure(elevations==std::vector<double>({-90,-80,-70,-60,-50,-40,-30,-20,-10,0,10,20,30,40,50,60,70,80,90}),
               2,"windneigung_deg muss [-90,-80,...,0,...,80,90] sein; alte Auftraege mit [0,10,...,90] muessen umgestellt werden");
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
J publish_model_result(const fs::path& stage,const fs::path& output,bool overwrite,
                       const std::function<void(J&)>& after_publish) {
    bool published=false;
    try {
        detail::ensure(!cancelled,130,"Benutzerabbruch vor Segelmodell-Veroeffentlichung");
        const bool replacing=publish_model_stage(stage,output,overwrite,published);
        auto result=success(output,"Segelmodell erzeugt");
        result["pruefbericht"]=nullptr;
        result["output_action"]=replacing?"overwritten":"created";
        if(after_publish)after_publish(result);
        return result;
    } catch(const std::exception& e) {
        if(!published)throw;
        int code=4;
        if(dynamic_cast<const J::exception*>(&e)||dynamic_cast<const std::invalid_argument*>(&e))code=2;
        if(dynamic_cast<const std::bad_alloc*>(&e))code=3;
        if(dynamic_cast<const fs::filesystem_error*>(&e)||
           dynamic_cast<const std::ios_base::failure*>(&e))code=5;
        if(auto error=dynamic_cast<const Error*>(&e))code=error->code==6?3:error->code;
        return {{"status","fehlgeschlagen"},{"exitcode",code},{"ergebnisordner",output.u8string()},
                {"pruefbericht",nullptr},{"output_action","written"},{"meldung",e.what()}};
    }
}
void update_model_fingerprint(J& result,const fs::path& model_root) {
    try {
        const auto updated=segelphysik::aktualisiere_modell_fingerprints(
            model_root,{segelphysik::ModellFingerprintBereich::aerodynamics});
        const auto& value=updated.werte.at(0);
        result["fingerprint"]={{"bereich","aerodynamics"},{"hash",value.hash},
            {"modified_at",value.modified_at},{"updated",updated.validierungsdatei_aktualisiert}};
    } catch(const std::exception& e) {
        throw Error(5,std::string("Core-Fingerprint fehlgeschlagen: ")+e.what());
    }
}
#ifdef _WIN32
namespace {
fs::path loaded_module_path(HMODULE module) {
    std::vector<wchar_t> path(32768);
    auto length=GetModuleFileNameW(module,path.data(),static_cast<DWORD>(path.size()));
    ensure(length>0&&length<path.size(),3,"Pfad der geladenen Bibliothek kann nicht ermittelt werden");
    return fs::path(std::wstring(path.data(),length));
}
size_t image_readable_bytes(const unsigned char* base,size_t size,size_t offset,size_t limit) {
    if(offset>=size||!limit)return 0;
    MEMORY_BASIC_INFORMATION info{};
    const auto* address=base+offset;
    if(!VirtualQuery(address,&info,sizeof(info))||info.State!=MEM_COMMIT||
       (info.Protect&PAGE_GUARD)||(info.Protect&0xff)==PAGE_NOACCESS)return 0;
    const DWORD protection=info.Protect&0xff;
    if(protection!=PAGE_READONLY&&protection!=PAGE_READWRITE&&protection!=PAGE_WRITECOPY&&
       protection!=PAGE_EXECUTE_READ&&protection!=PAGE_EXECUTE_READWRITE&&
       protection!=PAGE_EXECUTE_WRITECOPY)return 0;
    const auto region=reinterpret_cast<std::uintptr_t>(info.BaseAddress);
    const auto current=reinterpret_cast<std::uintptr_t>(address);
    if(current<region||current-region>=info.RegionSize)return 0;
    return std::min({limit,size-offset,info.RegionSize-(current-region)});
}
template<class T>
bool image_read(const unsigned char* base,size_t size,size_t offset,T& value) {
    if(offset>size||sizeof(T)>size-offset)return false;
    auto* out=reinterpret_cast<unsigned char*>(&value);
    for(size_t copied=0;copied<sizeof(T);) {
        size_t count=image_readable_bytes(base,size,offset+copied,sizeof(T)-copied);
        if(!count)return false;
        std::memcpy(out+copied,base+offset+copied,count);
        copied+=count;
    }
    return true;
}
bool image_string(const unsigned char* base,size_t size,size_t offset,std::string& value) {
    constexpr size_t max_length=4096;
    while(value.size()<max_length) {
        const size_t count=image_readable_bytes(base,size,offset,max_length-value.size());
        if(!count)return false;
        const auto* begin=reinterpret_cast<const char*>(base+offset);
        const auto* end=static_cast<const char*>(std::memchr(begin,0,count));
        value.append(begin,end?static_cast<size_t>(end-begin):count);
        if(end)return true;
        offset+=count;
    }
    return false;
}
template<size_t N>
HMODULE imported_module(HMODULE importer,const std::array<const char*,N>& symbols,bool prefix) {
    MODULEINFO info{};
    if(!K32GetModuleInformation(GetCurrentProcess(),importer,&info,sizeof(info)))return nullptr;
    const auto* base=static_cast<const unsigned char*>(info.lpBaseOfDll);
    const size_t size=info.SizeOfImage;
    IMAGE_DOS_HEADER dos{};
    if(!image_read(base,size,0,dos)||dos.e_magic!=IMAGE_DOS_SIGNATURE||dos.e_lfanew<0)return nullptr;
    IMAGE_NT_HEADERS nt{};
    if(!image_read(base,size,static_cast<size_t>(dos.e_lfanew),nt)||
       nt.Signature!=IMAGE_NT_SIGNATURE||nt.OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR_MAGIC||
       nt.OptionalHeader.NumberOfRvaAndSizes<=IMAGE_DIRECTORY_ENTRY_IMPORT)return nullptr;
    const auto directory=nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if(directory.VirtualAddress>size||directory.Size>size-directory.VirtualAddress)return nullptr;
    HMODULE found=nullptr;
    std::array<bool,N> matched{};
    bool terminated_descriptors=false;
    const size_t count=directory.Size/sizeof(IMAGE_IMPORT_DESCRIPTOR);
    for(size_t i=0;i<count;++i) {
        IMAGE_IMPORT_DESCRIPTOR descriptor{};
        if(!image_read(base,size,directory.VirtualAddress+i*sizeof(descriptor),descriptor))return nullptr;
        if(!descriptor.Name&&!descriptor.FirstThunk) {terminated_descriptors=true;break;}
        if(!descriptor.OriginalFirstThunk||!descriptor.FirstThunk)continue;
        const size_t names=descriptor.OriginalFirstThunk,addresses=descriptor.FirstThunk;
        if(names>size||addresses>size)return nullptr;
        const size_t thunk_count=std::min((size-names)/sizeof(IMAGE_THUNK_DATA),
                                          (size-addresses)/sizeof(IMAGE_THUNK_DATA));
        bool terminated=false;
        for(size_t j=0;j<thunk_count;++j) {
            IMAGE_THUNK_DATA name{},address{};
            if(!image_read(base,size,names+j*sizeof(name),name)||
               !image_read(base,size,addresses+j*sizeof(address),address))return nullptr;
            if(!name.u1.AddressOfData) {terminated=true;break;}
            if(IMAGE_SNAP_BY_ORDINAL(name.u1.Ordinal))continue;
            const size_t name_offset=static_cast<size_t>(name.u1.AddressOfData);
            if(name_offset>size||sizeof(WORD)>size-name_offset)return nullptr;
            const size_t text=name_offset+sizeof(WORD);
            std::string symbol;
            if(!image_string(base,size,text,symbol))return nullptr;
            const std::string_view actual_name(symbol);
            size_t index=0;
            while(index<N) {
                std::string_view expected= symbols[index];
                if(prefix?actual_name.substr(0,expected.size())==expected:actual_name==expected)break;
                ++index;
            }
            if(index==N)continue;
            HMODULE module=nullptr;
            if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                   reinterpret_cast<LPCWSTR>(address.u1.Function),&module))return nullptr;
            auto imported=reinterpret_cast<FARPROC>(address.u1.Function);
            if(GetProcAddress(module,symbol.c_str())!=imported||(found&&found!=module))return nullptr;
            found=module;
            matched[index]=true;
        }
        if(!terminated)return nullptr;
    }
    return terminated_descriptors&&std::all_of(matched.begin(),matched.end(),[](bool value){return value;})?
        found:nullptr;
}
}
#endif
J parquet_library_provenance(const std::string& package_version,const std::string& package_sha256) {
    if(package_version.empty()||package_sha256.empty())
        return {{"status","nicht_nachgewiesen"},{"version",nullptr},{"grund","keine_paketidentitaet"}};
#ifdef _WIN32
    HMODULE module=nullptr;
    ensure(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
           reinterpret_cast<LPCWSTR>(&parquet::arrow::FileWriter::Open),&module)!=0,3,
           "Geladene Parquet-Bibliothek kann nicht ermittelt werden");
    auto actual=hash(loaded_module_path(module));
    if(actual!=package_sha256)
        return {{"status","nicht_nachgewiesen"},{"version",nullptr},
                {"grund","paketdatei_stimmt_nicht_ueberein"},{"sha256",actual}};
    return {{"status","nachgewiesen"},{"version",package_version},
            {"grundlage","paketmetadaten_und_laufzeitdatei_sha256"},{"sha256",actual}};
#else
    return {{"status","nicht_nachgewiesen"},{"version",nullptr},{"grund","plattform_nicht_unterstuetzt"}};
#endif
}
J zstd_library_provenance(const std::string& arrow_package_sha256,
                          const std::string& parquet_package_version,const std::string& parquet_package_sha256) {
    if(arrow_package_sha256.empty()||parquet_package_version.empty()||parquet_package_sha256.empty())
        return {{"status","nicht_nachgewiesen"},{"version",nullptr},{"grund","keine_paketidentitaet"}};
#ifdef _WIN32
    HMODULE arrow_module=nullptr;
    ensure(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
           reinterpret_cast<LPCWSTR>(&arrow::GetBuildInfo),&arrow_module)!=0,3,
           "Geladene Arrow-Bibliothek kann nicht ermittelt werden");
    if(arrow::GetBuildInfo().version_string!="21.0.0"||hash(loaded_module_path(arrow_module))!=arrow_package_sha256)
        return {{"status","nicht_nachgewiesen"},{"version",nullptr},{"grund","arrow_paket_stimmt_nicht_ueberein"}};
    HMODULE parquet_module=nullptr;
    ensure(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
           reinterpret_cast<LPCWSTR>(&parquet::arrow::FileWriter::Open),&parquet_module)!=0,3,
           "Geladener Parquet-Writer kann nicht ermittelt werden");
    if(parquet_package_version!="21.0.0"||hash(loaded_module_path(parquet_module))!=parquet_package_sha256)
        return {{"status","nicht_nachgewiesen"},{"version",nullptr},{"grund","parquet_paket_stimmt_nicht_ueberein"}};
    // Arrow/Parquet 21 routes writer compression through Codec::Create and these Zstd imports.
    if(imported_module(parquet_module,std::array<const char*,1>{"?Create@Codec@util@arrow@@"},true)!=arrow_module)
        return {{"status","nicht_nachgewiesen"},{"version",nullptr},{"grund","parquet_arrow_codec_import_nicht_nachgewiesen"}};
    HMODULE module=imported_module(arrow_module,std::array<const char*,2>{"ZSTD_compress","ZSTD_compressStream"},false);
    if(!module)
        return {{"status","nicht_nachgewiesen"},{"version",nullptr},{"grund","arrow_zstd_import_nicht_nachgewiesen"}};
    using VersionFunction=const char* (__cdecl*)();
    auto version_function=reinterpret_cast<VersionFunction>(GetProcAddress(module,"ZSTD_versionString"));
    if(!version_function)
        return {{"status","nicht_nachgewiesen"},{"version",nullptr},{"grund","zstd_versionsfunktion_fehlt"}};
    const char* version=version_function();
    if(!version||!*version)
        return {{"status","nicht_nachgewiesen"},{"version",nullptr},{"grund","zstd_version_fehlt"}};
    return {{"status","nachgewiesen"},{"version",version},
            {"grundlage","parquet_arrow_21_paketidentitaet_und_zstd_import"},{"sha256",hash(loaded_module_path(module))}};
#else
    return {{"status","nicht_nachgewiesen"},{"version",nullptr},{"grund","plattform_nicht_unterstuetzt"}};
#endif
}
namespace {
fs::path fingerprint_model(const fs::path& parent) {
    for(auto current=fs::weakly_canonical(parent);!current.empty();) {
        if(current.filename()=="aerodynamics"&&current.parent_path().filename()=="modells") {
            auto model_root=current.parent_path().parent_path();
            ensure(fs::is_regular_file(model_root/"modells_data.json")&&
                   fs::is_regular_file(model_root/"modells_validation.json"),2,
                   "Fingerprint erfordert einen Modellordner mit modells_data.json und modells_validation.json");
            return model_root;
        }
        auto next=current.parent_path();
        if(next==current)break;
        current=next;
    }
    throw Error(2,"--fingerprint update erfordert einen Auftrag unter modells/aerodynamics");
}
void update_fingerprint(J& result,const fs::path& model_root,const Logger& log) {
    update_model_fingerprint(result,model_root);
    log.log("Fingerprint","Core-Aerodynamik-Fingerprint geprueft/aktualisiert: "+
            result.at("fingerprint").at("hash").get<std::string>());
}
using ModelVector=std::array<double,3>;
ModelVector model_cross(const ModelVector& a,const ModelVector& b) {
    return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};
}
double model_norm(const ModelVector& value) {
    return std::hypot(std::hypot(value[0],value[1]),value[2]);
}
ModelVector model_point(const J& strip) {
    return {strip.at("abfrage_x_m"),strip.at("abfrage_y_m"),strip.at("abfrage_z_m")};
}
J gradient_winds(const J& model_data,int type,bool reverse,double& vmax) {
    constexpr double pi=3.14159265358979323846;
    double base=model_data.at("z_min_m"),height=model_data.at("hoehe_m");
    J winds=J::array();
    for(const auto& strip:model_data.at("streifen")) {
        double t=(strip.at("abfrage_z_m").get<double>()-base)/height;
        if(reverse)t=1-t;
        double speed=type==1?10:1+9*t;
        double angle=type==0?0:(-45+90*t)*pi/180;
        ModelVector u{-speed*std::cos(angle),-speed*std::sin(angle),0};
        vmax=std::max(vmax,model_norm(u));
        winds.push_back({{"streifen_id",strip.at("streifen_id")},{"wind_m_s",u}});
    }
    return winds;
}
J check_model_gradients(const J& coarse,const J& fine) {
    const std::array<const char*,7> names={"geschwindigkeit","richtung","kombiniert","lokaler_nullwind",
                                           "gegenlaeufig","azimut_naht","lokal_vertikal"};
    const std::array<segel::detail::EdgeWindCase,3> edge_types={
        segel::detail::EdgeWindCase::counterflow,segel::detail::EdgeWindCase::azimuth_seam,
        segel::detail::EdgeWindCase::local_vertical};
    segel::detail::PreparedModel coarse_solver(coarse.dump()),fine_solver(fine.dump());
    const auto& coarse_points=coarse.at("streifen");
    const auto& fine_points=fine.at("streifen");
    const double cref=coarse.at("c_ref_m");
    J cases=J::array();
    for(int type=0;type<7;++type)for(bool reverse:{false,true}) {
        ensure(!cancelled,130,"Benutzerabbruch bei der Segelmodell-Rasterpruefung");
        double vmax=0;
        J coarse_winds=type>=4?segel::detail::edge_winds(coarse,coarse,edge_types[type-4],reverse,vmax):
            (type==3?segel::detail::local_zero_winds(coarse,coarse,reverse,vmax):gradient_winds(coarse,type,reverse,vmax));
        J fine_winds=type>=4?segel::detail::edge_winds(coarse,fine,edge_types[type-4],reverse,vmax):
            (type==3?segel::detail::local_zero_winds(coarse,fine,reverse,vmax):gradient_winds(fine,type,reverse,vmax));
        auto coarse_detailed=J::parse(coarse_solver.solve_with_contributions(coarse_winds.dump(),1.225,1.5e-5));
        auto fine_detailed=J::parse(fine_solver.solve_with_contributions(fine_winds.dump(),1.225,1.5e-5));
        const auto& coarse_loads=coarse_detailed.at("streifen");
        const auto& fine_loads=fine_detailed.at("streifen");
        double qmax=0.5*1.225*vmax*vmax;
        J balances={{"grob",segel::detail::compare_contribution_balance(coarse,coarse_detailed,qmax)},
                    {"halbiert",segel::detail::compare_contribution_balance(fine,fine_detailed,qmax)}};
        ModelVector coarse_total{},fine_total{},coarse_moment{},fine_moment{};
        J worst,comparisons=J::array();
        double worst_ratio=-1;
        auto observe=[&](const ModelVector& cf,const ModelVector& ff,
                         const ModelVector& cm,const ModelVector& fm,double area,const J& id) {
            ModelVector df{},dm{};
            for(int k=0;k<3;++k) {df[k]=ff[k]-cf[k];dm[k]=fm[k]-cm[k];}
            double force_limit=0.05*qmax*area+1e-9,moment_limit=0.05*qmax*area*cref+1e-9;
            double force_error=model_norm(df),moment_error=model_norm(dm);
            ensure(std::isfinite(force_limit)&&force_limit>0&&std::isfinite(moment_limit)&&moment_limit>0&&
                   std::isfinite(force_error)&&std::isfinite(moment_error),4,
                   "Rastervergleich: Abweichung oder Grenze nicht darstellbar");
            for(const auto& item:std::array<std::tuple<const char*,double,double,
                    const ModelVector*,const ModelVector*>,2>{{
                    {"kraft_N",force_error,force_limit,&cf,&ff},{"moment_Nm",moment_error,moment_limit,&cm,&fm}}}) {
                double ratio=std::get<1>(item)/std::get<2>(item);
                ensure(std::isfinite(ratio),4,"Rastervergleich: Grenzquotient nicht darstellbar");
                J comparison={{"streifen_id",id},{"groesse",std::get<0>(item)},
                              {"abweichung",std::get<1>(item)},{"grenze",std::get<2>(item)},
                              {"flaeche_m2",area},{"quotient",ratio},
                              {"wert_grob",*std::get<3>(item)},{"wert_halbiert",*std::get<4>(item)}};
                comparisons.push_back(comparison);
                if(ratio>worst_ratio) {
                    worst_ratio=ratio;
                    worst=std::move(comparison);
                }
            }
        };
        for(size_t id=0;id<coarse_loads.size();++id) {
            ModelVector cf=coarse_loads[id].at("kraft_N"),cm=coarse_loads[id].at("moment_Nm");
            auto cp=model_point(coarse_points[id]);
            ModelVector ff{},fm{};
            for(size_t j=2*id;j<2*id+2;++j) {
                ModelVector f=fine_loads[j].at("kraft_N"),m=fine_loads[j].at("moment_Nm");
                auto fp=model_point(fine_points[j]);
                ModelVector arm{fp[0]-cp[0],fp[1]-cp[1],fp[2]-cp[2]};
                auto shifted=model_cross(arm,f);
                for(int k=0;k<3;++k) {ff[k]+=f[k];fm[k]+=m[k]+shifted[k];}
            }
            auto coarse_arm=model_cross(cp,cf),fine_arm=model_cross(cp,ff);
            for(int k=0;k<3;++k) {
                coarse_total[k]+=cf[k];fine_total[k]+=ff[k];
                coarse_moment[k]+=cm[k]+coarse_arm[k];fine_moment[k]+=fm[k]+fine_arm[k];
            }
            double area=coarse_points[id].at("flaeche_m2");
            observe(cf,ff,cm,fm,area,id);
        }
        double area=coarse.at("S_m2");
        observe(coarse_total,fine_total,coarse_moment,fine_moment,area,nullptr);
        J modes=J::array();
        for(bool refined:{false,true}) {
            const auto& geometry=refined?fine:coarse;
            const auto& reference=refined?fine_loads:coarse_loads;
            const auto& winds=refined?fine_winds:coarse_winds;
            const auto& solver=refined?fine_solver:coarse_solver;
            for(int count:{32,64}) {
                ensure(!cancelled,130,"Benutzerabbruch beim Segelmodell-Modenvergleich");
                auto candidate=J::parse(solver.solve_modes(winds.dump(),1.225,1.5e-5,count));
                modes.push_back({{"moden",count},{"referenzmoden",48},{"halbiertes_raster",refined},
                                 {"schlechtester_vergleich",
                                  segel::detail::compare_same_grid_loads(geometry,reference,candidate,qmax)}});
            }
        }
        cases.push_back({{"windfeld",names[type]},{"umgekehrt",reverse},{"dichte_kg_m3",1.225},
                         {"viskositaet_m2_s",1.5e-5},{"q_max_Pa",qmax},{"c_ref_m",cref},
                         {"wind_grob",std::move(coarse_winds)},{"wind_halbiert",std::move(fine_winds)},
                         {"status",worst_ratio<=1?"bestanden":"fehlgeschlagen"},{"schlechtester_vergleich",worst},
                         {"vergleiche",std::move(comparisons)},
                         {"modenvergleiche_diagnostisch",modes},{"beitragsbilanz_diagnostisch",balances}});
    }
    return cases;
}
const std::array<double,23> model_uniform_speeds={0,0.0005,0.001,0.0055,0.01,0.055,0.1,0.3,0.5,
                                                  0.75,1,3,5,7.5,10,15,20,25,30,35,40,45,50};
J check_model_uniforms(const J& source,const J& model_data,const Logger& log,int budget,int& peak_workers) {
    constexpr double rho=1.225,nu=1.5e-5;
    const auto& speeds=model_uniform_speeds;
    const std::array<double,9> betas={-90,-80,-60,-30,0,30,60,80,90};
    auto refined_source=source;
    auto& grid=refined_source["numerik"]["hoehenraster"];
    grid["schritt_m"]=grid["schritt_m"].get<double>()/2;
    grid["verfeinerter_schritt_m"]=grid["verfeinerter_schritt_m"].get<double>()/2;
    const std::string source_text=source.dump(),refined_text=refined_source.dump(),model_text=model_data.dump();
    const J direct_grid=J::parse(segel::Context(source_text).geometry());
    const double cref=model_data.at("c_ref_m");
    std::atomic<size_t> next{0},finished{0};
    std::atomic<bool> failed{false};
    std::atomic<int> active{0},peak{0};
    std::mutex error_mutex,progress_mutex;
    std::condition_variable progress;
    std::exception_ptr failure;
    std::vector<J> results(speeds.size());
    auto calculate=[&](size_t si,const segel::detail::PreparedModel& solver,
                       const segel::detail::SplitReference& reference,
                       const segel::detail::SplitReference& finer,const segel::Context& direct) {
        double speed=speeds[si];
        std::map<double,bool> validity;
        const auto groups=segel::detail::uniform_transition_groups(model_data,speed,nu);
        std::set<std::pair<double,double>> angles;
        for(const auto& group:groups) {
            angles.insert(group.center);
            angles.insert(group.sides.begin(),group.sides.end());
        }
        const size_t transition_count=angles.size();
        const auto transition_angles=angles;
        std::map<std::pair<double,double>,std::pair<double,segel::detail::CompactLoads>> transition_loads;
        for(double beta:betas)for(int alpha=-180;alpha<180;alpha+=30)
            angles.emplace(static_cast<double>(alpha),beta);
        double worst=-1;
        double q_max=0;
        size_t h1_cases=0;
        size_t pole_comparisons=0;
        std::array<J,2> pole_loads;
        J diagnostic=nullptr,failures=J::array();
        for(const auto& [alpha,beta]:angles) {
            ensure(!cancelled,130,"Benutzerabbruch bei der Segelmodell-Gleichwindpruefung");
            ensure(!failed,4,"Segelmodell-Gleichwindpruefung nach Workerfehler beendet");
            ModelVector wind=segel::detail::uniform_wind(speed,alpha,beta);
            double actual_speed=model_norm(wind),re=actual_speed*cref/nu;
            q_max=std::max(q_max,0.5*rho*actual_speed*actual_speed);
            auto valid=validity.find(actual_speed);
            if(valid==validity.end())
                valid=validity.emplace(actual_speed,
                    segel::detail::uniform_h1_valid(actual_speed,nu,model_data,direct_grid)&&
                    reference.valid(re)&&finer.valid(re)).first;
            J winds=J::array();
            for(const auto& strip:model_data.at("streifen"))
                winds.push_back({{"streifen_id",strip.at("streifen_id")},{"wind_m_s",wind}});
            J loads=J::parse(solver.solve(winds.dump(),rho,nu));
            if(std::abs(beta)==90) {
                auto& pole_reference=pole_loads[beta<0?0:1];
                if(pole_reference.is_null())pole_reference=loads;
                else {
                    ensure(pole_reference==loads,4,"Gleichwindpruefung: Polwerte unterscheiden sich bei Geschwindigkeit "+
                           std::to_string(speed)+", Azimut "+std::to_string(alpha)+
                           ", Neigung "+std::to_string(beta));
                    ++pole_comparisons;
                }
            }
            if(transition_angles.count({alpha,beta})) {
                segel::detail::CompactLoads compact;
                compact.reserve(loads.size());
                for(const auto& load:loads) {
                    auto force=load.at("kraft_N").get<ModelVector>();
                    auto moment=load.at("moment_Nm").get<ModelVector>();
                    compact.push_back({force[0],force[1],force[2],moment[0],moment[1],moment[2]});
                }
                transition_loads.emplace(std::make_pair(alpha,beta),
                                         std::make_pair(0.5*rho*actual_speed*actual_speed,std::move(compact)));
            }
            if(speed==0)
                for(const auto& load:loads)
                    ensure(model_norm(load.at("kraft_N").get<ModelVector>())==0&&
                           model_norm(load.at("moment_Nm").get<ModelVector>())==0,
                           4,"Gleichwindpruefung: Last bei globalem Nullwind nicht null");
            if(!valid->second)continue;
            ++h1_cases;
            J comparison=segel::detail::compare_uniform_h1(model_data,loads,
                reference.solve_data(alpha,beta,re),finer.solve_data(alpha,beta,re),
                J::parse(direct.solve(alpha,beta,re,4,false,48)),actual_speed,rho);
            double ratio=comparison.at("quotient");
            comparison["azimut_deg"]=alpha;comparison["neigung_deg"]=beta;
            comparison["q_max_Pa"]=0.5*rho*actual_speed*actual_speed;
            if(ratio>1)failures.push_back(comparison);
            if(ratio>worst) {
                worst=ratio;diagnostic=std::move(comparison);
            }
        }
        double transition_worst=-1;
        size_t transition_comparisons=0;
        J transition_diagnostic=nullptr;
        for(const auto& group:groups) {
            const auto& center=transition_loads.at(group.center);
            for(const auto& side_key:group.sides) {
                ensure(!cancelled,130,"Benutzerabbruch beim Segelmodell-Uebergangsvergleich");
                ensure(!failed,4,"Segelmodell-Uebergangsvergleich nach Workerfehler beendet");
                const auto& side=transition_loads.at(side_key);
                double q=std::max(center.first,side.first);
                auto comparison=segel::detail::compare_same_grid_loads(model_data,center.second,side.second,q);
                comparison["referenz_azimut_deg"]=group.center.first;
                comparison["referenz_neigung_deg"]=group.center.second;
                comparison["azimut_deg"]=side_key.first;
                comparison["neigung_deg"]=side_key.second;
                comparison["q_max_Pa"]=q;
                if(comparison.at("quotient").get<double>()>transition_worst) {
                    transition_worst=comparison.at("quotient");
                    transition_diagnostic=std::move(comparison);
                }
                ++transition_comparisons;
            }
        }
        const char* status=!failures.empty()?"fehlgeschlagen":
            (h1_cases==0?"h1_nicht_anwendbar":(h1_cases<angles.size()?"h1_teilweise_nicht_anwendbar":"bestanden"));
        return J{{"geschwindigkeit_m_s",speed},{"dichte_kg_m3",rho},
                 {"viskositaet_m2_s",nu},{"q_max_Pa",q_max},
                 {"c_ref_m",cref},{"grundfaelle",108},
                 {"uebergangsfaelle",transition_count},{"modellfaelle",angles.size()},
                 {"h1_faelle",h1_cases},{"status",status},
                 {"schlechtester_vergleich",diagnostic},{"fehlgeschlagene_faelle",failures},
                 {"polpruefung",{{"status","bestanden"},{"vergleiche",pole_comparisons}}},
                 {"uebergang_diagnostisch",{{"vergleiche",transition_comparisons},
                     {"bezug","0,1 Prozent diagnostisch, keine eigenstaendige Abnahmegrenze"},
                     {"schlechtester_vergleich",transition_diagnostic}}}};
    };
    auto worker=[&] {
        try {
            struct WorkerSlot {
                std::atomic<int>& active;
                explicit WorkerSlot(std::atomic<int>& active_count,std::atomic<int>& maximum):active(active_count) {
                    int now=++active,old=maximum.load();
                    while(now>old&&!maximum.compare_exchange_weak(old,now)) {}
                }
                ~WorkerSlot(){--active;}
            } slot(active,peak);
            segel::detail::PreparedModel solver(model_text);
            segel::detail::SplitReference reference(source_text),finer(refined_text);
            segel::Context direct(source_text);
            while(!failed&&!cancelled) {
                const auto index=next.fetch_add(1);
                if(index>=speeds.size())break;
                results[index]=calculate(index,solver,reference,finer,direct);
                ++finished;
                progress.notify_one();
            }
        } catch(...) {
            std::lock_guard<std::mutex> guard(error_mutex);
            if(!failure)failure=std::current_exception();
            failed=true;
            progress.notify_one();
        }
    };
    std::vector<std::thread> threads;
    try {
        for(size_t i=0;i<std::min(static_cast<size_t>(budget),speeds.size());++i)
            threads.emplace_back(worker);
    } catch(const std::system_error& e) {
        failed=true;
        for(auto& thread:threads)thread.join();
        throw Error(3,std::string("Gleichwind-Worker konnten nicht gestartet werden: ")+e.what());
    } catch(...) {
        failed=true;
        for(auto& thread:threads)thread.join();
        throw;
    }
    size_t reported=0;
    try {
        log.log("Gleichwindpruefung","Beginn: "+std::to_string(speeds.size())+
                " Geschwindigkeiten mit hoechstens "+std::to_string(threads.size())+" Workern");
        auto last_report=Clock::now();
        std::unique_lock<std::mutex> lock(progress_mutex);
        while(finished<speeds.size()&&!failed&&!cancelled) {
            progress.wait_for(lock,std::chrono::seconds(1),[&] {
                return finished.load()!=reported||failed.load()||cancelled.load();
            });
            const auto done=finished.load();
            if(done!=reported) {
                reported=done;
                log.progress("Gleichwindpruefung",done,speeds.size(),"Geschwindigkeiten");
                last_report=Clock::now();
            } else if(Clock::now()-last_report>=std::chrono::seconds(30)) {
                log.log("Gleichwindpruefung","Laeuft: "+std::to_string(done)+" / "+
                        std::to_string(speeds.size())+" Geschwindigkeiten abgeschlossen");
                last_report=Clock::now();
            }
        }
    } catch(...) {
        failed=true;
        for(auto& thread:threads)thread.join();
        throw;
    }
    for(auto& thread:threads)thread.join();
    if(failure)std::rethrow_exception(failure);
    ensure(!cancelled,130,"Benutzerabbruch bei der Segelmodell-Gleichwindpruefung");
    ensure(finished==speeds.size(),4,"Segelmodell-Gleichwindpruefung unvollstaendig");
    if(reported!=speeds.size())
        log.progress("Gleichwindpruefung",speeds.size(),speeds.size(),"Geschwindigkeiten");
    peak_workers=peak.load();
    J reports=J::array();
    for(auto& result:results)reports.push_back(std::move(result));
    return reports;
}
J run_model_options(const fs::path& path,bool check,int limit,bool fingerprint,bool overwrite
#ifdef SEGELMACHER_ISOLIERTE_ABNAHME
                    ,const fs::path& reference_path,const std::string& reference_hash
#endif
                    ) {
    Logger log;
    auto absolute=fs::absolute(path);
#ifdef SEGELMACHER_ISOLIERTE_ABNAHME
    if(!check) {
        const auto temporary=fs::canonical(fs::temp_directory_path());
        const auto resolved=fs::canonical(absolute);
        auto prefix=temporary.begin();
        auto component=resolved.begin();
        for(;prefix!=temporary.end()&&component!=resolved.end()&&*prefix==*component;++prefix,++component) {}
        ensure(prefix==temporary.end()&&component!=resolved.end(),5,
               "Isolierte Abnahme ist nur innerhalb des temporaeren Verzeichnisses zulaessig");
    }
#endif
    const auto job_snapshot=read_json_snapshot(absolute);
    const auto& job=job_snapshot.value;
    fields(job,{"schema_version","status","eingabe","berechnung","numerik","ressourcen","segelmodell"},"auftrag");
    ensure(job.at("schema_version")=="0.1",2,"Nicht unterstuetzte Auftragsschemaversion");
    ensure(job.at("status")=="bereit"||(check&&job.at("status")=="entwurf_rechenparameter_nicht_freigegeben"),
           2,"Auftrag nicht bereit");
    const auto& input=job.at("eingabe");
    fields(input,{"segeldatei","konventionen"},"eingabe");
    auto geometry_path=(absolute.parent_path()/fs::u8path(input.at("segeldatei").get<std::string>())).lexically_normal();
    auto convention_path=(absolute.parent_path()/fs::u8path(input.at("konventionen").get<std::string>())).lexically_normal();
    const auto geometry_snapshot=read_json_snapshot(geometry_path);
    const auto convention_snapshot=read_json_snapshot(convention_path);
    const auto& geometry=geometry_snapshot.value;
    const auto& conventions=convention_snapshot.value;
    ensure(geometry.at("schema_version")=="0.1"&&conventions.at("schema_version")=="0.1",2,
           "Unbekannte Geometrie- oder Konventionsversion");
    const auto& convention=conventions.at("konvention");
    ensure(convention.at("laengeneinheit")=="m"&&convention.at("winkeleinheit")=="deg"&&
           convention.at("lokales_koordinatensystem").at("ursprung")=="hals"&&
           convention.at("lokales_koordinatensystem").at("x_positiv")=="voraus"&&
           convention.at("lokales_koordinatensystem").at("y_positiv")=="backbord"&&
           convention.at("lokales_koordinatensystem").at("z_positiv")=="oben",2,"Nicht unterstuetzte Segelkoordinaten");
    ensure(convention.at("bezuege").at("flaeche")=="ebene_grundkonturflaeche"&&
           convention.at("bezuege").at("laenge")=="ebene_grundkonturflaeche_geteilt_durch_konturhoehe"&&
           convention.at("bezuege").at("momentenursprung")=="hals"&&
           convention.at("bezuege").at("profilmoment_sehnenanteil")==0.25,2,"Nicht unterstuetzte Segelbezuege");
    const auto& resources=job.at("ressourcen");
    fields(resources,{"max_prozessorzahl","gpu"},"ressourcen");
    ensure(resources.at("max_prozessorzahl").is_number_integer(),2,"CPU-Limit muss ganzzahlig sein");
    auto requested=resources.at("max_prozessorzahl").get<int64_t>();
    ensure(requested>0&&requested<=100000,2,"CPU-Limit ausserhalb 1..100000");
    auto gpu=resources.at("gpu").get<std::string>();
    ensure(gpu=="aus"||gpu=="automatisch"||gpu=="erforderlich",2,"Ungueltige GPU-Auswahl");
    ensure(gpu!="erforderlich",3,"GPU erforderlich: fuer die Segelmodell-Pruefung ist kein GPU-Backend implementiert");
    int budget=std::min(static_cast<int>(requested),static_cast<int>(processors()));
    if(limit>0)budget=std::min(budget,limit);
    log.log("Ressourcen","CPU-Limit "+std::to_string(budget)+
            "; Segelmodell-Pruefung auf CPU"+(gpu=="automatisch"?" (kein GPU-Backend implementiert)":""));
    const auto& config=job.at("segelmodell");
    fields(config,{"dichte_kg_m3","viskositaet_m2_s"},"segelmodell");
    J validity={{"geschwindigkeit_m_s",{0,50}},{"dichte_kg_m3",config.at("dichte_kg_m3")},
                {"viskositaet_m2_s",config.at("viskositaet_m2_s")}};
    fields(job.at("berechnung"),{"modellstand","parameter"},"berechnung");
    ensure(job.at("berechnung").at("modellstand")=="H1",2,"Segelmodell erfordert Modellstand H1");
    J source={{"geometrie",geometry},{"numerik",job.at("numerik")},{"parameter",job.at("berechnung").at("parameter")}};
    auto coarse=J::parse(segel::detail::prepare_model_data(source.dump()));
    validate_model_validity(coarse,validity);
    auto rho=validity.at("dichte_kg_m3"),nu=validity.at("viskositaet_m2_s");
    ensure(rho[0].get<double>()<=1.225&&rho[1].get<double>()>=1.225&&
           nu[0].get<double>()<=1.5e-5&&nu[1].get<double>()>=1.5e-5,2,
           "Luftdatenbereiche muessen die festgelegten numerischen Testwerte enthalten");
    if(check)return success({},"Segelmodell-Eingaben geprueft; numerische Abnahme nicht ausgefuehrt");
    ensure(!cancelled,130,"Benutzerabbruch vor der Segelmodell-Erzeugung");
    const auto output=absolute.parent_path()/"Segelmodell";
    const auto lock=absolute.parent_path()/"Segelmodell.lock";
    std::error_code lock_error;
    bool locked=fs::create_directory(lock,lock_error);
    ensure(locked&&!lock_error,5,"Segelmodell-Ausgabe gesperrt oder nicht beschreibbar: "+
           lock.u8string()+"; Systemfehler "+std::to_string(lock_error.value()));
    struct ModelUnlock {
        fs::path path;
        bool held=true;
        void release() {
            std::error_code ec;
            bool removed=fs::remove(path,ec);
            ensure(removed&&!ec,5,"Segelmodell-Sperre kann nicht entfernt werden: "+path.u8string()+
                   "; Systemfehler "+std::to_string(ec.value()));
            held=false;
        }
        ~ModelUnlock() {
            if(held) {
                std::error_code ec;
                bool removed=fs::remove(path,ec);
                if(!removed||ec)
                    std::cerr<<"FEHLER: Segelmodell-Sperre kann nicht entfernt werden: "<<path.u8string()
                             <<"; Systemfehler "<<ec.value()<<'\n';
            }
        }
    } unlock{lock};
    if(fs::exists(output)) {
        ensure(!fs::is_symlink(output)&&fs::is_directory(output),5,"Segelmodell muss ein regulaerer Ordner sein");
        if(!overwrite) {
            auto result=success(output,"Vorhandenes Segelmodell nicht neu geschrieben");
            result["pruefbericht"]=nullptr;
            result["output_action"]="skipped";
            if(fingerprint)update_fingerprint(result,fingerprint_model(absolute.parent_path()),log);
            unlock.release();
            return result;
        }
    }
    J input_provenance={
        {"auftrag",{{"pfad",absolute.u8string()},{"sha256",hash_json_snapshot(job_snapshot,absolute)},
                     {"schema_version",job.at("schema_version")}}},
        {"geometrie",{{"pfad",geometry_path.u8string()},{"sha256",hash_json_snapshot(geometry_snapshot,geometry_path)},
                       {"schema_version",geometry.at("schema_version")}}},
        {"konventionen",{{"pfad",convention_path.u8string()},{"sha256",hash_json_snapshot(convention_snapshot,convention_path)},
                          {"schema_version",conventions.at("schema_version")}}}
    };
    const auto& arrow_version=arrow::GetBuildInfo().version_string;
    ensure(!arrow_version.empty(),3,"Laufzeitversion der Arrow-Bibliothek nicht verfuegbar");
#if defined(SEGELMACHER_PARQUET_PACKAGE_VERSION) && defined(SEGELMACHER_PARQUET_PACKAGE_SHA256)
    auto parquet_version=parquet_library_provenance(SEGELMACHER_PARQUET_PACKAGE_VERSION,
                                                     SEGELMACHER_PARQUET_PACKAGE_SHA256);
#else
    auto parquet_version=parquet_library_provenance("","");
#endif
#if defined(SEGELMACHER_ARROW_PACKAGE_SHA256) && defined(SEGELMACHER_PARQUET_PACKAGE_VERSION) && defined(SEGELMACHER_PARQUET_PACKAGE_SHA256)
    auto zstd_version=zstd_library_provenance(SEGELMACHER_ARROW_PACKAGE_SHA256,
                                              SEGELMACHER_PARQUET_PACKAGE_VERSION,SEGELMACHER_PARQUET_PACKAGE_SHA256);
#else
    auto zstd_version=zstd_library_provenance("","","");
#endif
    J library_versions={{"arrow",{{"status","nachgewiesen"},{"version",arrow_version}}},
                        {"parquet",parquet_version},
                        {"zstd",zstd_version}};
    J reused_report=nullptr;
#ifdef SEGELMACHER_ISOLIERTE_ABNAHME
    if(!reference_path.empty()) {
        const auto snapshot=read_json_snapshot(reference_path);
        const auto digest=hash_json_snapshot(snapshot,reference_path);
        ensure(digest==reference_hash,4,"Uebernommener Vorpruefbericht: SHA-256 stimmt nicht");
        reused_report=snapshot.value;
        ensure(reused_report.at("exitcode")==0&&reused_report.at("status")=="erfolgreich"&&
               reused_report.at("nachweise").at("fehlgeschlagen").empty(),
               4,"Uebernommener Vorpruefbericht ist ungueltig");
        ensure(reused_report.at("werkzeugversion")==SEGELMACHER_VORPRUEF_VERSION,4,
               "Uebernommener Vorpruefbericht passt nicht zur Werkzeugversion");
        for(const char* key:{"auftrag","geometrie","konventionen"})
            ensure(reused_report.at("eingaben").at(key).at("sha256")==input_provenance.at(key).at("sha256")&&
                   reused_report.at("eingaben").at(key).at("schema_version")==input_provenance.at(key).at("schema_version"),
                   4,std::string("Uebernommener Vorpruefbericht passt nicht zur Eingabe: ")+key);
        ensure(reused_report.at("bibliotheksversionen")==library_versions,4,
               "Uebernommener Vorpruefbericht passt nicht zu den Bibliotheksversionen");
        const auto& prior=reused_report.at("nachweise");
        ensure(prior.at("physikalisch_validiert")==false,4,
               "Uebernommener Vorpruefbericht hat ungueltigen Validierungsstatus");
        for(const char* name:{"rastervergleich_vorpruefung","nullwindfolge","gleichwind_vorpruefung",
                              "h1_referenzvergleich_vorpruefung","tabellen_roundtrip_vorpruefung",
                              "modell_lade_und_rechenprobe"}) {
            for(const char* state:{"ausgefuehrt","bestanden"}) {
                const auto& entries=prior.at(state);
                ensure(entries.is_array()&&std::count(entries.begin(),entries.end(),J(name))==1,4,
                       std::string("Uebernommener Vorpruefbericht unvollstaendig: ")+name);
            }
        }
        const auto& prior_roundtrip=reused_report.at("numerik_vorpruefung").at("tabellen_roundtrip");
        ensure(prior_roundtrip.at("status")=="bestanden"&&prior_roundtrip.at("ladeprobe").at("status")=="bestanden",
               4,"Uebernommener Tabellenroundtrip ist nicht bestanden");
        input_provenance["vorpruefbericht"]={{"pfad",fs::absolute(reference_path).u8string()},
                                              {"sha256",digest},{"schema_version","unversioniert"}};
        log.log("Qualitaetspruefung","Vorpruefbericht anhand von Hash und Eingaben uebernommen");
    }
#endif
    segel::detail::PreparedModel zero_solver(coarse.dump());
    J gradient_cases,zero_sequence,continuity,re_boundaries,uniform_cases=nullptr;
    int uniform_peak=0;
    if(!reused_report.is_null()) {
        const auto& prior=reused_report.at("numerik_vorpruefung");
        gradient_cases=prior.at("gradienten");
        zero_sequence=prior.at("nullwindfolge");
        continuity=prior.at("stetigkeit_diagnostisch");
        re_boundaries=prior.at("re_min_diagnostisch");
        uniform_cases=prior.at("gleichwind");
        ensure(gradient_cases.is_array()&&gradient_cases.size()==14&&uniform_cases.is_array()&&
               uniform_cases.size()==model_uniform_speeds.size(),4,
               "Uebernommener Vorpruefbericht hat unvollstaendige Faelle");
        for(size_t i=0;i<model_uniform_speeds.size();++i) {
            const auto& item=uniform_cases.at(i);
            const auto& status=item.at("status");
            ensure(item.at("geschwindigkeit_m_s")==model_uniform_speeds[i]&&
                   item.at("dichte_kg_m3")==1.225&&item.at("viskositaet_m2_s")==1.5e-5&&
                   (status=="bestanden"||status=="h1_nicht_anwendbar")&&
                   item.at("fehlgeschlagene_faelle").is_array()&&item.at("fehlgeschlagene_faelle").empty(),
                   4,"Uebernommener Gleichwindbericht hat ungueltige Faelle");
        }
    } else {
        log.log("Qualitaetspruefung","Beginn: Segelmodell-Rastergradienten");
        auto fine=J::parse(segel::detail::prepare_model_data(source.dump(),2));
        gradient_cases=check_model_gradients(coarse,fine);
        ensure(!cancelled,130,"Benutzerabbruch nach der Segelmodell-Rasterpruefung");
        zero_sequence=segel::detail::check_zero_sequence(coarse,zero_solver,[]{
            ensure(!cancelled,130,"Benutzerabbruch bei der Segelmodell-Nullwindfolge");
        });
        continuity=segel::detail::check_wind_continuity(coarse,zero_solver,[]{
            ensure(!cancelled,130,"Benutzerabbruch bei der Segelmodell-Stetigkeitsdiagnose");
        });
        re_boundaries=segel::detail::check_re_boundaries(coarse,zero_solver,[]{
            ensure(!cancelled,130,"Benutzerabbruch bei der Segelmodell-Reynoldsdiagnose");
        });
    }
    bool gradients_pass=std::all_of(gradient_cases.begin(),gradient_cases.end(),[](const J& item) {
        return item.at("status")=="bestanden";
    });
    if(gradients_pass&&reused_report.is_null()) uniform_cases=check_model_uniforms(source,coarse,log,budget,uniform_peak);
    bool uniform_pass=gradients_pass&&std::none_of(uniform_cases.begin(),uniform_cases.end(),[](const J& item) {
        return item.at("status")=="fehlgeschlagen";
    });
    J evidence={{"ausgefuehrt",J::array({"rastervergleich_vorpruefung","nullwindfolge"})},
                {"bestanden",J::array({"nullwindfolge"})},{"fehlgeschlagen",J::array()},
                {"nicht_ausgefuehrt",J::array({"vollstaendige_numerikabnahme","datenformat_und_integritaet",
                                               "end_to_end_integration","geschuetzte_publikation"})},
                {"physikalisch_validiert",false}};
    evidence[gradients_pass?"bestanden":"fehlgeschlagen"].push_back("rastervergleich_vorpruefung");
    if(gradients_pass) {
        evidence["ausgefuehrt"].push_back("gleichwind_vorpruefung");
        evidence[uniform_pass?"bestanden":"fehlgeschlagen"].push_back("gleichwind_vorpruefung");
        bool h1_executed=std::any_of(uniform_cases.begin(),uniform_cases.end(),[](const J& item) {
            return item.at("h1_faelle").get<size_t>()>0;
        });
        if(h1_executed) {
            evidence["ausgefuehrt"].push_back("h1_referenzvergleich_vorpruefung");
            evidence[uniform_pass?"bestanden":"fehlgeschlagen"].push_back("h1_referenzvergleich_vorpruefung");
        } else evidence["nicht_ausgefuehrt"].push_back("h1_referenzvergleich_vorpruefung");
    } else {
        evidence["nicht_ausgefuehrt"].push_back("gleichwind_vorpruefung");
        evidence["nicht_ausgefuehrt"].push_back("h1_referenzvergleich_vorpruefung");
    }
    J table_roundtrip={{"status","nicht_ausgefuehrt"},
                       {"grund","numerik_oder_bibliotheksprovenienz_unvollstaendig"}};
    if(uniform_pass&&library_versions.at("parquet").at("status")=="nachgewiesen"&&
       library_versions.at("zstd").at("status")=="nachgewiesen") {
        log.log("Qualitaetspruefung","Beginn: Segelmodell-Tabellenroundtrip im Arbeitsordner");
        std::string token=std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        const auto stage=absolute.parent_path()/fs::u8path("Segelmodell.arbeiten."+token);
        struct StageCleanup {
            const fs::path& path;
            bool held=false;
            void release() {
                std::error_code ec;
                fs::remove_all(path,ec);
                ensure(!ec,5,"Segelmodell-Arbeitsordner kann nicht entfernt werden: "+path.u8string()+
                       "; Systemfehler "+std::to_string(ec.value()));
                held=false;
            }
            ~StageCleanup() {
                if(held) {
                    std::error_code ec;
                    fs::remove_all(path,ec);
                    if(ec)std::cerr<<"FEHLER: Segelmodell-Arbeitsordner kann nicht entfernt werden: "
                                   <<path.u8string()<<"; Systemfehler "<<ec.value()<<'\n';
                }
            }
        } cleanup{stage};
        J provenance={{"gueltigkeit",validity},{"eingaben",input_provenance},
                      {"erzeuger",{{"werkzeug",SEGELMACHER_VERSION},{"arrow",arrow_version},
                                    {"parquet",library_versions.at("parquet").at("version")},
                                    {"zstd",library_versions.at("zstd").at("version")}}},
                      {"numerik",job.at("numerik")},{"nachweise",evidence}};
        auto manifest=create_model_stage(stage,output,coarse,provenance,[]{
            ensure(!cancelled,130,"Benutzerabbruch beim Segelmodell-Tabellenroundtrip");
        },budget);
        cleanup.held=true;
        if(!reused_report.is_null())
            ensure(manifest.at("tabellen")==reused_report.at("numerik_vorpruefung").at("tabellen_roundtrip").at("tabellen"),
                   4,"Uebernommener Vorpruefbericht passt nicht zu den erzeugten Tabellen");
        size_t loaded_strips=0;
        try {
            auto loaded=segel::modell::laden(stage);
            const auto points=loaded->punkte();
            ensure(points.size()==coarse.at("streifen").size(),5,"Segelmodell-Ladeprobe: Streifenzahl weicht ab");
            std::vector<segel::modell::Wind> winds;
            J raw_winds=J::array();
            for(size_t id=0;id<points.size();++id) {
                ensure(points[id].id==id&&points[id].position_m==model_point(coarse.at("streifen")[id]),5,
                       "Segelmodell-Ladeprobe: Streifen-ID oder Position weicht ab");
                winds.push_back({points[id].id,{-5,0,0}});
                raw_winds.push_back({{"streifen_id",points[id].id},{"wind_m_s",{-5,0,0}}});
            }
            auto expected=J::parse(zero_solver.solve(raw_winds.dump(),1.225,1.5e-5));
            auto actual=loaded->berechnen(winds,1.225,1.5e-5);
            ensure(actual.size()==expected.size(),5,"Segelmodell-Ladeprobe: Lastzahl weicht ab");
            for(size_t id=0;id<actual.size();++id) {
                ensure(actual[id].id==id&&actual[id].kraft_N==expected[id].at("kraft_N").get<ModelVector>()&&
                       actual[id].moment_Nm==expected[id].at("moment_Nm").get<ModelVector>(),5,
                       "Segelmodell-Ladeprobe: Lasten weichen vom Ausgangsmodell ab");
            }
            loaded_strips=points.size();
        } catch(const segel::modell::Fehler& e) {
            throw Error(e.art()==segel::modell::Fehlerart::numerik?4:5,
                        std::string("Segelmodell-Ladeprobe fehlgeschlagen: ")+e.what());
        }
        table_roundtrip={{"status","bestanden"},{"tabellen",manifest.at("tabellen")},
                         {"manifest_sha256",hash(stage/"manifest.json")},
                         {"ladeprobe",{{"status","bestanden"},{"streifen",loaded_strips},
                                       {"wind_m_s",{-5,0,0}},{"dichte_kg_m3",1.225},
                                       {"viskositaet_m2_s",1.5e-5}}}};
        evidence["ausgefuehrt"].push_back("tabellen_roundtrip_vorpruefung");
        evidence["bestanden"].push_back("tabellen_roundtrip_vorpruefung");
        evidence["ausgefuehrt"].push_back("modell_lade_und_rechenprobe");
        evidence["bestanden"].push_back("modell_lade_und_rechenprobe");
        const std::set<std::string> project_evidence={"vollstaendige_numerikabnahme","datenformat_und_integritaet",
                                                       "end_to_end_integration","geschuetzte_publikation"};
        bool publication_ready=std::all_of(evidence.at("nicht_ausgefuehrt").begin(),
                                           evidence.at("nicht_ausgefuehrt").end(),
                                           [&](const J& item) { return project_evidence.count(item.get<std::string>())!=0; });
        if(publication_ready&&evidence.at("fehlgeschlagen").empty()) {
            manifest["nachweise"]=evidence;
            write(stage/"manifest.json",manifest);
            fs::path model_root;
            if(fingerprint)model_root=fingerprint_model(absolute.parent_path());
            auto result=publish_model_result(stage,output,overwrite,[&](J& published) {
                if(fingerprint)update_fingerprint(published,model_root,log);
                cleanup.release();
                unlock.release();
                log.log("Export","Segelmodell abgeschlossen: "+output.u8string());
            });
            result["nachweise"]=evidence;
            result["numerik_vorpruefung"]={{"gradienten",gradient_cases},
                {"nullwindfolge",zero_sequence},{"stetigkeit_diagnostisch",continuity},
                {"re_min_diagnostisch",re_boundaries},{"gleichwind",uniform_cases},
                {"gleichwind_worker_spitze",uniform_peak},{"tabellen_roundtrip",table_roundtrip},
                {"weitere_nachweise","nicht_ausgefuehrt"}};
            result["eingaben"]=input_provenance;
            result["ressourcen"]={{"cpu_angefordert",requested},{"cpu_effektiv",budget},{"cli_limit",limit}};
            result["werkzeugversion"]=SEGELMACHER_VERSION;
            result["bibliotheksversionen"]=library_versions;
#ifdef SEGELMACHER_ISOLIERTE_ABNAHME
            result["projektfreigabe"]=false;
            result["modus"]="isolierte_abnahme";
            result["numerikquelle"]=reused_report.is_null()?"neu_berechnet":"uebernommener_vorpruefbericht";
#endif
            if(result.at("exitcode")!=0)
                log.log("Abschluss",result.at("meldung").get<std::string>()+
                        " | Segelmodell bereits geschrieben: "+output.u8string(),"FEHLER");
            return result;
        }
        cleanup.release();
        ensure(!cancelled,130,"Benutzerabbruch nach dem Segelmodell-Tabellenroundtrip");
        log.log("Qualitaetspruefung","Segelmodell-Tabellenroundtrip bestanden; Arbeitsordner entfernt");
    } else {
        evidence["nicht_ausgefuehrt"].push_back("tabellen_roundtrip_vorpruefung");
        evidence["nicht_ausgefuehrt"].push_back("modell_lade_und_rechenprobe");
    }
    std::string message=uniform_pass
        ?"Segelmodell nicht publiziert: weitere verpflichtende Numerik- und Integrationsnachweise fehlen"
        :"Segelmodell nicht publiziert: Raster- oder Gleichwindpruefung fehlgeschlagen; weitere Pflichtnachweise fehlen";
    log.log("Abschluss",message,"FEHLER");
    unlock.release();
    return {{"status","fehlgeschlagen"},{"exitcode",4},{"ergebnisordner",nullptr},{"pruefbericht",nullptr},
            {"meldung",message},{"numerik_vorpruefung",{{"gradienten",gradient_cases},
                {"nullwindfolge",zero_sequence},
                {"stetigkeit_diagnostisch",continuity},
                {"re_min_diagnostisch",re_boundaries},
                {"gleichwind",uniform_cases},
                {"gleichwind_worker_spitze",uniform_peak},
                {"tabellen_roundtrip",table_roundtrip},
                {"weitere_nachweise","nicht_ausgefuehrt"}}},{"eingaben",input_provenance},
                {"ressourcen",{{"cpu_angefordert",requested},{"cpu_effektiv",budget},{"cli_limit",limit}}},
                {"werkzeugversion",SEGELMACHER_VERSION},
                {"bibliotheksversionen",library_versions},{"nachweise",evidence}};
}
J run_options(const fs::path& path,bool check,int limit,bool fingerprint,bool overwrite) {
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
    const auto model_root=fingerprint?fingerprint_model(parent):fs::path{};
    std::error_code ec;
    bool locked=fs::create_directory(lock,ec);
    ensure(locked&&!ec,5,"Ausgabe gesperrt oder nicht beschreibbar: "+lock.u8string());
    struct Unlock { fs::path p;~Unlock(){std::error_code e;fs::remove(p,e);} } unlock{lock};
    if(fs::exists(output)) {
        ensure(!fs::is_symlink(output)&&fs::is_directory(output),5,"LookUp muss ein regulaerer Ordner sein");
        if(!overwrite) {
            log.log("Export","Vorhandener LookUp wird uebersprungen (create-only)");
            auto result=success(output,"Vorhandener LookUp nicht neu geschrieben");
            result["output_action"]="skipped";
            if(fingerprint)update_fingerprint(result,model_root,log);
            return result;
        }
    }
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
    bool published=false;
    try {
        log.log("Geometrieaufbereitung","Beginn: Konturschnitt, Woelbung/Twist, Raster 1/2/4");
        const auto& grid=input.grid;
        log.log("Geometrieaufbereitung","Abgeschlossen: S="+std::to_string(grid.at("S_m2").get<double>())+" m2");
        const std::vector<double>& lookup_angles=input.angles;
        std::vector<double> lookup_elevations=input.elevations;
        std::vector<double> added_elevations;
        for(double sign:{-1.,1.})for(int interval=1;interval<16;++interval) {
            double beta=sign*(50+2.5*interval);
            if(std::find(lookup_elevations.begin(),lookup_elevations.end(),beta)==lookup_elevations.end()) {
                lookup_elevations.insert(std::lower_bound(lookup_elevations.begin(),lookup_elevations.end(),beta),beta);
                added_elevations.push_back(beta);
            }
        }
        for(double beta:{-88.75,88.75}) {
            lookup_elevations.insert(std::lower_bound(lookup_elevations.begin(),lookup_elevations.end(),beta),beta);
            added_elevations.push_back(beta);
        }
        ensure(lookup_angles.size()*lookup_elevations.size()*input.res.size()<=500000,3,
               "Erweiterter Rechenplan ueber Ressourcenlimit von 500000 Summenzeilen");
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
        for(double sign:{-1.,1.})for(size_t ir=0;ir<input.res.size();++ir)
            for(size_t ia=0;ia<na;++ia) {
                auto it=std::find(lookup_elevations.begin(),lookup_elevations.end(),sign*90);
                ensure(it!=lookup_elevations.end(),4,"Signed Windpol fehlt in der Ausgabestuetzzone");
                size_t ib=static_cast<size_t>(it-lookup_elevations.begin());
                size_t i=(ir*na+ia)*nb+ib;
                auto c=base[i];c.b=sign*89.999;nearpole.push_back(c);poleindexes.push_back(i);
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
        ParquetTableWriter sums(stage/"summen.parquet",sums_schema());
        ParquetTableWriter forces(stage/"kraefte.parquet",forces_schema());
        ParquetTableWriter profiles(stage/"profile.parquet",profiles_schema());
        auto component=[](const J& arr,size_t i){return number_scalar(arr.at(i).get<double>());};
        auto nullable_component=[](const J& arr,size_t i,bool valid) {
            return valid?number_scalar(arr.at(i).get<double>()):null_number_scalar();
        };
        const size_t export_total=count+grid.at("grids").at("4").size()+1;
        size_t exported=0;auto last=Clock::now();
        log.progress("Export",0,export_total,"Beginn Tabellenexport | Teil: 0/"+std::to_string(export_total));
        for(double a:lookup_angles)for(double b:lookup_elevations)for(double re:input.res) {
            ensure(!cancelled,130,"Benutzerabbruch beim Export");
            J r=J::parse(input.context->solve(a,b,re,4,true));
            const bool point_valid=r.at("valid").get<bool>();
            sums.append({text_scalar("eingabe"),number_scalar(a),number_scalar(b),number_scalar(re),
                component(r.at("CF"),0),component(r.at("CF"),1),component(r.at("CF"),2),
                component(r.at("CM"),0),component(r.at("CM"),1),component(r.at("CM"),2),
                nullable_component(r.at("point"),0,point_valid),nullable_component(r.at("point"),1,point_valid),
                nullable_component(r.at("point"),2,point_valid),boolean_scalar(point_valid),
                component(r.at("rest"),0),component(r.at("rest"),1),component(r.at("rest"),2),text_scalar(quality)});
            for(const auto& l:r.at("loads")) {
                forces.append({text_scalar("eingabe"),number_scalar(a),number_scalar(b),number_scalar(re),
                    integer_scalar(l.at("id").get<int64_t>()),component(l.at("f"),0),component(l.at("f"),1),
                    component(l.at("f"),2),component(l.at("r"),0),component(l.at("r"),1),
                    component(l.at("r"),2),component(l.at("m"),0),component(l.at("m"),1),
                    component(l.at("m"),2),text_scalar(quality)});
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
                profiles.append({text_scalar(row.at("profil_id").get<std::string>()),number_scalar(a),
                    number_scalar(localre),number_scalar(value.cl),number_scalar(value.cd),
                    number_scalar(value.cm),text_scalar(quality)});
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
                {"fingerprint",fingerprint?"update":"skip"},{"output_policy",overwrite?"overwrite":"create-only"},
                {"gpu_plattform",!gpu_platform.empty()?J(gpu_platform):J(nullptr)},
                {"gpu_geraet",!gpu_device.empty()?J(gpu_device):J(nullptr)},
                {"modellstand",model},{"toolversion","0.1.0"},
                {"interpolation_stuetzstellen",report.at("interpolation_stuetzstellen")}}},{"sha256",inputhash}});
        report["status"]="erfolgreich";report["summenzeilen"]=count;
        report["cpu_peak"]=input.peak.load();
        report["dauer_s"]=std::chrono::duration<double>(Clock::now()-log.start).count();
        write(stage/"pruefbericht.json",report);
        J hashes;
        for(const auto& name:{"summen.parquet","kraefte.parquet","profile.parquet","pruefbericht.json","auftrag_ausgefuehrt.json"})hashes[name]=hash(stage/name);
        write(stage/"lookup.json",{{"formatversion","3"},{"status","erfolgreich"},{"qualitaet",quality},{"modellstand",model},
            {"parameter",input.parameters},{"segel_id",grid.at("segel_id")},{"geometrieschema","0.1"},{"geometrie",grid},
            {"konventionen",input.conv},{"formvarianten",input.job.at("formvarianten")},{"betriebspunkte",input.job.at("betriebspunkte")},
            {"windkonvention",{{"windherkunft","h=(cos(beta)*cos(alpha),cos(beta)*sin(alpha),sin(beta)); beta signiert"},
                {"stroemung","u=-h"},{"neigungsbereich_deg",{-90,90}},
                {"polverhalten","Azimut an beiden Polen unbestimmt; Lasten azimutinvariant"},
                {"anliegende_ueberblendung_abs_beta_deg",{60,80}}}},
            {"reynolds_referenz",{{"bezugspunkt","segelhals"},{"bezugslaenge","c_ref_m"},
                {"definition","Re_ref=|v_rel|*c_ref/nu; q=0.5*rho*|v_rel|^2"},
                {"interpolation","linear_in_log_Re_innerhalb_manifestierter_stuetzstellen"},
                {"ausserhalb","diagnose_ohne_extrapolation_oder_klemmen"}}},
            {"interpolation",{{"verfahren","periodische_tensorprodukt_cubic_hermite"},
                {"stuetzstellen",report.at("interpolation_stuetzstellen")}}},
            {"summenzeilen",count},{"parquet",{{"bibliothek","Apache Arrow"},{"bibliotheksversion","21.0.0"},
                {"schema_version","1"},{"kompression","ZSTD"},{"row_group_max_zeilen",parquet_row_group_rows},
                {"tabellen",{{"summen.parquet",{{"zeilen",sums.row_count()},{"row_groups",sums.row_group_count()},{"schema",sums_schema()->ToString()}}},
                    {"kraefte.parquet",{{"zeilen",forces.row_count()},{"row_groups",forces.row_group_count()},{"schema",forces_schema()->ToString()}}},
                    {"profile.parquet",{{"zeilen",profiles.row_count()},{"row_groups",profiles.row_group_count()},{"schema",profiles_schema()->ToString()}}}}},
                {"einheiten",{{"alpha_deg","deg"},{"windneigung_deg","deg"},{"re_ref","1"},{"re","1"},
                    {"CF_und_CM","1"},{"CL_CD_CM_profil","1"},{"angriff_und_ansatz","m"},{"IDs_Qualitaet","Text"}}}}},
            {"bezuege",{{"S_m2",grid.at("S_m2")},{"c_ref_m",grid.at("c_ref_m")},{"kraftschwelle",1e-10},{"momentenursprung","hals"}}},
            {"lastmodell",{{"gebunden","Viertelsehne plus freies Fourier-Profilmoment"},{"abgeloest","4 Mittelflaechen-Impulsbeitraege pro Streifen"}}},
            {"grenzen",report.at("warnungen")},{"interferenz","nicht_berechnet"},{"eingaben_sha256",inputhash},{"dateien_sha256",hashes},
            {"pruefbericht","pruefbericht.json"}});
        ensure(!cancelled,130,"Benutzerabbruch vor Veroeffentlichung");
        const auto previous=parent/fs::u8path("LookUp.ersetzen."+token);
        const bool replacing=fs::exists(output);
        ensure(!replacing||overwrite,5,"LookUp zwischenzeitlich angelegt");
        if(replacing) {
            fs::rename(output,previous,ec);
            ensure(!ec,5,"Bisheriger Lookup kann nicht zum Austausch verschoben werden: "+
                output.u8string()+"; Systemfehler "+std::to_string(ec.value()));
        }
        fs::rename(stage,output,ec);
        if(ec) {
            const auto message=std::to_string(ec.value());
            if(replacing) {
                std::error_code rollback;
                fs::rename(previous,output,rollback);
                ensure(!rollback,5,"Veroeffentlichung und Wiederherstellung fehlgeschlagen; bisheriger Lookup: "+
                    previous.u8string()+"; Systemfehler "+std::to_string(rollback.value()));
            }
            throw Error(5,"Veroeffentlichung fehlgeschlagen; Systemfehler "+message);
        }
        published=true;
        if(replacing) {
            fs::remove_all(previous,ec);
            ensure(!ec,5,"Neuer Lookup geschrieben, alter Austauschordner kann nicht entfernt werden: "+
                previous.u8string()+"; Systemfehler "+std::to_string(ec.value()));
        }
        auto result=success(output,quality);
        result["output_action"]=replacing?"overwritten":"created";
        if(fingerprint)update_fingerprint(result,model_root,log);
        log.log("Export","Abgeschlossen | Gesamt: 100 % | Phasen: 6/6 | "+output.u8string());
        return result;
    } catch(const std::exception& e) {
        int code=4;if(auto err=dynamic_cast<const Error*>(&e))code=err->code;
        if(dynamic_cast<const std::ios_base::failure*>(&e))code=5;
        if(dynamic_cast<const fs::filesystem_error*>(&e))code=5;
        if(dynamic_cast<const std::bad_alloc*>(&e))code=3;
        if(published) {
            log.log("Abschluss",std::string(e.what())+" | Lookup bereits geschrieben: "+output.u8string(),"FEHLER");
            return {{"status","fehlgeschlagen"},{"exitcode",code},{"ergebnisordner",output.u8string()},
                {"pruefbericht",(output/"pruefbericht.json").u8string()},{"output_action","written"},
                {"meldung",e.what()}};
        }
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
}
J run(const fs::path& path,bool check,int limit) {
    return run_options(path,check,limit,false,false);
}
J dispatch(const std::vector<std::string>& args) {
    try {
        if(args.size()==1&&(args[0]=="--hilfe"||args[0]=="--version"))
            return success({},args[0]=="--version"?std::string("SegelMacher ")+SEGELMACHER_VERSION+", H1":
#ifdef SEGELMACHER_ENABLE_LEGACY_LOOKUP
                "SegelMacher lookup|segelmodell|pruefen --auftrag <UTF-8-Pfad> [--max-prozessorzahl N] "
#else
                "SegelMacher segelmodell|pruefen --auftrag <UTF-8-Pfad> [--max-prozessorzahl N] "
#endif
                "[--fingerprint update|skip] [--output-policy overwrite|create-only]; "
                "OpenCL-FP64-GPU fuer Profilpolaren, CPU-Fallback bei gpu=automatisch");
        ensure(!args.empty()&&(args[0]=="lookup"||args[0]=="segelmodell"||args[0]=="pruefen"),
               2,"Befehl segelmodell oder pruefen erforderlich; --hilfe");
#ifdef SEGELMACHER_ISOLIERTE_ABNAHME
        ensure(args[0]!="lookup",2,"Lookup-Erzeugung ist im isolierten Abnahme-Build nicht verfuegbar");
#elif !defined(SEGELMACHER_ENABLE_LEGACY_LOOKUP)
        ensure(args[0]!="lookup",2,"Lookup-Erzeugung wurde durch segelmodell abgeloest");
#endif
        fs::path path;int limit=0;bool seenlimit=false;
#ifdef SEGELMACHER_ISOLIERTE_ABNAHME
        fs::path reference_path;
        std::string reference_hash;
#endif
        bool fingerprint=false,overwrite=false,seenfingerprint=false,seenpolicy=false;
        for(size_t i=1;i<args.size();++i) {
            auto option=args[i];
            bool known_option=option=="--auftrag"||option=="--max-prozessorzahl"||
                option=="--fingerprint"||option=="--output-policy";
#ifdef SEGELMACHER_ISOLIERTE_ABNAHME
            known_option=known_option||option=="--vorpruefbericht"||option=="--vorpruefbericht-sha256";
#endif
            ensure(known_option&&i+1<args.size(),2,"Ungueltige oder unvollstaendige Option: "+option);
            auto value=args[++i];
            if(option=="--auftrag") {ensure(path.empty(),2,"--auftrag doppelt");path=fs::u8path(value);}
#ifdef SEGELMACHER_ISOLIERTE_ABNAHME
            else if(option=="--vorpruefbericht") {
                ensure(reference_path.empty()&&!value.empty(),2,"--vorpruefbericht fehlt oder ist doppelt");
                reference_path=fs::u8path(value);
            } else if(option=="--vorpruefbericht-sha256") {
                ensure(reference_hash.empty()&&value.size()==64&&
                       value.find_first_not_of("0123456789abcdef")==std::string::npos,2,
                       "--vorpruefbericht-sha256 erwartet genau 64 kleine Hexadezimalzeichen");
                reference_hash=value;
            }
#endif
            else if(option=="--fingerprint") {
                ensure(!seenfingerprint,2,"--fingerprint doppelt");seenfingerprint=true;
                ensure(value=="update"||value=="skip",2,"--fingerprint erwartet update oder skip");
                fingerprint=value=="update";
            } else if(option=="--output-policy") {
                ensure(!seenpolicy,2,"--output-policy doppelt");seenpolicy=true;
                ensure(value=="overwrite"||value=="create-only",2,"--output-policy erwartet overwrite oder create-only");
                overwrite=value=="overwrite";
            }
            else {
                ensure(!seenlimit,2,"--max-prozessorzahl doppelt");seenlimit=true;
                ensure(!value.empty()&&value.find_first_not_of("0123456789")==std::string::npos,2,"--max-prozessorzahl positive Ganzzahl erforderlich");
                try {limit=std::stoi(value);}catch(...){throw Error(2,"--max-prozessorzahl ausserhalb Wertebereich");}
                ensure(limit>0,2,"--max-prozessorzahl muss positiv sein");
            }
        }
        ensure(!path.empty(),2,"--auftrag fehlt");
        ensure(args[0]!="pruefen"||(!seenfingerprint&&!seenpolicy),2,
            "--fingerprint und --output-policy sind fuer pruefen nicht vorgesehen");
#ifdef SEGELMACHER_ISOLIERTE_ABNAHME
        ensure(reference_path.empty()==reference_hash.empty()&&
               (reference_path.empty()||args[0]=="segelmodell"),2,
               "Vorpruefbericht und SHA-256 sind gemeinsam nur fuer segelmodell zulaessig");
#endif
        if(args[0]=="segelmodell"||(args[0]=="pruefen"&&read(path).contains("segelmodell")))
            return run_model_options(path,args[0]=="pruefen",limit,fingerprint,overwrite
#ifdef SEGELMACHER_ISOLIERTE_ABNAHME
                                     ,reference_path,reference_hash
#endif
                                     );
        return run_options(path,args[0]=="pruefen",limit,fingerprint,overwrite);
    } catch(const std::exception& e) {
        int code=4;
        if(dynamic_cast<const J::exception*>(&e)||dynamic_cast<const std::invalid_argument*>(&e))code=2;
        if(dynamic_cast<const std::bad_alloc*>(&e))code=3;
        if(dynamic_cast<const fs::filesystem_error*>(&e))code=5;
        if(auto err=dynamic_cast<const Error*>(&e))code=err->code==6?3:err->code;
        Logger{}.log("Abschluss",e.what(),"FEHLER");
        return {{"status","fehlgeschlagen"},{"exitcode",code},{"ergebnisordner",nullptr},{"pruefbericht",nullptr},{"meldung",e.what()}};
    }
}
}
