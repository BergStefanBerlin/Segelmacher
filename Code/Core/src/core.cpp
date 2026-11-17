#include "segel/core.hpp"
#include "segelform_internal.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>
#include <limits>
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
    G(const J& geom,const J& numerik,bool split_reference=false) {
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
        if(split_reference) {
            double count=std::ceil(height/detail::max_output_strip_height_m);
            require(count>=1&&count<=10000,"Anzahl der Referenzstreifen ungueltig");
            for(int i=1;i<static_cast<int>(count);++i) levels.insert(base+height*i/count);
        }
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
J strip_geometry(const G& g,int subdivision) {
    require(subdivision==1||subdivision==2||subdivision==4,"Streifenunterteilung muss 1, 2 oder 4 sein");
    const double strip_count=std::ceil(g.height/detail::max_output_strip_height_m)*subdivision;
    require(strip_count>=1&&strip_count<=10000,"Anzahl der Ausgabestreifen ausserhalb 1..10000");
    const int count=static_cast<int>(strip_count);
    J strips=J::array(), quadrature=J::array(), points=J::array();
    int total_points=0;
    double total_area=0;
    for(int id=0;id<count;++id) {
        const double lower=g.base+g.height*id/count;
        const double upper=g.base+g.height*(id+1)/count;
        const double mid=(lower+upper)/2;
        const auto middle=g.section(mid);
        require(middle.chord>1e-12,"Keine eindeutige positive Sehne fuer Streifen "+std::to_string(id));
        const V query=g.point(mid,0.25);
        std::set<double> boundaries{lower,upper};
        auto insert_boundary=[&](double z) {
            double tolerance=std::min((upper-lower)*1e-12,
                8*std::numeric_limits<double>::epsilon()*
                std::max({1.0,std::abs(lower),std::abs(upper),std::abs(z)}));
            if(z-lower<=tolerance||upper-z<=tolerance)return;
            auto next=boundaries.lower_bound(z);
            if(next!=boundaries.end()&&*next-z<=tolerance)return;
            if(next!=boundaries.begin()&&z-*std::prev(next)<=tolerance)return;
            boundaries.insert(z);
        };
        for(const auto& v:g.vertices)
            insert_boundary(v[2]);
        for(const auto& station:g.stations) {
            double z=g.base+g.height*station.first;
            insert_boundary(z);
        }
        double refinement=g.base+g.eta*g.height;
        insert_boundary(refinement);
        std::vector<double> levels(boundaries.begin(),boundaries.end());
        double area=0;
        int point_id=0;
        for(size_t k=1;k<levels.size();++k) {
            double a=levels[k-1],b=levels[k];
            double step=(a>=refinement-1e-12?g.refine:g.step)/16;
            double intervals=std::ceil((b-a)/step);
            require(intervals>=1&&intervals<=10000,"Quadraturpunktzahl ungueltig");
            int n=static_cast<int>(intervals);
            for(int j=0;j<n;++j) {
                require(++total_points<=10000,"Zu viele Quadraturpunkte insgesamt");
                double dz=(b-a)/n,z=a+(j+0.5)*dz;
                auto section=g.section(z);
                require(section.chord>0,"Quadratursehne ungueltig");
                auto coeff=analytic_fourier(section.shape);
                V quarter=g.point(z,0.25);
                quadrature.push_back({{"streifen_id",id},{"punkt_id",point_id},
                    {"z_m",z},{"dz_m",dz},{"chord_m",section.chord},
                    {"m",section.shape.m},{"p",section.shape.p},
                    {"twist_rad",section.shape.twist*pi/180},
                    {"a0",coeff[0]},{"a1",coeff[1]},{"a2",coeff[2]},
                    {"viertel_x_m",quarter[0]},{"viertel_y_m",quarter[1]},{"viertel_z_m",quarter[2]}});
                for(int panel=0;panel<4;++panel) {
                    double xi=(panel+0.5)/4,metric=0;
                    V position=g.point(z,xi),normal=g.normal(z,xi,&metric);
                    require(std::isfinite(metric)&&std::isfinite(metric*dz/4)&&metric>0,
                            "Ungueltige Flaechenpunktmetrik");
                    points.push_back({{"streifen_id",id},{"punkt_id",point_id},{"teil_id",panel},
                        {"position_x_m",position[0]},{"position_y_m",position[1]},{"position_z_m",position[2]},
                        {"normal_x",normal[0]},{"normal_y",normal[1]},{"normal_z",normal[2]},
                        {"flaeche_m2",metric*dz/4}});
                }
                area+=section.chord*dz;
                ++point_id;
            }
        }
        require(std::isfinite(area)&&area>0,"Ungueltige Streifenflaeche");
        total_area+=area;
        strips.push_back({{"streifen_id",id},{"z_min_m",lower},{"z_max_m",upper},
            {"flaeche_m2",area},{"abfrage_x_m",query[0]},{"abfrage_y_m",query[1]},{"abfrage_z_m",query[2]}});
    }
    require(std::abs(total_area-g.area)<=1e-10*std::max(1.0,g.area),
            "Streifenflaechen stimmen nicht mit Konturflaeche ueberein");
    return {{"segel_id",g.id},{"S_m2",g.area},{"c_ref_m",g.cref},{"z_min_m",g.base},
        {"hoehe_m",g.height},{"streifen",strips},{"quadratur",quadrature},{"flaechenpunkte",points}};
}
struct RigidTransform {
    std::array<V,3> rotation{{V{1,0,0},V{0,1,0},V{0,0,1}}};
    V translation{0,0,0};
    V vector(V v) const {return {dot(rotation[0],v),dot(rotation[1],v),dot(rotation[2],v)};}
    V point(V v) const {return vector(v)+translation;}
};
void validate_transform(const RigidTransform& transform) {
    const auto& rotation=transform.rotation;
    for(int i=0;i<3;++i)for(int k=0;k<3;++k)
        require(std::isfinite(dot(rotation[i],rotation[k]))&&
                std::abs(dot(rotation[i],rotation[k])-(i==k?1:0))<1e-10,
                "Rotation nicht orthonormal");
    require(dot(cross(rotation[0],rotation[1]),rotation[2])>1-1e-10,
            "Rotation ist Spiegelung");
    for(double x:transform.translation)require(std::isfinite(x),"Translation nicht endlich");
}
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
double coupling_value(double height,double chord,double theta,int column) {
    return std::sin((column+1)*theta)*(4*height/(2*pi*chord)+(column+1)/std::sin(theta));
}
struct LiftingFactor {
    std::vector<std::vector<double>> lu;
    std::vector<size_t> pivots;
    explicit LiftingFactor(const G& g,int n):lu(n,std::vector<double>(n)),pivots(n) {
        for(int i=0;i<n;++i) {
            double theta=pi*(i+.5)/n,z=g.base+g.height*(1-std::cos(theta))/2;
            auto s=g.section(z);
            require(s.chord>0,"Ungueltige Kollokationssehne");
            for(int j=0;j<n;++j) {
                lu[i][j]=coupling_value(g.height,s.chord,theta,j);
            }
        }
        factorize();
    }
    explicit LiftingFactor(std::vector<std::vector<double>> matrix)
        :lu(std::move(matrix)),pivots(lu.size()) {
        require(!lu.empty(),"Leere Wirbelmatrix");
        for(const auto& row:lu) require(row.size()==lu.size(),"Wirbelmatrix nicht quadratisch");
        factorize();
    }
    void factorize() {
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
J coupling_geometry(const G& g) {
    J collocation=J::array(),matrix=J::array();
    for(int modes:{32,48,64}) {
        for(int i=0;i<modes;++i) {
            double theta=pi*(i+0.5)/modes,z=g.base+g.height*(1-std::cos(theta))/2;
            auto section=g.section(z);
            require(section.chord>0,"Ungueltige Kollokationssehne");
            auto coeff=analytic_fourier(section.shape);
            collocation.push_back({{"moden",modes},{"index",i},{"z_m",z},{"chord_m",section.chord},
                {"twist_rad",section.shape.twist*pi/180},{"a0",coeff[0]}});
            for(int j=0;j<modes;++j) {
                double value=coupling_value(g.height,section.chord,theta,j);
                require(std::isfinite(value),"Nicht endlicher Kopplungsmatrixwert");
                matrix.push_back({{"moden",modes},{"zeile",i},{"spalte",j},{"wert",value}});
            }
        }
    }
    return {{"kollokation",collocation},{"kopplung",matrix}};
}
size_t index_field(const J& row,const char* key,size_t limit) {
    require(row.contains(key)&&row.at(key).is_number_integer(),std::string("Ganzzahliger Index fehlt: ")+key);
    if(row.at(key).is_number_unsigned()) {
        auto value=row.at(key).get<unsigned long long>();
        require(value<limit,std::string("Index ausserhalb Bereich: ")+key);
        return static_cast<size_t>(value);
    }
    auto value=row.at(key).get<long long>();
    require(value>=0&&static_cast<unsigned long long>(value)<limit,std::string("Index ausserhalb Bereich: ")+key);
    return static_cast<size_t>(value);
}
void validate_model_data(const J& model) {
    double height=number(model,"hoehe_m"),base=number(model,"z_min_m"),area=number(model,"S_m2");
    require(height>0&&area>0&&number(model,"c_ref_m")>0,"Ungueltige Modellgeometrie");
    const auto& strips=model.at("streifen");
    const auto& quadrature=model.at("quadratur");
    const auto& panels=model.at("flaechenpunkte");
    require(strips.is_array()&&!strips.empty()&&strips.size()<=10000,"Ungueltige Streifenzahl");
    require(quadrature.is_array()&&!quadrature.empty()&&quadrature.size()<=10000,"Ungueltige Quadraturzahl");
    require(panels.is_array()&&panels.size()==quadrature.size()*4,"Ungueltige Flaechenpunktzahl");
    double sum=0,previous=base,tolerance=1e-10*std::max(1.0,height);
    for(size_t i=0;i<strips.size();++i) {
        const auto& row=strips[i];
        require(index_field(row,"streifen_id",strips.size())==i,"Streifen-IDs nicht lueckenlos sortiert");
        double low=number(row,"z_min_m"),high=number(row,"z_max_m");
        require(high>low&&std::abs(low-previous)<=tolerance,"Streifenhoehen nicht lueckenlos");
        require(std::abs(low-(base+height*i/strips.size()))<=tolerance&&
                std::abs(high-(base+height*(i+1)/strips.size()))<=tolerance,
                "Streifenhoehen nicht gleichmaessig");
        double z=number(row,"abfrage_z_m");
        require(std::abs(z-(low+high)/2)<=tolerance,"Abfragepunkt nicht auf Streifenmitte");
        number(row,"abfrage_x_m");number(row,"abfrage_y_m");
        double strip_area=number(row,"flaeche_m2");
        require(strip_area>0,"Ungueltige Streifenflaeche");
        sum+=strip_area;previous=high;
    }
    require(std::abs(previous-(base+height))<=tolerance&&
            std::abs(sum-area)<=1e-10*std::max(1.0,area),"Streifengrenzen oder Flaechensumme ungueltig");
    std::vector<size_t> counts(strips.size());
    std::vector<double> quadrature_end(strips.size()),quadrature_area(strips.size());
    for(size_t i=0;i<strips.size();++i) quadrature_end[i]=number(strips[i],"z_min_m");
    size_t previous_id=0;
    for(size_t i=0;i<quadrature.size();++i) {
        const auto& q=quadrature[i];
        size_t id=index_field(q,"streifen_id",strips.size());
        require(i==0||id>=previous_id,"Quadratur nicht nach Streifen sortiert");
        size_t point=index_field(q,"punkt_id",quadrature.size());
        require(point==counts[id]++,"Quadraturpunkt-ID nicht lueckenlos");
        previous_id=id;
        double z=number(q,"z_m"),dz=number(q,"dz_m"),chord=number(q,"chord_m");
        double m=number(q,"m"),p=number(q,"p");
        double twist=number(q,"twist_rad");
        require(z>number(strips[id],"z_min_m")&&z<number(strips[id],"z_max_m")&&dz>0&&chord>0&&
                std::abs(m)<=0.2&&p>=0.05&&p<=0.95,"Ungueltige Quadraturgeometrie");
        Shape shape{m,p,twist*180/pi};
        checkshape(shape);
        require(std::abs(z-dz/2-quadrature_end[id])<=tolerance&&
                z+dz/2<=number(strips[id],"z_max_m")+tolerance,
                "Quadraturintervalle nicht lueckenlos oder ausserhalb des Streifens");
        quadrature_end[id]=z+dz/2;
        quadrature_area[id]+=chord*dz;
        auto coeff=analytic_fourier(shape);
        for(size_t k=0;k<coeff.size();++k) {
            double actual=number(q,k==0?"a0":k==1?"a1":"a2");
            require(std::abs(actual-coeff[k])<=1e-12*std::max(1.0,std::abs(coeff[k])),
                    "Fourier-Koeffizient stimmt nicht mit Profil ueberein");
        }
        double quarter_x=number(q,"viertel_x_m"),quarter_y=number(q,"viertel_y_m");
        require(std::abs(number(q,"viertel_z_m")-z)<=tolerance,"Ungueltiger Viertelsehnenpunkt");
        for(size_t k=0;k<4;++k) {
            const auto& panel=panels[4*i+k];
            require(index_field(panel,"streifen_id",strips.size())==id&&
                    index_field(panel,"punkt_id",quadrature.size())==point&&
                    index_field(panel,"teil_id",4)==k,"Ungueltige Flaechenpunktreferenz");
            V normal{number(panel,"normal_x"),number(panel,"normal_y"),number(panel,"normal_z")};
            require(std::abs(norm(normal)-1)<=1e-10&&number(panel,"flaeche_m2")>0,"Ungueltige Flaechenmetrik");
            double xi=(k+0.5)/4,dx=-chord*(xi-0.25),dy=chord*(camber(shape,xi)-camber(shape,0.25));
            double panel_tolerance=1e-10*std::max(1.0,chord);
            require(std::abs(number(panel,"position_x_m")-(quarter_x+dx*std::cos(twist)-dy*std::sin(twist)))<=panel_tolerance&&
                    std::abs(number(panel,"position_y_m")-(quarter_y+dx*std::sin(twist)+dy*std::cos(twist)))<=panel_tolerance&&
                    std::abs(number(panel,"position_z_m")-z)<=tolerance,
                    "Flaechenpunkt stimmt nicht mit Quadraturprofil ueberein");
        }
    }
    for(size_t i=0;i<counts.size();++i) {
        require(counts[i]>0,"Streifen ohne Quadratur");
        double strip_area=number(strips[i],"flaeche_m2");
        require(std::abs(quadrature_end[i]-number(strips[i],"z_max_m"))<=tolerance&&
                std::abs(quadrature_area[i]-strip_area)<=1e-10*std::max(1.0,strip_area),
                "Quadraturintegral stimmt nicht mit Streifengeometrie ueberein");
    }
    const auto& collocations=model.at("kollokation");
    const auto& matrix=model.at("kopplung");
    require(collocations.is_array()&&collocations.size()==144&&matrix.is_array()&&matrix.size()==7424,
            "Unvollstaendige Kopplungsdaten");
    size_t collocation_index=0,matrix_index=0;
    for(int modes:{32,48,64}) {
        std::vector<std::vector<double>> values(modes,std::vector<double>(modes));
        for(int i=0;i<modes;++i) {
            const auto& row=collocations[collocation_index++];
            require(index_field(row,"moden",65)==static_cast<size_t>(modes)&&
                    index_field(row,"index",modes)==static_cast<size_t>(i),"Ungueltige Kollokationsreihenfolge");
            double theta=pi*(i+0.5)/modes,z=base+height*(1-std::cos(theta))/2;
            require(std::abs(number(row,"z_m")-z)<=tolerance,"Ungueltige Kollokationshoehe");
            double chord=number(row,"chord_m");
            require(chord>0,"Ungueltige Kollokationssehne");
            number(row,"twist_rad");number(row,"a0");
            for(int j=0;j<modes;++j) {
                const auto& entry=matrix[matrix_index++];
                require(index_field(entry,"moden",65)==static_cast<size_t>(modes)&&
                        index_field(entry,"zeile",modes)==static_cast<size_t>(i)&&
                        index_field(entry,"spalte",modes)==static_cast<size_t>(j),"Ungueltige Matrixreihenfolge");
                double value=number(entry,"wert"),expected=coupling_value(height,chord,theta,j);
                require(std::abs(value-expected)<=1e-12*std::max(1.0,std::abs(expected)),
                        "Kopplungsmatrix stimmt nicht mit Modellgeometrie ueberein");
                values[i][j]=value;
            }
        }
        LiftingFactor factor(std::move(values));
    }
}
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
double speed(V v) { return std::hypot(std::hypot(v[0],v[1]),v[2]); }
double attached_local(const Params& p,double angle,double re) {
    double delta=re>=p.remin ? p.shift*std::log10(re/p.transition) :
        p.shift*(std::log10(p.remin/p.transition)+(re/p.remin-1)/std::log(10.));
    return 1-smooth(p.stall0+delta,p.stall1+delta,std::abs(angle));
}
J solve_strips(const Params& p,const J& model,const J& input,double rho,double nu,
               const std::vector<std::vector<double>>& matrix,const LiftingFactor& factor,int modes,
               bool include_contributions=false,J* lifting_diagnostics=nullptr,
               const RigidTransform* sdp_transform=nullptr) {
    require(std::isfinite(rho)&&rho>0&&std::isfinite(nu)&&nu>0,"Ungueltige Luftdichte oder Viskositaet");
    const double height=number(model,"hoehe_m"),base=number(model,"z_min_m");
    require(height>0,"Ungueltige Modellhoehe");
    const auto& strips=model.at("streifen");
    require(input.is_array()&&input.size()==strips.size(),"Anzahl der Windvektoren ungueltig");
    std::vector<V> winds(strips.size());
    std::vector<bool> present(strips.size(),false);
    for(const auto& item:input) {
        fields(item,{"streifen_id","wind_m_s"},"Wind");
        require(item.at("streifen_id").is_number_integer(),"Streifen-ID muss ganzzahlig sein");
        auto id=item.at("streifen_id").get<long long>();
        require(id>=0&&static_cast<size_t>(id)<winds.size()&&!present[static_cast<size_t>(id)],
                "Unbekannte oder doppelte Streifen-ID: "+std::to_string(id));
        V v=item.at("wind_m_s").get<V>();
        require(std::isfinite(v[0])&&std::isfinite(v[1])&&std::isfinite(v[2])&&speed(v)<=50,
                "Windvektor ungueltig oder schneller als 50 m/s");
        winds[static_cast<size_t>(id)]=v;
        present[static_cast<size_t>(id)]=true;
    }
    for(bool valid:present) require(valid,"Streifenwind fehlt");
    auto wind_at=[&](double z) {
        if(z<=strips.front().at("abfrage_z_m").get<double>()) return winds.front();
        if(z>=strips.back().at("abfrage_z_m").get<double>()) return winds.back();
        size_t hi=1;
        while(z>strips[hi].at("abfrage_z_m").get<double>()) ++hi;
        double low=strips[hi-1].at("abfrage_z_m").get<double>();
        double high=strips[hi].at("abfrage_z_m").get<double>();
        double t=(z-low)/(high-low);
        return winds[hi-1]*(1-t)+winds[hi]*t;
    };
    auto local_weight=[&](V u,double chord,double twist) {
        double v=speed(u);
        if(v==0) return 0.0;
        double re=v*chord/nu;
        require(std::isfinite(re)&&re<=p.remax,"Lokale Reynolds-Zahl oberhalb Modellbereich");
        double horizontal=std::hypot(u[0],u[1]);
        double alpha=horizontal==0?0:std::atan2(-u[1],-u[0])*180/pi;
        double beta=std::atan2(-u[2],horizontal)*180/pi;
        double angle=std::remainder(alpha-twist*180/pi,360.);
        return attached_local(p,angle,re)*(1-smooth(60,80,std::abs(beta)));
    };
    double max_horizontal=0,max_weight=0;
    for(V u:winds) max_horizontal=std::max(max_horizontal,std::hypot(u[0],u[1]));
    for(const auto& q:model.at("quadratur")) {
        V u=wind_at(q.at("z_m"));
        max_weight=std::max(max_weight,local_weight(u,q.at("chord_m"),q.at("twist_rad")));
    }
    std::vector<double> b(modes),coefficients(modes,0);
    bool lifting_solved=false;
    double residual_inf=0,residual_denominator=0;
    if(max_weight>0) {
        std::vector<bool> collocation_seen(modes,false);
        for(const auto& row:model.at("kollokation")) if(row.at("moden")==modes) {
            int i=row.at("index");
            require(i>=0&&i<modes&&!collocation_seen[i],"Ungueltiger Kollokationsindex");
            double z=number(row,"z_m"),twist=number(row,"twist_rad"),a0=number(row,"a0");
            require(number(row,"chord_m")>0,"Ungueltige Kollokationssehne");
            V u=wind_at(z);
            double horizontal=std::hypot(u[0],u[1]);
            max_horizontal=std::max(max_horizontal,horizontal);
            if(horizontal>0) {
                double phi=std::atan2(u[1],-u[0]);
                double gate=1-smooth(120,180,std::abs(phi)*180/pi);
                b[i]=horizontal*gate*(phi+twist-a0);
            }
            collocation_seen[i]=true;
        }
        for(bool seen:collocation_seen)require(seen,"Unvollstaendige Kollokation");
        if(max_horizontal>0) {
            coefficients=factor.solve(b);
            lifting_solved=true;
            double matrix_norm=0,coef_norm=0,b_norm=0;
            for(double x:coefficients) coef_norm=std::max(coef_norm,std::abs(x));
            for(int i=0;i<modes;++i) {
                double row=0,product=0;
                for(int j=0;j<modes;++j) {
                    double m=matrix[i][j];
                    row+=std::abs(m);product+=m*coefficients[j];
                }
                matrix_norm=std::max(matrix_norm,row);
                b_norm=std::max(b_norm,std::abs(b[i]));
                residual_inf=std::max(residual_inf,std::abs(product-b[i]));
            }
            residual_denominator=matrix_norm*coef_norm+b_norm;
            require(std::isfinite(residual_inf)&&std::isfinite(residual_denominator)&&
                    (residual_denominator==0?residual_inf==0:residual_inf/residual_denominator<=1e-10),
                    "Wirbelmatrix: Rueckwaertsfehler ueberschritten");
        }
    }
    if(lifting_diagnostics)
        *lifting_diagnostics={{"solved",lifting_solved},{"coefficients",coefficients},{"rhs",b},
            {"residual_inf",lifting_solved?J(residual_inf):J(nullptr)},
            {"residual_denominator",lifting_solved?J(residual_denominator):J(nullptr)},
            {"backward_error",lifting_solved?J(residual_denominator==0?0:residual_inf/residual_denominator):J(nullptr)}};
    std::vector<V> forces(strips.size()),moments(strips.size());
    J contributions=J::array();
    double sdp_q=0,sdp_b=0,sdp_x=0,sdp_z=0;
    const auto& surface=model.at("flaechenpunkte");
    size_t surface_index=0;
    for(const auto& q:model.at("quadratur")) {
        size_t id=q.at("streifen_id").get<size_t>();
        double z=q.at("z_m"),dz=q.at("dz_m"),chord=q.at("chord_m"),twist=q.at("twist_rad");
        V u=wind_at(z);
        double v=speed(u);
        if(v==0) { surface_index+=4;continue; }
        double re=v*chord/nu;
        require(std::isfinite(re)&&re<=p.remax,"Lokale Reynolds-Zahl oberhalb Modellbereich");
        double xy=std::hypot(u[0],u[1]);
        double alpha=xy==0?0:std::atan2(-u[1],-u[0])*180/pi;
        double angle=std::remainder(alpha-twist*180/pi,360.);
        double weight=local_weight(u,chord,twist);
        double cos_theta=std::clamp(1-2*(z-base)/height,-1.,1.);
        double theta=std::acos(cos_theta);
        double circulation=0,downwash=0;
        double previous=0,current=1;
        for(int j=0;j<modes;++j) {
            double term=coefficients[j]*std::sin((j+1)*theta);
            circulation+=term;
            downwash+=(j+1)*coefficients[j]*current;
            double next=2*cos_theta*current-previous;
            previous=current;current=next;
        }
        V induced{};
        if(xy>0&&max_horizontal>0) {
            double scale=xy>=0.01*max_horizontal?1:smooth(0,0.01*max_horizontal,xy);
            induced=V{u[1]/xy,-u[0]/xy,0}*(-downwash*scale);
        }
        V query{strips[id].at("abfrage_x_m"),strips[id].at("abfrage_y_m"),strips[id].at("abfrage_z_m")};
        auto add=[&](V f,V position,V intrinsic) {
            V moment=cross(position-query,f)+intrinsic;
            forces[id]=forces[id]+f;moments[id]=moments[id]+moment;
            if(sdp_transform) {
                const V transformed_force=sdp_transform->vector(f);
                const V transformed_position=sdp_transform->point(position);
                const double fy=transformed_force[1];
                sdp_q+=fy;
                sdp_b+=std::abs(fy);
                sdp_x+=transformed_position[0]*fy;
                sdp_z+=transformed_position[2]*fy;
                require(std::isfinite(sdp_q)&&std::isfinite(sdp_b)&&
                        std::isfinite(sdp_x)&&std::isfinite(sdp_z),
                        "SDP-Summen nicht endlich darstellbar");
            }
            if(include_contributions)
                contributions.push_back({{"streifen_id",id},{"kraft_N",f},
                    {"ansatz_m",position},{"eigenmoment_Nm",intrinsic}});
        };
        V quarter{q.at("viertel_x_m"),q.at("viertel_y_m"),q.at("viertel_z_m")};
        V lift=cross(u+induced,V{0,0,1})*(rho*(2*height*circulation)*dz*weight);
        double cm=pi*(q.at("a2").get<double>()-q.at("a1").get<double>())/4;
        double profile_moment=-0.5*rho*xy*xy*cm*chord*chord*dz*weight;
        add(lift,quarter,V{0,0,profile_moment});
        double cl=2*pi*(-angle*pi/180-q.at("a0").get<double>());
        for(int k=0;k<4;++k) {
            const auto& panel=surface.at(surface_index++);
            V normal{panel.at("normal_x"),panel.at("normal_y"),panel.at("normal_z")};
            V position{panel.at("position_x_m"),panel.at("position_y_m"),panel.at("position_z_m")};
            double area=panel.at("flaeche_m2"),un=dot(u,normal);
            V tang=u-normal*un;
            V pressure=normal*(0.5*rho*p.cn*un*std::abs(un)*(1-weight)*area);
            V drag=tang*(0.5*rho*p.k*cl*cl*weight*(xy/v)*(xy/v)*speed(tang)*area);
            if(re>=p.remin) {
                drag=drag+tang*(rho*p.friction(re)*speed(tang)*area);
            } else {
                V tang_unit{tang[0]/v,tang[1]/v,tang[2]/v};
                double scaled=rho*p.friction(p.remin)*std::sqrt(p.remin)*std::sqrt(nu)/std::sqrt(chord)*std::pow(v,1.5)*area;
                drag=drag+tang_unit*(scaled*speed(tang_unit));
            }
            add(pressure+drag,position,V{});
        }
    }
    J result=J::array();
    for(size_t id=0;id<forces.size();++id) {
        for(double x:forces[id]) require(std::isfinite(x),"Nicht endliche Streifenkraft");
        for(double x:moments[id]) require(std::isfinite(x),"Nicht endliches Streifenmoment");
        result.push_back({{"streifen_id",id},{"kraft_N",forces[id]},{"moment_Nm",moments[id]}});
    }
    if(include_contributions)return {{"streifen",result},{"beitraege",contributions}};
    if(sdp_transform)return {{"streifen",result},{"sdp",{
        {"Q_N",sdp_q},{"B_N",sdp_b},{"X_Nm",sdp_x},{"Z_Nm",sdp_z}}}};
    return result;
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
namespace detail {
struct PreparedModel::Impl {
    struct Coupling {
        std::vector<std::vector<double>> matrix;
        LiftingFactor factor;
        explicit Coupling(std::vector<std::vector<double>> values)
            :matrix(std::move(values)),factor(matrix) {}
    };
    J model;
    Params params;
    std::map<int,std::unique_ptr<Coupling>> couplings;
    static std::vector<std::vector<double>> matrix_for(const J& model,int modes) {
        std::vector<std::vector<double>> matrix(modes,std::vector<double>(modes));
        for(const auto& row:model.at("kopplung")) if(row.at("moden")==modes)
            matrix[row.at("zeile").get<size_t>()][row.at("spalte").get<size_t>()]=number(row,"wert");
        return matrix;
    }
    explicit Impl(J source):model(std::move(source)),params(model.at("parameter")) {
        for(int modes:{32,48,64})
            couplings.emplace(modes,std::make_unique<Coupling>(matrix_for(model,modes)));
    }
};
PreparedModel::PreparedModel(std::string_view data) {
    auto model=parse(data);
    Params params(model.at("parameter"));
    validate_model_data(model);
    impl_=std::make_unique<Impl>(std::move(model));
}
PreparedModel::~PreparedModel()=default;
std::string PreparedModel::solve(std::string_view winds,double rho,double nu) const {
    return solve_modes(winds,rho,nu,48);
}
std::string PreparedModel::solve_modes(std::string_view winds,double rho,double nu,int modes) const {
    auto system=impl_->couplings.find(modes);
    require(system!=impl_->couplings.end(),"Nicht unterstuetzte Segelmodell-Modenzahl");
    const auto& coupling=*system->second;
    return solve_strips(impl_->params,impl_->model,parse(winds),rho,nu,coupling.matrix,coupling.factor,modes).dump();
}
std::string PreparedModel::solve_with_contributions(std::string_view winds,double rho,double nu) const {
    const auto& coupling=*impl_->couplings.at(48);
    return solve_strips(impl_->params,impl_->model,parse(winds),rho,nu,coupling.matrix,coupling.factor,48,true).dump();
}
std::string PreparedModel::solve_with_sdp(std::string_view winds,double rho,double nu,
                                         const std::array<std::array<double,3>,3>& rotation,
                                         const std::array<double,3>& translation) const {
    RigidTransform placement{rotation,translation};
    validate_transform(placement);
    const auto& coupling=*impl_->couplings.at(48);
    return solve_strips(impl_->params,impl_->model,parse(winds),rho,nu,
                        coupling.matrix,coupling.factor,48,false,nullptr,&placement).dump();
}
J PreparedModel::solve_with_lifting_diagnostics(std::string_view winds,double rho,double nu,int modes) const {
    auto system=impl_->couplings.find(modes);
    require(system!=impl_->couplings.end(),"Nicht unterstuetzte Segelmodell-Modenzahl");
    const auto& coupling=*system->second;
    J diagnostics;
    auto loads=solve_strips(impl_->params,impl_->model,parse(winds),rho,nu,
                            coupling.matrix,coupling.factor,modes,false,&diagnostics);
    return {{"loads",std::move(loads)},{"lifting",std::move(diagnostics)}};
}
std::string prepare_strip_geometry(std::string_view input,int subdivision) {
    auto j=parse(input);
    fields(j,{"parameter","geometrie","numerik"},"Streifengeometrie");
    Params params(j.at("parameter"));
    G geometry(j.at("geometrie"),j.at("numerik"));
    return strip_geometry(geometry,subdivision).dump();
}
std::string prepare_coupling_geometry(std::string_view input) {
    auto j=parse(input);
    fields(j,{"parameter","geometrie","numerik"},"Kopplungsgeometrie");
    Params params(j.at("parameter"));
    G geometry(j.at("geometrie"),j.at("numerik"));
    return coupling_geometry(geometry).dump();
}
std::string prepare_model_data(std::string_view input,int subdivision) {
    auto j=parse(input);
    fields(j,{"parameter","geometrie","numerik"},"Segelmodell");
    Params params(j.at("parameter"));
    G geometry(j.at("geometrie"),j.at("numerik"));
    auto model=strip_geometry(geometry,subdivision);
    auto coupling=coupling_geometry(geometry);
    model["parameter"]=j.at("parameter");
    model["kollokation"]=std::move(coupling["kollokation"]);
    model["kopplung"]=std::move(coupling["kopplung"]);
    return model.dump();
}
std::string solve_prepared_case(std::string_view model_data,std::string_view winds,double rho,double nu) {
    return PreparedModel(model_data).solve(winds,rho,nu);
}
void validate_model_data_json(std::string_view model_data) {
    auto model=parse(model_data);
    Params params(model.at("parameter"));
    validate_model_data(model);
}
std::string solve_strip_case(std::string_view input,std::string_view winds,double rho,double nu,int subdivision) {
    auto model=prepare_model_data(input,subdivision);
    return solve_prepared_case(model,winds,rho,nu);
}
}
namespace {
J h1_solve(const G& g,const Params& params,const std::map<int,LiftingFactor>& lifting_factors,
           const std::vector<Surface>& surfaces,double alpha,double beta,double re,int factor,bool keep,int panels) {
    require(std::isfinite(alpha)&&std::isfinite(beta)&&beta>=-90&&beta<=90,"Windwinkel ungueltig");
    params.recheck(re);
    require(g.grids.count(factor)!=0,"Rasterfaktor muss 1,2 oder 4 sein");
    require(panels==12||panels==16||panels==24||panels==32||panels==48||panels==64,
            "Wirbelfeldzahl muss 12, 16, 24, 32, 48 oder 64 sein");
    double a=std::remainder(alpha,360.)*pi/180,b=beta*pi/180;
    V u=std::abs(beta)==90?V{0,0,-std::sin(b)}:
        V{-std::cos(b)*std::cos(a),-std::cos(b)*std::sin(a),-std::sin(b)};
    double elevweight=1-smooth(60,80,std::abs(beta));
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
        return h1_solve(g,params,lifting_factors,surfaces,alpha,beta,re,factor,keep,panels);
    }
};
namespace detail {
struct SplitReference::Impl {
    Params params;
    std::shared_ptr<G> geometry;
    std::map<int,LiftingFactor> factors;
    std::vector<Surface> surfaces;
    explicit Impl(const J& j):params(j.at("parameter")) {
        fields(j,{"parameter","geometrie","numerik"},"H1-Referenz");
        auto numerik=j.at("numerik");
        auto& grid=numerik["hoehenraster"];
        grid["schritt_m"]=number(grid,"schritt_m")/4;
        grid["verfeinerter_schritt_m"]=number(grid,"verfeinerter_schritt_m")/4;
        geometry=std::make_shared<G>(j.at("geometrie"),numerik,true);
        factors.emplace(48,LiftingFactor(*geometry,48));
        surfaces.push_back({geometry->id,geometry,{}});
    }
};
SplitReference::SplitReference(std::string_view input):impl_(std::make_unique<Impl>(parse(input))) {}
SplitReference::~SplitReference()=default;
bool SplitReference::valid(double re) const {
    const auto& p=impl_->params;
    if(!std::isfinite(re)||re<p.remin||re>p.remax)return false;
    for(const auto& section:impl_->geometry->grids.at(4)) {
        double local=re*section.chord/impl_->geometry->cref;
        if(!std::isfinite(local)||local<p.remin||local>p.remax)return false;
    }
    return true;
}
J SplitReference::lifting_modes(double alpha,double beta) const {
    require(std::isfinite(alpha)&&std::isfinite(beta)&&beta>=-90&&beta<=90,"Windwinkel ungueltig");
    const double a=std::remainder(alpha,360.)*pi/180,b=beta*pi/180;
    V u=std::abs(beta)==90?V{0,0,-std::sin(b)}:
        V{-std::cos(b)*std::cos(a),-std::cos(b)*std::sin(a),-std::sin(b)};
    require(std::hypot(u[0],u[1])>0,"H1-Moden erfordern horizontalen Wind");
    return lifting(*impl_->geometry,u,impl_->factors.at(48)).modes;
}
nlohmann::json SplitReference::solve_data(double alpha,double beta,double re) const {
    return h1_solve(*impl_->geometry,impl_->params,impl_->factors,impl_->surfaces,alpha,beta,re,4,true,48);
}
std::string SplitReference::solve(double alpha,double beta,double re) const {
    return solve_data(alpha,beta,re).dump();
}
std::string h1_split_reference(std::string_view input,double alpha,double beta,double re) {
    return SplitReference(input).solve(alpha,beta,re);
}
}
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
    RigidTransform placement{r,t};
    validate_transform(placement);
    J loads=j.at("loads");
    for(auto& l:loads) {
        l["f"]=placement.vector(l.at("f").get<V>());l["m"]=placement.vector(l.at("m").get<V>());
        l["r"]=placement.point(l.at("r").get<V>());
        for(const auto* name:{"f","m","r"})for(double x:l[name])require(std::isfinite(x),"Transformierte Last nicht darstellbar");
    }
    return loads.dump();
}
}
