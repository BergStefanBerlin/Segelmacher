#include "segel/core.hpp"
#include "../src/segelform_internal.hpp"
#include "../src/segelform_test_winds.hpp"
#include "../src/segelform_uniform_check.hpp"
#include "../src/segelform_edge_check.hpp"
#include "../src/segelform_re_check.hpp"
#include <nlohmann/json.hpp>
#include <array>
#include <cmath>
#include <future>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
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
int main(int argc,char** argv) {
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
        segel::detail::SplitReference validity_reference(in.dump());
        check(validity_reference.valid(1000)&&validity_reference.valid(1e8)&&
              !validity_reference.valid(std::nextafter(1000.,0.))&&
              !validity_reference.valid(std::nextafter(1e8,std::numeric_limits<double>::infinity()))&&
              !validity_reference.valid(std::numeric_limits<double>::quiet_NaN()),
              "H1 reference validity uses its actual integration grid");
        auto strips=J::parse(segel::detail::prepare_strip_geometry(in.dump()));
        check(strips["streifen"].size()==16,"Equal-height output strips");
        auto fine_strips=J::parse(segel::detail::prepare_strip_geometry(in.dump(),2));
        for(auto kind:{segel::detail::EdgeWindCase::counterflow,segel::detail::EdgeWindCase::azimuth_seam,
                       segel::detail::EdgeWindCase::local_vertical})for(bool reverse:{false,true}) {
            double vmax=0;
            auto coarse_winds=segel::detail::edge_winds(strips,strips,kind,reverse,vmax);
            auto fine_winds=segel::detail::edge_winds(strips,fine_strips,kind,reverse,vmax);
            auto center=coarse_winds[coarse_winds.size()/2].at("wind_m_s").get<std::array<double,3>>();
            if(kind==segel::detail::EdgeWindCase::counterflow)
                check(center==std::array<double,3>{0,0,0},"Counterflow includes local zero wind");
            else if(kind==segel::detail::EdgeWindCase::azimuth_seam)
                check(center==std::array<double,3>{5,0,0},"Azimuth field crosses the seam");
            else check(center==std::array<double,3>{0,0,5},"Vertical field includes local vertical wind");
            auto first=coarse_winds.front().at("wind_m_s").get<std::array<double,3>>();
            auto last=coarse_winds.back().at("wind_m_s").get<std::array<double,3>>();
            int component=kind==segel::detail::EdgeWindCase::azimuth_seam?1:0;
            check(reverse?(first[component]>0&&last[component]<0):(first[component]<0&&last[component]>0),
                  "Edge field crosses its boundary in both directions");
            for(size_t id=0;id<coarse_winds.size();++id) {
                auto c=coarse_winds[id].at("wind_m_s").get<std::array<double,3>>();
                auto a=fine_winds[2*id].at("wind_m_s").get<std::array<double,3>>();
                auto b=fine_winds[2*id+1].at("wind_m_s").get<std::array<double,3>>();
                for(int k=0;k<3;++k)
                    check(std::abs(c[k]-(a[k]+b[k])/2)<1e-12,"Same continuous field on both grids");
            }
        }
        auto transition_model=strips;
        transition_model["parameter"]=parameters();
        auto validity_model=transition_model;
        validity_model["quadratur"]=J::array({{{"chord_m",1.0}}});
        check(segel::detail::uniform_h1_valid(1.0,0.001,validity_model,g),
              "H1 comparison accepts local Reynolds lower bound");
        validity_model["quadratur"][0]["chord_m"]=0.5;
        check(!segel::detail::uniform_h1_valid(1.0,0.001,validity_model,g),
              "H1 comparison excludes low-Re model quadrature");
        validity_model["quadratur"][0]["chord_m"]=200000.0;
        check(!segel::detail::uniform_h1_valid(1.0,0.001,validity_model,g),
              "H1 comparison excludes model quadrature above Re max");
        auto transition_cases=segel::detail::uniform_transition_cases(transition_model,10,1.5e-5);
        auto transition_groups=segel::detail::uniform_transition_groups(transition_model,10,1.5e-5);
        size_t pole_groups=0;
        for(const auto& group:transition_groups) {
            check(transition_cases.count(group.center)==1,"Transition center is calculated");
            for(const auto& side:group.sides)
                check(transition_cases.count(side)==1,"Transition side is calculated");
            if(group.center.first==-5&&std::abs(group.center.second)==90) {
                check(group.sides.size()==1,"Pole transition has one valid side");
                ++pole_groups;
            }
        }
        check(pole_groups==2,"Both poles have a transition group");
        for(double beta:{-90.,-80.,-60.,60.,80.,90.}) {
            check(transition_cases.count({-5.,beta})==1,"Exact elevation transition included");
            if(beta> -90)check(transition_cases.count({-5.,beta-1e-6})==1,"Lower elevation transition included");
            if(beta<90)check(transition_cases.count({-5.,beta+1e-6})==1,"Upper elevation transition included");
        }
        for(double alpha:{-180.,-120.,120.,180.})
            for(double offset:{-1e-6,0.,1e-6})
                check(transition_cases.count({std::remainder(alpha+offset,360.),0.})==1,
                      "Azimuth seam and blend transition included");
        double transition_stall_shift=std::log10((10*2/1.5e-5)/500000);
        for(double edge:{12+transition_stall_shift,25+transition_stall_shift})
            for(double sign:{-1.,1.})
                for(double offset:{-1e-6,0.,1e-6})
                    check(transition_cases.count({std::remainder(sign*edge+offset,360.),0.})==1,
                          "Local stall transition included");
        auto zero_transition_cases=segel::detail::uniform_transition_cases(transition_model,0,1.5e-5);
        for(const auto& angles:zero_transition_cases)
            check(std::isfinite(angles.first)&&std::isfinite(angles.second),
                  "Zero-speed transition angles are finite");
        for(double beta:{-80.,-30.,0.,30.,80.,90.})for(double alpha:{-179.,-137.,-5.,17.,90.,150.}) {
            auto wind=segel::detail::uniform_wind(50,alpha,beta);
            auto components=std::count_if(wind.begin(),wind.end(),[](double value){return value!=0;});
            double bound=components>1?std::nextafter(std::nextafter(50.,0.),0.):50.;
            check(std::hypot(std::hypot(wind[0],wind[1]),wind[2])<=bound,
                  "Uniform-wind vector remains within the speed limit");
        }
        auto axis_wind=segel::detail::uniform_wind(50,0,0);
        check(axis_wind[0]==-50&&axis_wind[1]==0&&axis_wind[2]==0,
              "Axis-aligned uniform wind reaches exactly 50 m/s");
        J bad_reference={{"CF",{1e308,0,0}},{"CM",{0,0,0}}};
        J zero_reference={{"CF",{0,0,0}},{"CM",{0,0,0}},{"loads",J::array()}};
        bool invalid_comparison_rejected=false;
        try {
            segel::detail::compare_uniform_h1(
                strips,J::array(),bad_reference,zero_reference,zero_reference,10,1.225);
        } catch(const std::runtime_error& error) {
            invalid_comparison_rejected=std::string(error.what()).find("H1-Vergleich")!=std::string::npos;
        }
        check(invalid_comparison_rejected,"Non-representable H1 comparison rejected");
        J zero_loads=J::array();
        for(const auto& strip:strips["streifen"])
            zero_loads.push_back({{"kraft_N",{0,0,0}},{"moment_Nm",{0,0,0}}});
        auto exact=segel::detail::compare_uniform_h1(
            strips,zero_loads,zero_reference,zero_reference,zero_reference,10,1.225);
        check(exact["quotient"]==0&&exact["groesse"]=="h1_split_force"&&
              std::abs(exact["grenze"].get<double>()-(0.001*0.5*1.225*100*8+1e-9))<1e-12,
              "Exact H1 comparison still records its actual tolerance");
        zero_loads[0]["kraft_N"]={1,0,0};
        auto mismatch=segel::detail::compare_uniform_h1(
            strips,zero_loads,zero_reference,zero_reference,zero_reference,10,1.225);
        check(mismatch["quotient"].get<double>()>1&&mismatch["streifen_id"]==0&&
              mismatch["groesse"]=="strip_force"&&mismatch["flaeche_m2"]==0.5&&
              mismatch["wert_a"]==J::array({1,0,0})&&mismatch["wert_b"]==J::array({0,0,0}),
              "H1 comparison reports the failing strip and reference area");
        J other_loads=zero_loads;
        other_loads[1]["moment_Nm"]={0,0,2};
        segel::detail::CompactLoads compact_zero,compact_other;
        for(const auto& load:zero_loads) {
            auto force=load.at("kraft_N").get<std::array<double,3>>();
            auto moment=load.at("moment_Nm").get<std::array<double,3>>();
            compact_zero.push_back({force[0],force[1],force[2],moment[0],moment[1],moment[2]});
        }
        compact_other=compact_zero;
        compact_other[1][5]=2;
        check(segel::detail::compare_same_grid_loads(strips,zero_loads,other_loads,10)==
              segel::detail::compare_same_grid_loads(strips,compact_zero,compact_other,10),
              "Compact transition loads preserve force and moment comparisons");
        double strip_area=0;
        for(size_t i=0;i<strips["streifen"].size();++i) {
            const auto& strip=strips["streifen"][i];
            strip_area+=strip["flaeche_m2"].get<double>();
            check(strip["streifen_id"]==i,"Stable strip ID");
            check(std::abs(strip["abfrage_x_m"].get<double>()+0.5)<1e-12,
                  "Quarter-chord wind point");
            check(std::abs(strip["abfrage_z_m"].get<double>()-(i+0.5)*0.25)<1e-12,
                  "Mid-height wind point");
        }
        check(std::abs(strip_area-8)<1e-10,"Output strip areas preserve contour");
        check(strips["flaechenpunkte"].size()==4*strips["quadratur"].size(),
              "Four H1 surface points per quadrature point");
        auto adjacent=in;
        auto& stations=adjacent["geometrie"]["flaechenform"]["profilschnitte"]["stationen"];
        auto near_boundary=stations[0];
        near_boundary["eta"]=std::nextafter(0.125,1.0);
        stations.insert(stations.begin()+1,near_boundary);
        auto adjacent_model=J::parse(segel::detail::prepare_model_data(adjacent.dump()));
        segel::detail::PreparedModel adjacent_solver(adjacent_model.dump());
        check(adjacent_model["streifen"].size()==16,"Near-coincident profile and strip boundaries");
        auto height_42=in;
        height_42["geometrie"]["grundkontur"]["punkte"][2]["position_m"][2]=4.2;
        height_42["geometrie"]["grundkontur"]["punkte"][3]["position_m"][2]=4.2;
        auto strips_42=J::parse(segel::detail::prepare_strip_geometry(height_42.dump()));
        check(strips_42["streifen"].size()==17,"4.2 m uses ceil(H/0.25), not twice ceil(H/0.5)");
        for(const auto& strip:strips_42["streifen"])
            check(std::abs(strip["z_max_m"].get<double>()-strip["z_min_m"].get<double>()-4.2/17)<1e-12,
                  "4.2 m strips are equally high");
        auto above_four=in;
        above_four["geometrie"]["grundkontur"]["punkte"][2]["position_m"][2]=4.0001;
        above_four["geometrie"]["grundkontur"]["punkte"][3]["position_m"][2]=4.0001;
        check(J::parse(segel::detail::prepare_strip_geometry(above_four.dump()))["streifen"].size()==17,
              "Height above a 0.25 m boundary rounds up");
        auto short_sail=in;
        short_sail["geometrie"]["grundkontur"]["punkte"][2]["position_m"][2]=0.25;
        short_sail["geometrie"]["grundkontur"]["punkte"][3]["position_m"][2]=0.25;
        check(J::parse(segel::detail::prepare_strip_geometry(short_sail.dump()))["streifen"].size()==1,
              "0.25 m sail uses one strip");
        auto short_model=J::parse(segel::detail::prepare_model_data(short_sail.dump()));
        segel::detail::PreparedModel short_solver(short_model.dump());
        auto short_continuity=segel::detail::check_wind_continuity(short_model,short_solver,[]{});
        check(short_continuity.size()==6&&
              short_continuity[0]["schlechtester_vergleich"]["quotient"].get<double>()>0&&
              short_continuity[1]["schlechtester_vergleich"]["quotient"].get<double>()>0,
              "One-strip sail tests both sides of the azimuth seam");
        auto short_re=segel::detail::check_re_boundaries(short_model,short_solver,[]{});
        check(short_re["gepruefte_punkte"]==short_model["quadratur"].size()&&
              short_re["gepruefte_punkte"].get<size_t>()>1&&
              short_re["verschiedene_grenzgeschwindigkeiten"]==1&&
              short_re["ausserhalb_0_bis_50_m_s"]==0&&
              !short_re["schlechtester_vergleich"].is_null(),
              "Re_min diagnosis covers every point and reuses equal boundary speeds");
        check(short_re["faelle"].size()==short_model["quadratur"].size()&&
              short_re["faelle"][0]["punkt_id"]==short_model["quadratur"][0]["punkt_id"]&&
              short_re["faelle"][0]["status"]=="geprueft"&&
              short_re["faelle"][0]["vergleiche"].size()==2&&
              short_re["faelle"][0]["vergleiche"][0]["faktor"]==1-1e-6&&
              short_re["faelle"][0]["vergleiche"][1]["faktor"]==1+1e-6,
              "Re_min diagnosis reports both comparisons for each quadrature point");
        int cancellation_checks=0;
        rejects([&]{segel::detail::check_re_boundaries(short_model,short_solver,[&]{
            if(++cancellation_checks==2)throw std::runtime_error("cancelled");
        });});
        auto mixed_re_model=short_model;
        mixed_re_model["quadratur"][0]["chord_m"]=1e-8;
        auto mixed_re=segel::detail::check_re_boundaries(mixed_re_model,short_solver,[]{});
        check(mixed_re["ausserhalb_0_bis_50_m_s"]==1&&
              mixed_re["gepruefte_punkte"].get<size_t>()+1==short_model["quadratur"].size(),
              "Re_min diagnosis reports boundaries outside the speed range");
        check(mixed_re["faelle"][0]["status"]=="ausserhalb_0_bis_50_m_s"&&
              mixed_re["faelle"][0]["vergleiche"].empty()&&
              mixed_re["faelle"][1]["status"]=="geprueft",
              "Re_min diagnosis identifies each out-of-range boundary");
        short_sail["geometrie"]["grundkontur"]["punkte"][2]["position_m"][2]=0.2501;
        short_sail["geometrie"]["grundkontur"]["punkte"][3]["position_m"][2]=0.2501;
        check(J::parse(segel::detail::prepare_strip_geometry(short_sail.dump()))["streifen"].size()==2,
              "Sail just above 0.25 m uses two strips");
        auto coupling=J::parse(segel::detail::prepare_coupling_geometry(in.dump()));
        check(coupling["kollokation"].size()==32+48+64,"Complete collocation rows");
        check(coupling["kopplung"].size()==32*32+48*48+64*64,"Complete coupling matrices");
        size_t row=0;
        for(int modes:{32,48,64}) for(int i=0;i<modes;++i) {
            const auto& c=coupling["kollokation"][row++];
            check(c["moden"]==modes&&c["index"]==i,"Ordered collocation indices");
        }
        const auto& entry=coupling["kopplung"][0];
        double theta=3.141592653589793*0.5/32;
        double expected=std::sin(theta)*(4*4/(2*3.141592653589793*2)+1/std::sin(theta));
        check(std::abs(entry["wert"].get<double>()-expected)<1e-12,"Coupling matrix matches H1");
        auto solve_strips=[&](double alpha,double beta,double velocity) {
            double a=alpha*3.141592653589793/180,b=beta*3.141592653589793/180;
            J winds=J::array();
            for(size_t i=0;i<strips["streifen"].size();++i)
                winds.push_back({{"streifen_id",i},{"wind_m_s",{-velocity*std::cos(b)*std::cos(a),
                    -velocity*std::cos(b)*std::sin(a),-velocity*std::sin(b)}}});
            return J::parse(segel::detail::solve_strip_case(in.dump(),winds.dump(),1.225,1.5e-5));
        };
        auto total_strips=[&](const J& values) {
            std::array<double,6> total{};
            for(const auto& row:values) {
                size_t id=row["streifen_id"].get<size_t>();
                auto f=row["kraft_N"].get<std::array<double,3>>();
                auto m=row["moment_Nm"].get<std::array<double,3>>();
                const auto& point=strips["streifen"][id];
                std::array<double,3> r{point["abfrage_x_m"],point["abfrage_y_m"],point["abfrage_z_m"]};
                std::array<double,3> arm{r[1]*f[2]-r[2]*f[1],r[2]*f[0]-r[0]*f[2],r[0]*f[1]-r[1]*f[0]};
                for(int k=0;k<3;++k) { total[k]+=f[k];total[k+3]+=m[k]+arm[k]; }
            }
            return total;
        };
        for(double beta:{-90.,-60.,0.,60.,90.}) for(double alpha:{-180.,-30.,-5.,0.,30.,150.}) {
            auto actual=total_strips(solve_strips(alpha,beta,10));
            auto ref=J::parse(ctx.solve(alpha,beta,10*2/1.5e-5,4,false,48));
            double force_error=0,moment_error=0,q=0.5*1.225*100;
            for(int k=0;k<3;++k) {
                force_error+=std::pow(actual[k]-q*8*ref["CF"][k].get<double>(),2);
                moment_error+=std::pow(actual[k+3]-q*8*2*ref["CM"][k].get<double>(),2);
            }
            check(std::sqrt(force_error)<=0.001*q*8+1e-9,"Uniform force matches direct H1");
            check(std::sqrt(moment_error)<=0.001*q*8*2+1e-9,"Uniform moment matches direct H1");
        }
        auto split_reference=J::parse(segel::detail::h1_split_reference(in.dump(),-5,0,1e6));
        segel::detail::SplitReference reusable_reference(in.dump());
        check(J::parse(reusable_reference.solve(-5,0,1e6))==split_reference,
              "Reusable split reference matches an independent reference");
        for(const auto& angles:std::array<std::array<double,2>,3>{{{-180,0},{-5,0},{30,80}}}) {
            auto direct_data=reusable_reference.solve_data(angles[0],angles[1],1e6);
            auto serialized=J::parse(reusable_reference.solve(angles[0],angles[1],1e6));
            check(direct_data==serialized,"Split reference data matches serialized output");
        }
        auto direct_reference=J::parse(ctx.solve(-5,0,1e6,4,true,48));
        double split_force_error=0,split_moment_error=0;
        for(int k=0;k<3;++k) {
            split_force_error+=std::pow(split_reference["CF"][k].get<double>()-
                                        direct_reference["CF"][k].get<double>(),2);
            split_moment_error+=std::pow(split_reference["CM"][k].get<double>()-
                                         direct_reference["CM"][k].get<double>(),2);
        }
        check(std::sqrt(split_force_error)<=0.001&&std::sqrt(split_moment_error)<=0.001,
              "Refined split H1 reference preserves direct result within tolerance");
        auto zero=total_strips(solve_strips(0,0,0));
        for(double x:zero) check(x==0,"Zero wind yields zero force and moment");
        auto slow=total_strips(solve_strips(-5,0,0.001));
        for(double x:slow) check(std::isfinite(x),"Low-Re force and moment finite");
        auto below=total_strips(solve_strips(-5,0,0.0075*(1-1e-6)));
        auto above=total_strips(solve_strips(-5,0,0.0075*(1+1e-6)));
        for(size_t i=0;i<below.size();++i)
            check(std::abs(below[i]-above[i])<1e-8,"Low-Re transition is continuous");
        J bad_winds=J::array();
        for(size_t i=0;i<strips["streifen"].size();++i)
            bad_winds.push_back({{"streifen_id",i},{"wind_m_s",{0,0,0}}});
        auto prepared=J::parse(segel::detail::prepare_model_data(in.dump()));
        auto prepared_zero=J::parse(segel::detail::solve_prepared_case(prepared.dump(),bad_winds.dump(),1.225,1.5e-5));
        check(total_strips(prepared_zero)==zero,"Prepared model uses persisted geometry and coupling");
        segel::detail::PreparedModel reusable(prepared.dump());
        auto detailed_zero=J::parse(reusable.solve_with_contributions(bad_winds.dump(),1.225,1.5e-5));
        check(detailed_zero["streifen"]==prepared_zero&&detailed_zero["beitraege"].empty(),
              "Zero wind yields zero strips and no individual loads");
        for(const auto& angles:std::array<std::array<double,2>,3>{{{-5,0},{5,30},{0,-30}}}) {
            double a=angles[0]*std::acos(-1.0)/180,b=angles[1]*std::acos(-1.0)/180;
            double horizontal=10*std::cos(b);
            J uniform=bad_winds;
            for(auto& wind:uniform)
               wind["wind_m_s"]={-horizontal*std::cos(a),-horizontal*std::sin(a),-10*std::sin(b)};
            auto diagnostics=reusable.solve_with_lifting_diagnostics(uniform.dump(),1.225,1.5e-5);
            auto reference_modes=reusable_reference.lifting_modes(angles[0],angles[1]);
            check(diagnostics.at("loads")==J::parse(reusable.solve(uniform.dump(),1.225,1.5e-5))&&
                 diagnostics["lifting"]["solved"]==true&&reference_modes.size()==48,
                 "Lifting diagnostic preserves the normal solution");
            const auto& lift=diagnostics.at("lifting");
            double residual=lift.at("residual_inf"),denominator=lift.at("residual_denominator");
            check(denominator>=0&&
                  (denominator==0?(residual==0&&lift.at("backward_error")==0):
                                  lift.at("backward_error").get<double>()==residual/denominator)&&
                  lift.at("backward_error").get<double>()<=1e-10,
                  "Lifting diagnostic reports the actual backward error");
            for(int modes:{32,64}) {
                auto other=reusable.solve_with_lifting_diagnostics(uniform.dump(),1.225,1.5e-5,modes);
                check(other.at("loads")==J::parse(reusable.solve_modes(uniform.dump(),1.225,1.5e-5,modes))&&
                      other["lifting"]["coefficients"].size()==static_cast<size_t>(modes)&&
                      other["lifting"]["backward_error"].get<double>()<=1e-10,
                      "Lifting diagnostic uses the requested mode count without changing loads");
            }
            for(size_t i=0;i<reference_modes.size();++i) {
               double expected=horizontal*reference_modes[i].get<double>();
               double actual=diagnostics["lifting"]["coefficients"][i].get<double>();
               check(std::abs(actual-expected)<=1e-10*std::max(1.0,std::abs(expected)),
                     "Uniform-wind lifting modes equal horizontal speed times H1 modes");
            }
        }
        rejects([&]{reusable.solve_with_lifting_diagnostics(bad_winds.dump(),1.225,1.5e-5,33);});
        for(double alpha:{-180.,-120.,120.,180.}) {
            double a=alpha*std::acos(-1.0)/180;
            J uniform=bad_winds;
            for(auto& wind:uniform)wind["wind_m_s"]={-10*std::cos(a),-10*std::sin(a),0};
            auto diagnostics=reusable.solve_with_lifting_diagnostics(uniform.dump(),1.225,1.5e-5);
            check(diagnostics["lifting"]["solved"]==false&&
                 diagnostics["lifting"]["backward_error"].is_null()&&
                 diagnostics["lifting"]["residual_inf"].is_null()&&
                 diagnostics["lifting"]["residual_denominator"].is_null()&&
                 diagnostics.at("loads")==J::parse(reusable.solve(uniform.dump(),1.225,1.5e-5)),
                 "Separated uniform flow skips lifting without changing the loads");
        }
        J steady_winds=bad_winds;
        for(auto& wind:steady_winds)wind["wind_m_s"]={-10,1,0};
        const auto zero_input=bad_winds.dump(),steady_input=steady_winds.dump();
        const auto expected_zero=reusable.solve(zero_input,1.225,1.5e-5);
        const auto expected_steady=reusable.solve(steady_input,1.225,1.5e-5);
        auto cambered_input=in;
        for(auto& station:cambered_input["geometrie"]["flaechenform"]["profilschnitte"]["stationen"])
            station["woelbungsverhaeltnis"]=0.08;
        auto cambered_model=J::parse(segel::detail::prepare_model_data(cambered_input.dump()));
        segel::detail::PreparedModel cambered_solver(cambered_model.dump());
        J varying_winds=steady_winds;
        for(size_t id=0;id<varying_winds.size();++id)
            varying_winds[id]["wind_m_s"]=id==varying_winds.size()/2?
                J::array({0,0,0}):J::array({-10+0.2*id,1+0.1*id,0.3});
        auto detailed=J::parse(cambered_solver.solve_with_contributions(varying_winds.dump(),1.225,1.5e-5));
        check(detailed["streifen"]==J::parse(cambered_solver.solve(varying_winds.dump(),1.225,1.5e-5)),
              "Contribution diagnostic preserves normal strip output");
        std::vector<std::array<double,3>> contribution_forces(strips["streifen"].size()),
                                           contribution_moments(strips["streifen"].size());
        std::array<double,3> origin_force{},origin_moment{};
        bool intrinsic_seen=false;
        for(const auto& contribution:detailed["beitraege"]) {
            size_t id=contribution.at("streifen_id");
            auto f=contribution.at("kraft_N").get<std::array<double,3>>();
            auto r=contribution.at("ansatz_m").get<std::array<double,3>>();
            auto m=contribution.at("eigenmoment_Nm").get<std::array<double,3>>();
            const auto& query=cambered_model["streifen"][id];
            std::array<double,3> arm={r[0]-query["abfrage_x_m"].get<double>(),
                                      r[1]-query["abfrage_y_m"].get<double>(),
                                      r[2]-query["abfrage_z_m"].get<double>()};
            std::array<double,3> shifted={arm[1]*f[2]-arm[2]*f[1],
                                          arm[2]*f[0]-arm[0]*f[2],
                                          arm[0]*f[1]-arm[1]*f[0]};
            std::array<double,3> origin_shift={r[1]*f[2]-r[2]*f[1],
                                               r[2]*f[0]-r[0]*f[2],
                                               r[0]*f[1]-r[1]*f[0]};
            for(int k=0;k<3;++k) {
                contribution_forces[id][k]+=f[k];
                contribution_moments[id][k]+=shifted[k]+m[k];
                origin_force[k]+=f[k];
                origin_moment[k]+=origin_shift[k]+m[k];
                intrinsic_seen=intrinsic_seen||m[k]!=0;
            }
        }
        check(intrinsic_seen,"Contribution diagnostic retains free profile moments");
        for(size_t id=0;id<contribution_forces.size();++id)
            for(int k=0;k<3;++k) {
                check(std::abs(contribution_forces[id][k]-detailed["streifen"][id]["kraft_N"][k].get<double>())<1e-10,
                      "Individual forces sum to strip force");
                check(std::abs(contribution_moments[id][k]-detailed["streifen"][id]["moment_Nm"][k].get<double>())<1e-10,
                      "Individual shifted moments sum to strip moment");
            }
        std::array<double,6> strip_total{};
        for(const auto& row:detailed["streifen"]) {
            size_t id=row.at("streifen_id");
            auto f=row.at("kraft_N").get<std::array<double,3>>();
            auto m=row.at("moment_Nm").get<std::array<double,3>>();
            const auto& query=cambered_model["streifen"][id];
            std::array<double,3> r{query["abfrage_x_m"],query["abfrage_y_m"],query["abfrage_z_m"]};
            std::array<double,3> arm={r[1]*f[2]-r[2]*f[1],r[2]*f[0]-r[0]*f[2],r[0]*f[1]-r[1]*f[0]};
            for(int k=0;k<3;++k) {strip_total[k]+=f[k];strip_total[k+3]+=m[k]+arm[k];}
        }
        for(int k=0;k<3;++k) {
            check(std::abs(origin_force[k]-strip_total[k])<1e-10,"Individual forces sum to total force");
            check(std::abs(origin_moment[k]-strip_total[k+3])<1e-10,"Individual moments sum at the common origin");
        }
        auto balance=segel::detail::compare_contribution_balance(cambered_model,detailed,100);
        check(balance["anzahl"]==detailed["beitraege"].size()&&
              balance["schlechtester_vergleich"]["quotient"].get<double>()<1e-8,
              "Contribution balance checks both strip and total loads");
        auto damaged_details=detailed;
        damaged_details["beitraege"][0]["eigenmoment_Nm"][2]=10;
        auto bad_balance=segel::detail::compare_contribution_balance(cambered_model,damaged_details,100);
        check(bad_balance["schlechtester_vergleich"]["quotient"].get<double>()>1,
              "Contribution balance detects a changed free moment");
        for(int modes:{32,48,64}) {
            auto loads=J::parse(reusable.solve_modes(steady_input,1.225,1.5e-5,modes));
            check(loads.size()==strips["streifen"].size(),"Mode-specific solve returns all strip loads");
            for(const auto& load:loads)
                check(std::isfinite(length(load["kraft_N"]))&&std::isfinite(length(load["moment_Nm"])),
                      "Mode-specific solve is finite");
            auto actual=total_strips(loads);
            double velocity=std::hypot(10.,1.),alpha=std::atan2(-1.,10.)*180/3.141592653589793;
            auto h1=J::parse(ctx.solve(alpha,0,velocity*2/1.5e-5,4,false,modes));
            double q=0.5*1.225*velocity*velocity,force_error=0,moment_error=0;
            for(int k=0;k<3;++k) {
                force_error+=std::pow(actual[k]-q*8*h1["CF"][k].get<double>(),2);
                moment_error+=std::pow(actual[k+3]-q*8*2*h1["CM"][k].get<double>(),2);
            }
            check(std::sqrt(force_error)<=0.001*q*8+1e-9,"Mode-specific uniform force matches H1");
            check(std::sqrt(moment_error)<=0.001*q*8*2+1e-9,"Mode-specific uniform moment matches H1");
        }
        check(reusable.solve_modes(steady_input,1.225,1.5e-5,48)==expected_steady,"48 modes is the normal solve");
        rejects([&]{reusable.solve_modes(steady_input,1.225,1.5e-5,16);});
        check(expected_steady==segel::detail::solve_prepared_case(prepared.dump(),steady_input,1.225,1.5e-5),
              "Reusable model produces the same result");
        std::vector<std::future<std::string>> parallel;
        for(int i=0;i<8;++i)
            parallel.push_back(std::async(std::launch::async,[&,i]{
                return reusable.solve(i%2?steady_input:zero_input,1.225,1.5e-5);
            }));
        for(size_t i=0;i<parallel.size();++i)
            check(parallel[i].get()==(i%2?expected_steady:expected_zero),"Parallel prepared solves are independent");
        J localized=bad_winds;
        J two_strips={{"streifen",J::array({
            {{"streifen_id",0},{"abfrage_z_m",0.25}},
            {{"streifen_id",1},{"abfrage_z_m",0.75}}})}};
        J four_strips={{"streifen",J::array({
            {{"streifen_id",0},{"abfrage_z_m",0.125}},
            {{"streifen_id",1},{"abfrage_z_m",0.375}},
            {{"streifen_id",2},{"abfrage_z_m",0.625}},
            {{"streifen_id",3},{"abfrage_z_m",0.875}}})}};
        for(bool reverse:{false,true}) {
            double vmax=0;
            auto coarse_wind=segel::detail::local_zero_winds(two_strips,two_strips,reverse,vmax);
            auto fine_wind=segel::detail::local_zero_winds(two_strips,four_strips,reverse,vmax);
            check(coarse_wind[reverse?1:0]["wind_m_s"][0]==0&&
                  coarse_wind[reverse?0:1]["wind_m_s"][0]==-5,
                  "Two-strip local zero is at one interior point");
            check(fine_wind[reverse?2:1]["wind_m_s"][0]==-1.25&&
                  fine_wind[reverse?1:2]["wind_m_s"][0]==-3.75&&vmax==5,
                  "Two-strip local-zero field is interpolated at fine points");
        }
        J one_strip={{"z_min_m",0},{"hoehe_m",1},{"streifen",J::array({
            {{"streifen_id",0},{"abfrage_z_m",0.5}}})}};
        for(bool reverse:{false,true}) {
            double vmax=0;
            auto fine_wind=segel::detail::local_zero_winds(one_strip,two_strips,reverse,vmax);
            check(fine_wind[reverse?1:0]["wind_m_s"][0]==-1.25&&
                  fine_wind[reverse?0:1]["wind_m_s"][0]==-3.75&&vmax==3.75,
                  "Single-strip local-zero field reaches one contour endpoint");
        }
        for(auto& wind:localized)wind["wind_m_s"]={-5,0,0};
        localized[localized.size()/2]["wind_m_s"]={0,0,0};
        auto localized_result=total_strips(J::parse(reusable.solve(localized.dump(),1.225,1.5e-5)));
        localized[localized.size()/2]["wind_m_s"]={-1e-6,0,0};
        auto near_zero_result=total_strips(J::parse(reusable.solve(localized.dump(),1.225,1.5e-5)));
        for(size_t i=0;i<localized_result.size();++i)
            check(std::isfinite(localized_result[i])&&std::abs(localized_result[i]-near_zero_result[i])<1e-3,
                  "Local zero-wind limit is finite and continuous");
        J vertical=bad_winds;
        for(auto& wind:vertical)wind["wind_m_s"]={0,0,5};
        for(double value:total_strips(J::parse(reusable.solve(vertical.dump(),1.225,1.5e-5))))
            check(std::isfinite(value),"Exactly vertical wind remains finite");
        J seam=bad_winds;
        for(auto& wind:seam)wind["wind_m_s"]={5,5e-8,0};
        auto seam_left=total_strips(J::parse(reusable.solve(seam.dump(),1.225,1.5e-5)));
        for(auto& wind:seam)wind["wind_m_s"]={5,-5e-8,0};
        auto seam_right=total_strips(J::parse(reusable.solve(seam.dump(),1.225,1.5e-5)));
        for(size_t i=0;i<seam_left.size();++i)
            check(std::abs(seam_left[i]-seam_right[i])<1e-3,"Azimuth seam is continuous");
        auto uniform_loads=[&](double alpha,double beta,double velocity) {
            constexpr double pi=3.14159265358979323846;
            double a=alpha*pi/180,b=beta*pi/180;
            std::array<double,3> vector=std::abs(beta)==90
                ?std::array<double,3>{0,0,-velocity*std::sin(b)}
                :std::array<double,3>{-velocity*std::cos(b)*std::cos(a),
                                      -velocity*std::cos(b)*std::sin(a),-velocity*std::sin(b)};
            J winds=bad_winds;
            for(auto& wind:winds)wind["wind_m_s"]=vector;
            return J::parse(reusable.solve(winds.dump(),1.225,1.5e-5));
        };
        auto check_transition=[&](const J& baseline,const J& side,double velocity) {
            double q=0.5*1.225*velocity*velocity;
            for(size_t i=0;i<baseline.size();++i) {
                double force_error=0,moment_error=0;
                for(int k=0;k<3;++k) {
                    force_error+=std::pow(baseline[i]["kraft_N"][k].get<double>()-
                                          side[i]["kraft_N"][k].get<double>(),2);
                    moment_error+=std::pow(baseline[i]["moment_Nm"][k].get<double>()-
                                           side[i]["moment_Nm"][k].get<double>(),2);
                }
                double area=strips["streifen"][i]["flaeche_m2"];
                check(std::sqrt(force_error)<=0.001*q*area+1e-9,"Transition strip force continuous");
                check(std::sqrt(moment_error)<=0.001*q*area*2+1e-9,"Transition strip moment continuous");
            }
        };
        constexpr double angle_step=1e-6;
        double stall_shift=std::log10((10*2/1.5e-5)/500000);
        for(double edge:{-180.,-120.,-(25+stall_shift),-(12+stall_shift),
                         12+stall_shift,25+stall_shift,120.,180.}) {
            auto baseline=uniform_loads(edge,0,10);
            check_transition(baseline,uniform_loads(edge-angle_step,0,10),10);
            check_transition(baseline,uniform_loads(edge+angle_step,0,10),10);
        }
        for(double edge:{-90.,-80.,-60.,60.,80.,90.}) {
            auto baseline=uniform_loads(-5,edge,10);
            if(edge>-90)check_transition(baseline,uniform_loads(-5,edge-angle_step,10),10);
            if(edge<90)check_transition(baseline,uniform_loads(-5,edge+angle_step,10),10);
        }
        auto re_transition=uniform_loads(-5,0,0.0075);
        check_transition(re_transition,uniform_loads(-5,0,0.0075*(1-1e-6)),0.0075);
        check_transition(re_transition,uniform_loads(-5,0,0.0075*(1+1e-6)),0.0075);
        double previous_force=std::numeric_limits<double>::infinity();
        double previous_moment=std::numeric_limits<double>::infinity();
        for(double velocity:{1e-3,1e-6,1e-9,1e-12}) {
            auto total=total_strips(uniform_loads(-5,0,velocity));
            double force=std::hypot(std::hypot(total[0],total[1]),total[2]);
            double moment=std::hypot(std::hypot(total[3],total[4]),total[5]);
            check(std::isfinite(force)&&force<previous_force&&
                  std::isfinite(moment)&&moment<previous_moment,
                  "Low-Re dimensional loads converge towards zero");
            previous_force=force;previous_moment=moment;
        }
        check(previous_force<1e-9&&previous_moment<1e-9,
              "Low-Re dimensional loads approach the absolute zero-wind tolerance");
        J opposing=bad_winds;
        for(size_t i=0;i<opposing.size();++i)
            opposing[i]["wind_m_s"]=i%2?J::array({5,0,0}):J::array({-5,0,0});
        auto opposing_exact=J::parse(reusable.solve(opposing.dump(),1.225,1.5e-5));
        for(double offset:{-5e-8,5e-8}) {
            auto side=opposing;
            for(size_t i=1;i<side.size();i+=2)side[i]["wind_m_s"]={5,offset,0};
            check_transition(opposing_exact,J::parse(reusable.solve(side.dump(),1.225,1.5e-5)),5);
        }
        J vertical_local=bad_winds;
        for(auto& wind:vertical_local)wind["wind_m_s"]={-5,0,0};
        vertical_local[vertical_local.size()/2]["wind_m_s"]={0,0,5};
        auto vertical_exact=J::parse(reusable.solve(vertical_local.dump(),1.225,1.5e-5));
        for(double offset:{-5e-8,5e-8}) {
            auto side=vertical_local;
            side[side.size()/2]["wind_m_s"]={offset,0,5};
            check_transition(vertical_exact,J::parse(reusable.solve(side.dump(),1.225,1.5e-5)),5);
        }
        auto invalid_reusable=steady_winds;
        invalid_reusable[1]["streifen_id"]=0;
        rejects([&]{reusable.solve(invalid_reusable.dump(),1.225,1.5e-5);});
        check(reusable.solve(steady_input,1.225,1.5e-5)==expected_steady,"Failed solve does not modify prepared model");
        auto damaged_model=prepared;
        damaged_model["kopplung"][0]["wert"]=999;
        rejects([&]{segel::detail::solve_prepared_case(damaged_model.dump(),bad_winds.dump(),1.225,1.5e-5);});
        damaged_model=prepared;
        damaged_model["flaechenpunkte"][0]["punkt_id"]=999;
        rejects([&]{segel::detail::solve_prepared_case(damaged_model.dump(),bad_winds.dump(),1.225,1.5e-5);});
        damaged_model=prepared;
        damaged_model["quadratur"][0]["dz_m"]=0.001;
        rejects([&]{segel::detail::validate_model_data_json(damaged_model.dump());});
        damaged_model=prepared;
        damaged_model["quadratur"][0]["chord_m"]=0.001;
        rejects([&]{segel::detail::validate_model_data_json(damaged_model.dump());});
        damaged_model=prepared;
        damaged_model["quadratur"][0]["a0"]=1.0;
        rejects([&]{segel::detail::validate_model_data_json(damaged_model.dump());});
        damaged_model=prepared;
        damaged_model["flaechenpunkte"][0]["position_x_m"]=100.0;
        rejects([&]{segel::detail::validate_model_data_json(damaged_model.dump());});
        bad_winds[1]["streifen_id"]=0;
        rejects([&]{segel::detail::solve_strip_case(in.dump(),bad_winds.dump(),1.225,1.5e-5);});
        bad_winds[1]["streifen_id"]=1;
        bad_winds[0]["wind_m_s"]={51,0,0};
        rejects([&]{segel::detail::solve_strip_case(in.dump(),bad_winds.dump(),1.225,1.5e-5);});
        if(argc>1&&std::string(argv[1])=="--gradient-acceptance") {
        auto fine_strips=J::parse(segel::detail::prepare_strip_geometry(in.dump(),2));
        auto gradient=[](const J& geometry) {
            J winds=J::array();
            for(const auto& row:geometry["streifen"]) {
                double z=row["abfrage_z_m"],v=1+9*z/4,alpha=(-45+90*z/4)*3.141592653589793/180;
                winds.push_back({{"streifen_id",row["streifen_id"]},{"wind_m_s",{-v*std::cos(alpha),-v*std::sin(alpha),0}}});
            }
            return winds;
        };
        auto coarse_winds=gradient(strips),fine_winds=gradient(fine_strips);
        auto coarse_loads=J::parse(segel::detail::solve_strip_case(in.dump(),coarse_winds.dump(),1.225,1.5e-5));
        auto fine_loads=J::parse(segel::detail::solve_strip_case(in.dump(),fine_winds.dump(),1.225,1.5e-5,2));
        double vmax=0;
        for(const auto& wind:fine_winds) vmax=std::max(vmax,length(wind["wind_m_s"]));
        double qmax=0.5*1.225*vmax*vmax;
        for(size_t i=0;i<coarse_loads.size();++i) {
            auto cf=coarse_loads[i]["kraft_N"].get<std::array<double,3>>();
            auto cm=coarse_loads[i]["moment_Nm"].get<std::array<double,3>>();
            const auto& cp=strips["streifen"][i];
            std::array<double,3> cpoint{cp["abfrage_x_m"],cp["abfrage_y_m"],cp["abfrage_z_m"]};
            std::array<double,3> ff{},fm{};
            for(size_t j=2*i;j<2*i+2;++j) {
                auto f=fine_loads[j]["kraft_N"].get<std::array<double,3>>();
                auto m=fine_loads[j]["moment_Nm"].get<std::array<double,3>>();
                const auto& fp=fine_strips["streifen"][j];
                std::array<double,3> r{fp["abfrage_x_m"].get<double>()-cpoint[0],
                    fp["abfrage_y_m"].get<double>()-cpoint[1],fp["abfrage_z_m"].get<double>()-cpoint[2]};
                std::array<double,3> arm{r[1]*f[2]-r[2]*f[1],r[2]*f[0]-r[0]*f[2],r[0]*f[1]-r[1]*f[0]};
                for(int k=0;k<3;++k) { ff[k]+=f[k];fm[k]+=m[k]+arm[k]; }
            }
            double ef=0,em=0;
            for(int k=0;k<3;++k) { ef+=std::pow(ff[k]-cf[k],2);em+=std::pow(fm[k]-cm[k],2); }
            double area=cp["flaeche_m2"];
            if(std::sqrt(ef)>0.05*qmax*area+1e-9)
                std::cerr<<"Gradient strip "<<i<<": force error="<<std::sqrt(ef)
                         <<" N, limit="<<0.05*qmax*area+1e-9<<" N\n";
            check(std::sqrt(ef)<=0.05*qmax*area+1e-9,"Gradient strip force converges under subdivision");
            check(std::sqrt(em)<=0.05*qmax*area*2+1e-9,"Gradient strip moment converges under subdivision");
        }
        }
        auto r=J::parse(ctx.solve(-5,0,1e6,4));
        check(r["CF"][1].get<double>()>0,"Lift sign");
        check(r["CF"][0].get<double>()<0,"Positive total drag");
        auto neg=J::parse(ctx.solve(5,0,1e6,4));
        check(std::abs(r["CF"][1].get<double>()+neg["CF"][1].get<double>())<1e-8,"Lift symmetry");
        auto pole=J::parse(ctx.solve(-180,90,1e6,4));
        check(pole["CF"]==J::parse(ctx.solve(77,90,1e6,4))["CF"],"Pole azimuth invariance");
        check(pole["CF"][2].get<double>()<0,"Spanwise skin drag not zero");
        for(double beta:{-90.,-89.999,-80.,-60.,-20.,-0.001,0.,0.001,20.,60.,80.,89.999,90.}) {
            for(double alpha:{-180.,-90.,0.,90.,179.}) {
                auto result=J::parse(ctx.solve(alpha,beta,1e6,4));
                check(std::isfinite(length(result["CF"]))&&std::isfinite(length(result["CM"])),
                      "Signed wind angle finite");
            }
        }
        for(double beta:{-90.,90.}) {
            auto reference=J::parse(ctx.solve(0,beta,1e6,4));
            for(double alpha:{-180.,-90.,45.,179.}) {
                auto result=J::parse(ctx.solve(alpha,beta,1e6,4));
                check(result["CF"]==reference["CF"]&&result["CM"]==reference["CM"],
                      "Signed pole azimuth invariance");
            }
        }
        rejects([&]{ctx.solve(0,-90.001,1e6,4);});
        rejects([&]{ctx.solve(0,90.001,1e6,4);});
        rejects([&]{ctx.solve(0,std::numeric_limits<double>::infinity(),1e6,4);});
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
        auto source_result=J::parse(segel::resultant(J{{"c_ref",2},{"loads",loads}}.dump()));
        auto world_result=J::parse(segel::resultant(J{{"c_ref",2},{"loads",transformed}}.dump()));
        auto rotate=[](const J& value) {
            return std::array<double,3>{-value[1].get<double>(),value[0].get<double>(),value[2].get<double>()};
        };
        auto expected_force=rotate(source_result["CF"]);
        auto expected_moment=rotate(source_result["CM"]);
        std::array<double,3> expected_moment_vec{
            expected_moment[0]+(3*expected_force[2]-4*expected_force[1])/2,
            expected_moment[1]+(4*expected_force[0]-2*expected_force[2])/2,
            expected_moment[2]+(2*expected_force[1]-3*expected_force[0])/2};
        auto world_force=world_result["CF"].get<std::array<double,3>>();
        auto world_moment=world_result["CM"].get<std::array<double,3>>();
        double moment_error=0;
        for(int i=0;i<3;++i)moment_error+=std::pow(world_moment[i]-expected_moment_vec[i],2);
        check(world_force==expected_force&&std::sqrt(moment_error)<1e-12,
              "Full moment transforms about the translated reference");
        J couple=J::array({{{"f",{1,0,0}},{"r",{0,1,0}},{"m",{0,0,0}}},
                           {{"f",{-1,0,0}},{"r",{0,-1,0}},{"m",{0,0,0}}}});
        auto pair=J::parse(segel::resultant(J{{"c_ref",2},{"loads",couple}}.dump()));
        check(pair["valid"]==false&&pair["CM"][2]==-1,"Force couple retains moment");
        auto huge=couple;huge[0]["f"]={1e308,1e308,1e308};
        rejects([&]{segel::resultant(J{{"c_ref",2},{"loads",huge}}.dump());});
        rejects([&]{ctx.solve(0,0,1e6,3);});
        auto damaged=in;damaged["geometrie"]["grundkontur"]["rand"]["kanten"][0]["punkt_ids"][1]="unknown";
        rejects([&]{segel::Context bad(damaged.dump());});
        auto triangle=in;
        triangle["geometrie"]["grundkontur"]["punkte"].erase(2);
        triangle["geometrie"]["grundkontur"]["rand"]["kanten"][0]["punkt_ids"]={"hals","clew","head","hals"};
        segel::Context tri(triangle.dump());
        segel::detail::SplitReference triangle_reference(triangle.dump());
        check(!triangle_reference.valid(1000)&&triangle_reference.valid(1e6),
              "H1 reference validity rejects local Reynolds numbers outside the range");
        check(std::abs(J::parse(tri.geometry())["S_m2"].get<double>()-4)<1e-12,"Triangle tip");
        auto triangular_strips=J::parse(segel::detail::prepare_strip_geometry(triangle.dump()));
        double triangular_area=0;
        for(const auto& strip:triangular_strips["streifen"]) triangular_area+=strip["flaeche_m2"].get<double>();
        check(std::abs(triangular_area-4)<1e-10,"Triangular strip areas preserve tip");
        check(length(J::parse(tri.solve(90,45,1e6,4))["CF"])>0,"High angle spatial load");
        auto shape=in;shape["geometrie"]["flaechenform"]["profilschnitte"]["stationen"][1]["woelbungsverhaeltnis"]=0.1;
        shape["geometrie"]["flaechenform"]["profilschnitte"]["stationen"][1]["verwindung_deg"]=-8;
        segel::Context curved(shape.dump());
        check(J::parse(curved.solve(0,0,1e6,4))["CF"]!=J::parse(ctx.solve(0,0,1e6,4))["CF"],"Camber/twist effect");
        auto signed_positive=J::parse(curved.solve(13,20,1e6,4));
        auto signed_negative=J::parse(curved.solve(13,-20,1e6,4));
        check(signed_positive["CF"]!=signed_negative["CF"]&&signed_positive["CM"]!=signed_negative["CM"],
              "Negative elevation is directly recomputed, not mirrored");
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
