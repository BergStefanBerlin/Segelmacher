#include "segel/core.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <set>
#include <stdexcept>
#include <vector>

namespace segel {
namespace {
using J = nlohmann::json;
constexpr double pi = 3.14159265358979323846;
using V = std::array<double, 3>;
V operator+(V a, V b) { return {a[0]+b[0],a[1]+b[1],a[2]+b[2]}; }
V operator-(V a, V b) { return {a[0]-b[0],a[1]-b[1],a[2]-b[2]}; }
V operator*(V a, double s) { return {a[0]*s,a[1]*s,a[2]*s}; }
V operator/(V a, double s) { return a*(1/s); }
double dot(V a,V b) { return a[0]*b[0]+a[1]*b[1]+a[2]*b[2]; }
V cross(V a,V b) { return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]}; }
double norm(V a) { return std::sqrt(dot(a,a)); }
V unit(V a) { double n=norm(a); if(!std::isfinite(n)||n<1e-14) throw std::runtime_error("Degenerierter Richtungsvektor"); return a/n; }
void require(bool b,const std::string& msg) { if(!b) throw std::invalid_argument(msg); }
void fields(const J& j,std::initializer_list<const char*> names,const char* context) {
    require(j.is_object(),std::string(context)+": Objekt erforderlich");
    std::set<std::string> allowed;for(auto name:names)allowed.insert(name);
    for(auto it=j.begin();it!=j.end();++it)require(allowed.count(it.key())!=0,std::string(context)+": Unbekanntes Feld "+it.key());
}
double number(const J& j,const char* key) {
    require(j.contains(key)&&j.at(key).is_number(),std::string("Zahlenfeld fehlt: ")+key);
    double v=j.at(key).get<double>(); require(std::isfinite(v),std::string("Nicht endlich: ")+key); return v;
}
J parse(std::string_view s) {
    std::vector<std::set<std::string>> keys;
    return J::parse(s, [&keys](int, J::parse_event_t event,J& v) {
        if(event==J::parse_event_t::object_start) keys.emplace_back();
        if(event==J::parse_event_t::key)
            require(keys.back().insert(v.get<std::string>()).second,"Doppelter JSON-Schluessel: "+v.get<std::string>());
        if(event==J::parse_event_t::object_end) keys.pop_back();
        return true;
    });
}
double smooth(double a,double b,double x) {
    double t=std::clamp((x-a)/(b-a),0.,1.); return t*t*(3-2*t);
}
struct Params {
    double stall0,stall1,shift,transition,width,k,cn,remin,remax;
    explicit Params(const J& j):
        stall0(number(j,"stall_start_deg")),stall1(number(j,"stall_end_deg")),
        shift(number(j,"stall_re_shift_deg")),transition(number(j,"transition_re")),
        width(number(j,"transition_width")),k(number(j,"viscous_cl2")),
        cn(number(j,"normal_pressure")),
        remin(number(j,"re_min")),remax(number(j,"re_max")) {
        fields(j,{"stall_start_deg","stall_end_deg","stall_re_shift_deg","transition_re","transition_width",
                  "viscous_cl2","normal_pressure","re_min","re_max"},"parameter");
        require(stall0>=1&&stall1>stall0&&stall1<=45&&std::abs(shift)<=5,
                "parameter: Abrisswinkel ausserhalb H1");
        require(transition>=1e4&&transition<=1e7&&width>0&&width<=1&&k>=0&&k<=1&&cn>0&&cn<=4,
                "parameter: viskose/Hochwinkelparameter ungueltig");
        require(remin>=1000&&remax<=1e8&&remax>remin,"parameter: Re-Bereich ungueltig");
    }
    void recheck(double re) const { require(std::isfinite(re)&&re>=remin&&re<=remax,"Lokale Reynolds-Zahl ausserhalb Modellbereich: "+std::to_string(re)); }
    double friction(double re) const {
        recheck(re);
        double t=smooth(transition*(1-width),transition*(1+width),re);
        return (1-t)*1.328/std::sqrt(re)+t*0.074/std::pow(re,0.2);
    }
    double attached(double a,double re) const {
        double delta=shift*std::log10(re/transition);
        return 1-smooth(stall0+delta,stall1+delta,std::abs(a));
    }
};
struct Shape { double m,p,twist; };
void checkshape(Shape s) {
    require(std::isfinite(s.m)&&std::isfinite(s.p)&&std::isfinite(s.twist)&&
            std::abs(s.m)<=0.2&&s.p>=0.05&&s.p<=0.95&&std::abs(s.twist)<=45,
            "Profil: m/p/Twist ausserhalb Modellbereich");
}
double camber(Shape s,double x) {
    return x<=s.p?s.m*(2*s.p*x-x*x)/(s.p*s.p):
        s.m*((1-2*s.p)+2*s.p*x-x*x)/((1-s.p)*(1-s.p));
}
std::array<double,3> fourier(Shape s,int n) {
    double mean=0,a1=0,a2=0;
    for(int i=0;i<n;++i) {
        double t=pi*(i+0.5)/n,x=(1-std::cos(t))/2;
        double slope= x<=s.p?2*s.m*(s.p-x)/(s.p*s.p):
            2*s.m*(s.p-x)/((1-s.p)*(1-s.p));
        mean+=slope/n; a1+=2*slope*std::cos(t)/n; a2+=2*slope*std::cos(2*t)/n;
    }
    return {mean-a1/2,a1,a2};
}
std::array<double,3> analytic_fourier(Shape s) {
    double theta=std::acos(1-2*s.p);
    auto primitive=[](int n,double t) {return n==0?t:std::sin(n*t)/n;};
    auto integral=[&](int n,double a,double b,double divisor) {
        auto diff=[&](int k){return primitive(k,b)-primitive(k,a);};
        return 2*s.m/divisor*((s.p-.5)*diff(n)+.25*(diff(n-1)+diff(n+1)));
    };
    std::array<double,3> coeff{};
    for(int n=0;n<3;++n) coeff[n]=(integral(n,0,theta,s.p*s.p)+integral(n,theta,pi,(1-s.p)*(1-s.p)))/pi;
    return {coeff[0]-coeff[1],2*coeff[1],2*coeff[2]};
}
struct Section {
    double z,front,chord,dz;Shape shape;
    std::array<double,3> coeff{};
    V quarter{};
    std::array<V,4> positions{},normals{};
    std::array<double,4> areas{};
};
struct G {
    std::string id;
    std::vector<V> vertices;
    std::vector<std::pair<double,Shape>> stations;
    double area=0,height=0,base=0,cref=0;
    double step,refine,eta;
    std::map<int,std::vector<Section>> grids;
    G(const J& geom,const J& numerik) {
        require(geom.at("schema_version")=="0.1","Geometrie.schema_version nicht unterstuetzt");
        id=geom.at("segel").at("id").get<std::string>();
        require(!id.empty(),"segel.id leer");
        require(geom.at("grundkontur").at("punkte").size()>=3&&geom.at("grundkontur").at("punkte").size()<=4096,
                "Konturpunktzahl ausserhalb 3..4096");
        std::map<std::string,V> points;
        for(const auto& p:geom.at("grundkontur").at("punkte")) {
            std::string key=p.at("id");
            auto v=p.at("position_m").get<V>();
            require(std::isfinite(v[0])&&std::isfinite(v[1])&&std::isfinite(v[2])&&v[1]==0,
                    "grundkontur.punkte."+key+": keine endliche XZ-Position");
            require(points.emplace(key,v).second,"Doppelte Punkt-ID: "+key);
        }
        require(points.count("hals")&&norm(points.at("hals"))<1e-12,"Hals nicht im Ursprung");
        std::vector<std::string> path;
        std::set<std::string> names;
        for(const auto& e:geom.at("grundkontur").at("rand").at("kanten")) {
            require(names.insert(e.at("name")).second,"Doppelter Kantenname");
            auto ids=e.at("punkt_ids").get<std::vector<std::string>>();
            require(ids.size()>=2,"Randkante zu kurz");
            if(!path.empty()) require(path.back()==ids.front(),"Randkanten nicht verbunden");
            for(size_t i=0;i<ids.size();++i) {
                require(points.count(ids[i])!=0,"Ungueltige Punktreferenz: "+ids[i]);
                if(i==0&&!path.empty()) continue;
                path.push_back(ids[i]);
            }
        }
        require(path.size()>=4&&path.front()==path.back(),"Rand nicht geschlossen");
        path.pop_back();
        std::set<std::string> used;
        for(const auto& key:path) {
            require(used.insert(key).second,"Randpunkt wiederholt: "+key);
            vertices.push_back(points.at(key));
        }
        require(used.size()==points.size(),"Unbenutzter Konturpunkt");
        auto orient=[](V a,V b,V c) { return (b[0]-a[0])*(c[2]-a[2])-(b[2]-a[2])*(c[0]-a[0]); };
        auto between=[](double a,double b,double x) { return x>=std::min(a,b)-1e-12&&x<=std::max(a,b)+1e-12; };
        for(size_t i=0;i<vertices.size();++i) {
            V a=vertices[i],b=vertices[(i+1)%vertices.size()];
            require(norm(b-a)>1e-12,"Randkante mit Laenge null");
            for(size_t j=i+1;j<vertices.size();++j) {
                if(j==i+1||(i==0&&j+1==vertices.size())) continue;
                V c=vertices[j],d=vertices[(j+1)%vertices.size()];
                double o1=orient(a,b,c),o2=orient(a,b,d),o3=orient(c,d,a),o4=orient(c,d,b);
                bool hit=(o1*o2<0&&o3*o4<0);
                for(auto pair:std::array<std::pair<V,double>,2>{{{c,o1},{d,o2}}})
                    if(std::abs(pair.second)<1e-12&&between(a[0],b[0],pair.first[0])&&between(a[2],b[2],pair.first[2])) hit=true;
                for(auto pair:std::array<std::pair<V,double>,2>{{{a,o3},{b,o4}}})
                    if(std::abs(pair.second)<1e-12&&between(c[0],d[0],pair.first[0])&&between(c[2],d[2],pair.first[2])) hit=true;
                require(!hit,"Selbstschneidender Rand");
            }
        }
        base=vertices[0][2]; double top=base;
        std::set<double> levels;
        for(size_t i=0;i<vertices.size();++i) {
            V a=vertices[i],b=vertices[(i+1)%vertices.size()];
            area+=a[0]*b[2]-b[0]*a[2];
            base=std::min(base,a[2]);top=std::max(top,a[2]); levels.insert(a[2]);
        }
        area=std::abs(area)/2;height=top-base;
        require(std::isfinite(area)&&std::isfinite(height)&&area>1e-10&&height>1e-8,"Konturflaeche/Hoehe nicht endlich/positiv"); cref=area/height;
        require(std::isfinite(cref)&&cref>0,"Referenzlaenge nicht darstellbar");
        require(geom.at("flaechenform").at("profilschnitte").at("stationen").size()<=1024,"Zu viele Profilstationen");
        for(const auto& s:geom.at("flaechenform").at("profilschnitte").at("stationen")) {
            fields(s,{"eta","woelbungsverhaeltnis","woelbungsposition_relativ","verwindung_deg"},"Profilstation");
            double e=number(s,"eta");
            Shape sh{number(s,"woelbungsverhaeltnis"),number(s,"woelbungsposition_relativ"),number(s,"verwindung_deg")};
            checkshape(sh);
            require(e>=0&&e<=1&&(stations.empty()||e>stations.back().first),"Profilstationen nicht strikt aufsteigend");
            stations.emplace_back(e,sh);levels.insert(base+height*e);
        }
        require(stations.size()>=2&&stations.front().first==0&&stations.back().first==1,"Profilabdeckung eta 0..1 fehlt");
        if(geom.at("segel").value("typ","")=="grosssegel") {
            bool flat=false;
            for(const auto& e:geom.at("grundkontur").at("rand").at("kanten"))
                if(e.at("name")=="unterliek") {
                    flat=true;
                    for(const auto& key:e.at("punkt_ids"))
                        flat=flat&&std::abs(points.at(key.get<std::string>())[2]-base)<1e-12;
                }
            if(flat) require(stations.front().second.m==0&&stations.front().second.twist==0,"Gerades Baum-Unterliek muss ungewolbt/unverdreht sein");
        }
        const auto& r=numerik.at("hoehenraster");
        require(numerik.at("streifenintegration")=="mittelpunktregel","Streifenintegration nicht unterstuetzt");
        step=number(r,"schritt_m");refine=number(r,"verfeinerter_schritt_m");eta=number(r,"verfeinern_ab_eta");
        require(step>0&&refine>0&&eta>=0&&eta<=1&&r.at("kontur_und_profilstationen_einfuegen")==true&&
                r.at("nullsehnen_als_punkte_behandeln")==true,"hoehenraster ungueltig");
        levels.insert(base+eta*height);
        std::vector<double> bounds(levels.begin(),levels.end());
        for(size_t i=1;i<bounds.size();++i) slice((bounds[i-1]+bounds[i])/2);
        for(int factor:{1,2,4}) {
            auto& out=grids[factor];
            for(size_t i=1;i<bounds.size();++i) {
                double a=bounds[i-1],b=bounds[i],h=(a>=base+eta*height-1e-12?refine:step)/factor;
                double count=std::ceil((b-a)/h);
                require(count<=10000,"hoehenraster: zu viele Streifen");
                int n=static_cast<int>(count);
                for(int k=0;k<n;++k) {
                    double dz=(b-a)/n,z=a+(k+0.5)*dz;
                    auto s=section(z);s.dz=dz;s.coeff=analytic_fourier(s.shape);s.quarter=point(z,.25);
                    for(int panel=0;panel<4;++panel) {
                        double xi=(panel+.5)/4,metric=0;
                        s.positions[panel]=point(z,xi);s.normals[panel]=normal(z,xi,&metric);
                        s.areas[panel]=metric*dz/4;
                    }
                    out.push_back(s);
                }
            }
            require(out.size()<=10000,"hoehenraster: zu viele Streifen");
        }
    }
    std::pair<double,double> slice(double z) const {
        std::vector<double> xs;
        for(size_t i=0;i<vertices.size();++i) {
            V a=vertices[i],b=vertices[(i+1)%vertices.size()];
            if((a[2]<=z&&z<b[2])||(b[2]<=z&&z<a[2]))
                xs.push_back(a[0]+(z-a[2])*(b[0]-a[0])/(b[2]-a[2]));
        }
        if(z==base+height) {
            for(V a:vertices) if(a[2]==z) xs.push_back(a[0]);
            std::sort(xs.begin(),xs.end());
            if(xs.size()==1) xs.push_back(xs[0]);
        }
        require(xs.size()==2,"Horizontalschnitt kein zusammenhaengendes Intervall, z="+std::to_string(z));
        return {*std::min_element(xs.begin(),xs.end()),*std::max_element(xs.begin(),xs.end())};
    }
    Section section(double z) const {
        auto range=slice(z); double eta0=(z-base)/height;
        size_t i=1;while(i+1<stations.size()&&eta0>stations[i].first) ++i;
        const auto& a=stations[i-1];const auto& b=stations[i];
        double t=(eta0-a.first)/(b.first-a.first);
        Shape s{a.second.m+(b.second.m-a.second.m)*t,a.second.p+(b.second.p-a.second.p)*t,
                a.second.twist+(b.second.twist-a.second.twist)*t};
        return {z,range.second,range.second-range.first,0,s};
    }
    V point(double z,double xi) const {
        auto s=section(z);double t=s.shape.twist*pi/180,x=-s.chord*xi,y=s.chord*camber(s.shape,xi);
        return {s.front+x*std::cos(t)-y*std::sin(t),x*std::sin(t)+y*std::cos(t),z};
    }
    V normal(double z,double xi,double* metric=nullptr) const {
        double h=height*1e-6,xh=1e-5;
        double x0=std::max(0.,xi-xh),x1=std::min(1.,xi+xh);
        double z0=std::max(base,z-h),z1=std::min(base+height,z+h);
        V chord=point(z,x1)-point(z,x0);
        V span=point(z1,xi)-point(z0,xi);
        if(metric)*metric=norm(cross(span/(z1-z0),chord/(x1-x0)));
        return unit(cross(span,chord)*-1);
    }
};
struct RigidTransform {
    std::array<V,3> rotation{{V{1,0,0},V{0,1,0},V{0,0,1}}};
    V translation{0,0,0};
    V vector(V v) const {return {dot(rotation[0],v),dot(rotation[1],v),dot(rotation[2],v)};}
    V point(V v) const {return vector(v)+translation;}
};
struct Surface {
    std::string id;
    std::shared_ptr<const G> intrinsic;
    RigidTransform placement;
};
struct Lifting {
    std::vector<double> modes;
    double height=0,base=0,speed=0;
    V normal{0,1,0};
    std::pair<double,V> at(double z) const {
        double theta=std::acos(std::clamp(1-2*(z-base)/height,-1.,1.));
        double circulation=0,induced=0;
        for(size_t n=0;n<modes.size();++n) {
            double term=modes[n]*std::sin((n+1)*theta);
            circulation+=term;induced+=(n+1)*term/std::sin(theta);
        }
        return {2*height*speed*circulation,normal*(-speed*induced)};
    }
};
struct LiftingFactor {
    std::vector<std::vector<double>> lu;
    std::vector<size_t> pivots;
    explicit LiftingFactor(const G& g,int n):lu(n,std::vector<double>(n)),pivots(n) {
        for(int i=0;i<n;++i) {
            double theta=pi*(i+.5)/n,z=g.base+g.height*(1-std::cos(theta))/2;
            auto s=g.section(z);
            for(int j=0;j<n;++j) {
                lu[i][j]=std::sin((j+1)*theta)*
                    (4*g.height/(2*pi*s.chord)+(j+1)/std::sin(theta));
            }
        }
        for(size_t k=0;k<lu.size();++k) {
            size_t p=k;
            for(size_t i=k+1;i<lu.size();++i)
                if(std::abs(lu[i][k])>std::abs(lu[p][k]))p=i;
            require(std::abs(lu[p][k])>1e-10,"Wirbelmatrix singulaer");
            pivots[k]=p;
            std::swap(lu[k],lu[p]);
            for(size_t i=k+1;i<lu.size();++i) {
                lu[i][k]/=lu[k][k];
                for(size_t j=k+1;j<lu.size();++j)lu[i][j]-=lu[i][k]*lu[k][j];
            }
        }
    }
    std::vector<double> solve(std::vector<double> b) const {
        for(size_t k=0;k<lu.size();++k)std::swap(b[k],b[pivots[k]]);
        for(size_t i=0;i<lu.size();++i)
            for(size_t j=0;j<i;++j)b[i]-=lu[i][j]*b[j];
        for(size_t ii=lu.size();ii>0;--ii) {
            size_t i=ii-1;
            for(size_t j=i+1;j<lu.size();++j)b[i]-=lu[i][j]*b[j];
            b[i]/=lu[i][i];
            require(std::isfinite(b[i]),"Wirbelloesung nicht endlich");
        }
        return b;
    }
};
Lifting lifting(const G& g,V u,const LiftingFactor& factor) {
    size_t n=factor.lu.size();
    std::vector<double> b(n);
    for(size_t i=0;i<n;++i) {
        double theta=pi*(i+.5)/n,z=g.base+g.height*(1-std::cos(theta))/2;
        auto s=g.section(z);auto f=analytic_fourier(s.shape);
        b[i]=std::atan2(u[1],-u[0])+s.shape.twist*pi/180-f[0];
    }
    return {factor.solve(std::move(b)),g.height,g.base,std::hypot(u[0],u[1]),unit(cross(u,V{0,0,1}))};
}
J representation(V f,V m,double cref) {
    for(double x:f)require(std::isfinite(x),"Resultierende Kraft nicht darstellbar");
    for(double x:m)require(std::isfinite(x),"Resultierendes Moment nicht darstellbar");
    double fn=norm(f);require(std::isfinite(fn*fn),"Resultierende Kraft zu gross");J point=nullptr;V rest=m;
    if(fn>1e-10) { V r=cross(f,m)*(cref/(fn*fn));point=r;rest=m-cross(r/cref,f); }
    if(!point.is_null())for(double x:point)require(std::isfinite(x),"Wirkungslinie nicht darstellbar");
    for(double x:rest)require(std::isfinite(x),"Restmoment nicht darstellbar");
    return {{"CF",f},{"CM",m},{"point",point},{"rest",rest},{"valid",fn>1e-10}};
}
J integrate(const J& loads,double cref) {
    require(std::isfinite(cref)&&cref>0,"c_ref muss positiv sein");
    V f{0,0,0},m{0,0,0};
    for(const auto& l:loads) {
        V fi=l.at("f").get<V>(),r=l.at("r").get<V>(),mi=l.at("m").get<V>();
        for(double x:fi)require(std::isfinite(x),"Last nicht endlich");
        for(double x:r)require(std::isfinite(x),"Ansatz nicht endlich");
        for(double x:mi)require(std::isfinite(x),"Moment nicht endlich");
        f=f+fi;m=m+cross(r/cref,fi)+mi;
    }
    return representation(f,m,cref);
}
}
struct Context::Impl {
    Params params;std::shared_ptr<const G> intrinsic;const G& g;
    std::map<int,LiftingFactor> lifting_factors;
    std::vector<Surface> surfaces;J description;
    Impl(const J& j):params(j.at("parameter")),
        intrinsic(std::make_shared<G>(j.at("geometrie"),j.at("numerik"))),g(*intrinsic),
        surfaces{{g.id,intrinsic,{}}} {
        fields(j,{"parameter","geometrie","numerik"},"Context");
        for(int modes:{12,16,24,32,48,64})lifting_factors.emplace(modes,LiftingFactor(g,modes));
        description={{"segel_id",g.id},{"S_m2",g.area},{"c_ref_m",g.cref},{"hoehe_m",g.height},{"grids",J::object()}};
        for(const auto& grid:g.grids) {
            J rows=J::array();int i=0;
            for(const auto& s:grid.second) {
                auto f=fourier(s.shape,256),fine=fourier(s.shape,512);
                require(std::abs(f[0]-fine[0])<1e-5&&std::abs(f[1]-fine[1])<1e-5&&std::abs(f[2]-fine[2])<1e-5,
                        "Fourier-Providerkonvergenz nicht erreicht");
                auto exact=analytic_fourier(s.shape);
                for(int k=0;k<3;++k)require(std::abs(fine[k]-exact[k])<1e-5,"Analytische Fourier-Referenz nicht erreicht");
                rows.push_back({{"profil_id","r"+std::to_string(grid.first)+"p"+std::to_string(i++)},
                    {"z",s.z},{"front",s.front},{"chord",s.chord},{"dz",s.dz},
                    {"m",s.shape.m},{"p",s.shape.p},{"twist",s.shape.twist},{"a0",exact[0]},
                    {"cm",-pi*(exact[2]-exact[1])/4}});
            }
            description["grids"][std::to_string(grid.first)]=rows;
        }
    }
    J solve(double alpha,double beta,double re,int factor,bool keep,int panels=16) const {
        require(std::isfinite(alpha)&&std::isfinite(beta)&&beta>=0&&beta<=90,"Windwinkel ungueltig");
        params.recheck(re);
        require(g.grids.count(factor)!=0,"Rasterfaktor muss 1,2 oder 4 sein");
        require(panels==12||panels==16||panels==24||panels==32||panels==48||panels==64,
                "Wirbelfeldzahl muss 12, 16, 24, 32, 48 oder 64 sein");
        double a=std::remainder(alpha,360.)*pi/180,b=beta*pi/180;
        V u=beta==90?V{0,0,-1}:V{-std::cos(b)*std::cos(a),-std::cos(b)*std::sin(a),-std::sin(b)};
        double elevweight=1-smooth(60,80,beta);
        double maxweight=0;
        for(const auto& s:g.grids.at(factor)) {
            double localre=re*s.chord/g.cref;params.recheck(localre);
            double angle=std::remainder(alpha-s.shape.twist,360.);
            maxweight=std::max(maxweight,params.attached(angle,localre)*elevweight);
        }
        Lifting system;
        if(maxweight>0) system=lifting(g,u,lifting_factors.at(panels));
        J loads=J::array(); int id=0;V sumforce{0,0,0},summoment{0,0,0};
        auto contribute=[&](V f,V r,V m,const char* kind) {
            const auto& surface=surfaces.front();
            f=surface.placement.vector(f);r=surface.placement.point(r);m=surface.placement.vector(m);
            sumforce=sumforce+f;summoment=summoment+cross(r/g.cref,f)+m;
            if(keep)loads.push_back({{"id",id++},{"f",f},{"r",r},{"m",m},{"segel_id",surface.id},{"kind",kind}});
        };
        for(const auto& s:g.grids.at(factor)) {
            double localre=re*s.chord/g.cref;
            double angle=std::remainder(alpha-s.shape.twist,360.);
            double w=params.attached(angle,localre)*elevweight;
            double cf=params.friction(localre);
            if(w>0) {
                auto induced=system.at(s.z);double gamma=induced.first;
                V pos=s.quarter,ind=induced.second,span{0,0,1};
                V f=cross(u+ind,span)*(2*gamma*s.dz/g.area*w);
                double cm=pi*(s.coeff[2]-s.coeff[1])/4;
                V m=span*(-cm*s.chord*s.chord*s.dz/g.area/g.cref*std::cos(b)*std::cos(b)*w);
                contribute(f,pos,m,"gebunden_plus_profilmoment");
            }
            double cl=2*pi*(-angle*pi/180-s.coeff[0]);
            for(int k=0;k<4;++k) {
                double ds=s.areas[k];
                V n=s.normals[k],pos=s.positions[k];
                double un=dot(u,n);
                V tang=u-n*un;
                double tangspeed=norm(tang);
                V pressure=n*(params.cn*un*std::abs(un)*(1-w)*ds/g.area);
                double drag=2*cf+params.k*cl*cl*w*std::cos(b)*std::cos(b);
                V friction=tang*(drag*tangspeed*ds/g.area);
                contribute(pressure+friction,pos,V{0,0,0},"mittelflaechen_impuls_reibung");
            }
        }
        J result=representation(sumforce,summoment,g.cref);
        V f=result.at("CF").get<V>(),m=result.at("CM").get<V>();
        for(double x:f)require(std::isfinite(x),"Kraft nicht endlich");
        for(double x:m)require(std::isfinite(x),"Moment nicht endlich");
        require(dot(f,u)>=-1e-8,"Negativer Gesamtwiderstand: alpha="+std::to_string(alpha)+
                ", beta="+std::to_string(beta)+", CD="+std::to_string(dot(f,u)));
        if(keep)result["loads"]=loads;
        return result;
    }
};
Context::Context(std::string_view input):impl_(std::make_unique<Impl>(parse(input))) {}
Context::~Context()=default;
std::string Context::geometry() const { return impl_->description.dump(); }
std::string Context::solve(double a,double b,double re,int factor,bool loads,int panels) const {
    return impl_->solve(a,b,re,factor,loads,panels).dump();
}
std::string polar(std::string_view input) {
    J j=parse(input); Params p(j.at("parameter"));
    fields(j,{"m","p","alpha_deg","re","parameter"},"polar");
    Shape s{number(j,"m"),number(j,"p"),0};checkshape(s);
    double alpha=std::remainder(number(j,"alpha_deg"),360.),re=number(j,"re");p.recheck(re);
    auto c=analytic_fourier(s);double cl=2*pi*(-alpha*pi/180-c[0]);
    double w=p.attached(alpha,re),sina=std::sin(-alpha*pi/180);
    double normal=p.cn*sina*std::abs(sina);
    return J{{"CL",w*cl+(1-w)*normal*std::cos(alpha*pi/180)},
        {"CD",2*p.friction(re)+w*p.k*cl*cl+(1-w)*std::abs(normal*sina)},
        {"CM",-w*pi*(c[2]-c[1])/4},{"a0",c[0]},{"A1",c[1]},{"A2",c[2]}}.dump();
}
std::string resultant(std::string_view input) {
    J j=parse(input);return integrate(j.at("loads"),number(j,"c_ref")).dump();
}
std::string transform(std::string_view input) {
    J j=parse(input);auto r=j.at("rotation").get<std::array<V,3>>();V t=j.at("translation").get<V>();
    for(int i=0;i<3;++i)for(int k=0;k<3;++k)
        require(std::isfinite(dot(r[i],r[k]))&&std::abs(dot(r[i],r[k])-(i==k?1:0))<1e-10,"Rotation nicht orthonormal");
    require(dot(cross(r[0],r[1]),r[2])>1-1e-10,"Rotation ist Spiegelung");
    for(double x:t)require(std::isfinite(x),"Translation nicht endlich");
    RigidTransform placement{r,t};
    J loads=j.at("loads");
    for(auto& l:loads) {
        l["f"]=placement.vector(l.at("f").get<V>());l["m"]=placement.vector(l.at("m").get<V>());
        l["r"]=placement.point(l.at("r").get<V>());
        for(const auto* name:{"f","m","r"})for(double x:l[name])require(std::isfinite(x),"Transformierte Last nicht darstellbar");
    }
    return loads.dump();
}
}
