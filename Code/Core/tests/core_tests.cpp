#include "segel/core.hpp"
#include <nlohmann/json.hpp>
#include <cmath>
#include <iostream>
#include <stdexcept>
using J=nlohmann::json;
namespace {
int checks=0;
void check(bool b,const char* text) {++checks;if(!b)throw std::runtime_error(text);}
template<class F> void rejects(F f) {bool failed=false;try{f();}catch(const std::exception&){failed=true;}check(failed,"Expected rejection");}
J parameters() {
    return {{"stall_start_deg",12},{"stall_end_deg",25},{"stall_re_shift_deg",1},{"transition_re",500000},
        {"transition_width",0.2},{"viscous_cl2",0.01},{"normal_pressure",2},{"re_min",1000},{"re_max",1e8}};
}
J geometry() {
    J g;
    g["schema_version"]="0.1";g["segel"]={{"id","test"},{"typ","fock"}};
    g["grundkontur"]["punkte"]=J::array({
        {{"id","hals"},{"position_m",{0,0,0}}},{{"id","clew"},{"position_m",{-2,0,0}}},
        {{"id","aft"},{"position_m",{-2,0,4}}},{{"id","head"},{"position_m",{0,0,4}}}});
    g["grundkontur"]["rand"]["kanten"]=J::array({
        {{"name","a"},{"punkt_ids",{"hals","clew","aft","head","hals"}}}});
    g["flaechenform"]["profilschnitte"]["stationen"]=J::array({
        {{"eta",0},{"woelbungsverhaeltnis",0},{"woelbungsposition_relativ",0.4},{"verwindung_deg",0}},
        {{"eta",1},{"woelbungsverhaeltnis",0},{"woelbungsposition_relativ",0.4},{"verwindung_deg",0}}});
    return g;
}
J input() {
    return {{"parameter",parameters()},{"geometrie",geometry()},
        {"numerik",{{"streifenintegration","mittelpunktregel"},{"hoehenraster",{{"schritt_m",0.5},
            {"verfeinerter_schritt_m",0.25},{"verfeinern_ab_eta",0.75},
            {"kontur_und_profilstationen_einfuegen",true},{"nullsehnen_als_punkte_behandeln",true}}}}}};
}
double length(const J& a) {double n=0;for(double v:a)n+=v*v;return std::sqrt(n);}
}
int main() {
    try {
        J p={{"m",0},{"p",0.4},{"alpha_deg",-5},{"re",1e6},{"parameter",parameters()}};
        auto flat=J::parse(segel::polar(p.dump()));
        check(std::abs(flat["CL"].get<double>()-2*3.141592653589793*5*3.141592653589793/180)<1e-10,"2pi slope");
        check(std::abs(flat["CM"].get<double>())<1e-12,"Symmetric quarter-chord moment");
        p["alpha_deg"]=0;p["re"]=10000;
        auto lam=J::parse(segel::polar(p.dump()));
        check(std::abs(lam["CD"].get<double>()-2*1.328/100)<1e-12,"Laminar friction reference");
        p["re"]=1e6;p["m"]=0.08;
        auto cam=J::parse(segel::polar(p.dump()));
        check(cam["CL"].get<double>()>0.5,"Camber lift");
        check(std::abs(cam["CM"].get<double>())>0.01,"Camber moment");
        p["p"]=0.6;
        auto moved=J::parse(segel::polar(p.dump()));
        check(std::abs(moved["CM"].get<double>()-cam["CM"].get<double>())>1e-3,"Camber location effect");
        for(double edge:{12.,25.}) {
            p["alpha_deg"]=edge-1e-5;auto left=J::parse(segel::polar(p.dump()));
            p["alpha_deg"]=edge+1e-5;auto right=J::parse(segel::polar(p.dump()));
            check(std::abs(left["CL"].get<double>()-right["CL"].get<double>())<1e-4,"Stall transition continuity");
        }
        p["re"]=999;rejects([&]{segel::polar(p.dump());});
        rejects([]{segel::polar("{\"m\":0,\"m\":1}");});
        auto unknown=p;unknown["parameter"]["unknown"]=1;
        rejects([&]{segel::polar(unknown.dump());});
        auto in=input();segel::Context ctx(in.dump());
        auto g=J::parse(ctx.geometry());
        check(std::abs(g["S_m2"].get<double>()-8)<1e-12,"Polygon area");
        check(std::abs(g["c_ref_m"].get<double>()-2)<1e-12,"Reference length");
        auto r=J::parse(ctx.solve(-5,0,1e6,4));
        check(r["CF"][1].get<double>()>0,"Lift sign");
        check(r["CF"][0].get<double>()<0,"Positive total drag");
        auto neg=J::parse(ctx.solve(5,0,1e6,4));
        check(std::abs(r["CF"][1].get<double>()+neg["CF"][1].get<double>())<1e-8,"Lift symmetry");
        auto pole=J::parse(ctx.solve(-180,90,1e6,4));
        check(pole["CF"]==J::parse(ctx.solve(77,90,1e6,4))["CF"],"Pole azimuth invariance");
        check(pole["CF"][2].get<double>()<0,"Spanwise skin drag not zero");
        auto integrated=J::parse(segel::resultant(J{{"c_ref",2},{"loads",r["loads"]}}.dump()));
        check(integrated["CF"]==r["CF"]&&integrated["CM"]==r["CM"],"Load sum");
        J loads=J::array({{{"f",{1,0,0}},{"r",{0,1,0}},{"m",{2,0,0}},{"segel_id","a"}}});
        auto rr=J::parse(segel::resultant(J{{"c_ref",2},{"loads",loads}}.dump()));
        check(rr["valid"]==true&&std::abs(rr["point"][1].get<double>()-1)<1e-12,"Central axis");
        check(rr["rest"][0]==2,"Parallel residual moment");
        loads[0]["f"]={0,0,0};
        auto pure=J::parse(segel::resultant(J{{"c_ref",2},{"loads",loads}}.dump()));
        check(pure["valid"]==false&&pure["point"].is_null()&&pure["rest"][0]==2,"Pure moment");
        loads[0]["f"]={1e-12,0,0};
        check(J::parse(segel::resultant(J{{"c_ref",2},{"loads",loads}}.dump()))["valid"]==false,"Small resultant invalid");
        loads[0]["f"]={1,0,0};
        auto transformed=J::parse(segel::transform(J{{"loads",loads},{"rotation",{{0,-1,0},{1,0,0},{0,0,1}}},{"translation",{2,3,4}}}.dump()));
        check(transformed[0]["f"]==J::array({0,1,0})&&transformed[0]["r"]==J::array({1,3,4}),"Rigid transformation");
        check(transformed[0]["segel_id"]=="a","Sail identity");
        J couple=J::array({{{"f",{1,0,0}},{"r",{0,1,0}},{"m",{0,0,0}}},
                           {{"f",{-1,0,0}},{"r",{0,-1,0}},{"m",{0,0,0}}}});
        auto pair=J::parse(segel::resultant(J{{"c_ref",2},{"loads",couple}}.dump()));
        check(pair["valid"]==false&&pair["CM"][2]==-1,"Force couple retains moment");
        auto huge=couple;huge[0]["f"]={1e308,1e308,1e308};
        rejects([&]{segel::resultant(J{{"c_ref",2},{"loads",huge}}.dump());});
        rejects([&]{ctx.solve(0,-1,1e6,4);});
        rejects([&]{ctx.solve(0,0,1e6,3);});
        auto damaged=in;damaged["geometrie"]["grundkontur"]["rand"]["kanten"][0]["punkt_ids"][1]="unknown";
        rejects([&]{segel::Context bad(damaged.dump());});
        auto triangle=in;
        triangle["geometrie"]["grundkontur"]["punkte"].erase(2);
        triangle["geometrie"]["grundkontur"]["rand"]["kanten"][0]["punkt_ids"]={"hals","clew","head","hals"};
        segel::Context tri(triangle.dump());
        check(std::abs(J::parse(tri.geometry())["S_m2"].get<double>()-4)<1e-12,"Triangle tip");
        check(length(J::parse(tri.solve(90,45,1e6,4))["CF"])>0,"High angle spatial load");
        auto shape=in;shape["geometrie"]["flaechenform"]["profilschnitte"]["stationen"][1]["woelbungsverhaeltnis"]=0.1;
        shape["geometrie"]["flaechenform"]["profilschnitte"]["stationen"][1]["verwindung_deg"]=-8;
        segel::Context curved(shape.dump());
        check(J::parse(curved.solve(0,0,1e6,4))["CF"]!=J::parse(ctx.solve(0,0,1e6,4))["CF"],"Camber/twist effect");
        for(double b:{0.,30.,70.,89.9,90.}) for(double a:{-180.,-90.,0.,90.,179.})
            check(std::isfinite(length(J::parse(curved.solve(a,b,1e6,4))["CF"])),"Full angle finite");
        auto ellipse=in;
        auto& points=ellipse["geometrie"]["grundkontur"]["punkte"];
        auto& ids=ellipse["geometrie"]["grundkontur"]["rand"]["kanten"][0]["punkt_ids"];
        points=J::array();ids=J::array();
        constexpr int edges=120;
        for(int i=0;i<edges;++i) {
            double theta=2*3.141592653589793*i/edges;
            std::string id=i==0?"hals":"e"+std::to_string(i);
            points.push_back({{"id",id},{"position_m",{-std::sin(theta),0,2-2*std::cos(theta)}}});
            ids.push_back(id);
        }
        ids.push_back("hals");
        segel::Context elliptic(ellipse.dump());
        auto eg=J::parse(elliptic.geometry());
        auto er=J::parse(elliptic.solve(-5,0,1e6,4));
        double ar=16/eg["S_m2"].get<double>();
        double expected_cl=2*3.141592653589793*(5*3.141592653589793/180)*ar/(ar+2);
        double inviscid_lift=0,inviscid_drag=0;
        for(const auto& l:er["loads"])if(l["kind"]=="gebunden_plus_profilmoment") {
            double a=5*3.141592653589793/180,fx=l["f"][0].get<double>(),fy=l["f"][1].get<double>();
            inviscid_lift+=fx*std::sin(a)+fy*std::cos(a);
            inviscid_drag+=-fx*std::cos(a)+fy*std::sin(a);
        }
        check(std::abs(inviscid_lift-expected_cl)<0.02*expected_cl,"Elliptic finite wing analytical lift");
        double expected_cdi=expected_cl*expected_cl/(3.141592653589793*ar);
        check(std::abs(inviscid_drag-expected_cdi)<0.03*expected_cdi,"Elliptic induced drag without double counting");
        std::cout<<checks<<" core contract checks passed\n";return 0;
    } catch(const std::exception& e){std::cerr<<"Core test failed after "<<checks<<": "<<e.what()<<'\n';return 1;}
}
