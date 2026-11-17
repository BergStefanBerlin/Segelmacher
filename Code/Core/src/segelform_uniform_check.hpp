#pragma once
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

namespace segel::detail {
using UniformJson = nlohmann::json;
using UniformVector = std::array<double,3>;
inline UniformVector uniform_wind(double speed,double alpha,double beta) {
    constexpr double pi=3.14159265358979323846;
    double a=alpha*pi/180,b=beta*pi/180;
    UniformVector wind=std::abs(beta)==90?UniformVector{0,0,-speed*std::sin(b)}:
        UniformVector{-speed*std::cos(b)*std::cos(a),-speed*std::cos(b)*std::sin(a),-speed*std::sin(b)};
    double actual=std::hypot(std::hypot(wind[0],wind[1]),wind[2]);
    const auto components=std::count_if(wind.begin(),wind.end(),[](double value){return value!=0;});
    // Reserve two ULPs for component and norm rounding at the speed limit.
    const double safe_max=std::nextafter(std::nextafter(50.,0.),0.);
    if(speed==50&&(actual>50||components>1)) {
        for(int attempt=0;actual>safe_max&&attempt<4;++attempt) {
            double scale=std::nextafter(safe_max/actual,0.);
            for(double& component:wind)component*=scale;
            actual=std::hypot(std::hypot(wind[0],wind[1]),wind[2]);
        }
        if(actual>safe_max)
            throw std::runtime_error("Gleichwind: Geschwindigkeitsgrenze ueberschritten");
    }
    return wind;
}
inline UniformVector uniform_cross(const UniformVector& a,const UniformVector& b) {
    return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};
}
inline UniformVector uniform_point(const UniformJson& strip) {
    return {strip.at("abfrage_x_m"),strip.at("abfrage_y_m"),strip.at("abfrage_z_m")};
}
inline bool uniform_h1_valid(double speed,double nu,const UniformJson& model,
                             const UniformJson& direct_grid) {
    const auto& parameters=model.at("parameter");
    const double re_min=parameters.at("re_min"),re_max=parameters.at("re_max");
    const double re=speed*model.at("c_ref_m").get<double>()/nu;
    bool valid=re>=re_min&&re<=re_max;
    for(const auto& row:direct_grid.at("grids").at("4")) {
        double local=re*row.at("chord").get<double>()/model.at("c_ref_m").get<double>();
        valid=valid&&local>=re_min&&local<=re_max;
    }
    for(const auto& row:model.at("quadratur")) {
        double local=speed*row.at("chord_m").get<double>()/nu;
        valid=valid&&local>=re_min&&local<=re_max;
    }
    return valid;
}
struct UniformTransition {
    std::pair<double,double> center;
    std::vector<std::pair<double,double>> sides;
};
inline std::vector<UniformTransition> uniform_transition_groups(
        const UniformJson& model,double speed,double nu) {
    constexpr double pi=3.14159265358979323846,step=1e-6;
    std::vector<UniformTransition> groups;
    auto add_azimuth=[&](double angle) {
        groups.push_back({{std::remainder(angle,360.),0.},
                          {{std::remainder(angle-step,360.),0.},
                           {std::remainder(angle+step,360.),0.}}});
    };
    for(double angle:{-180.,-120.,120.,180.})add_azimuth(angle);
    for(double beta:{-90.,-80.,-60.,60.,80.,90.}) {
        UniformTransition group{{-5.,beta},{}};
        for(double offset:{-step,step})
            if(beta+offset>=-90&&beta+offset<=90)group.sides.emplace_back(-5.,beta+offset);
        groups.push_back(std::move(group));
    }
    const auto& parameters=model.at("parameter");
    const double re_min=parameters.at("re_min");
    for(const auto& row:model.at("quadratur")) {
        double local_re=speed*row.at("chord_m").get<double>()/nu;
        double shift=parameters.at("stall_re_shift_deg").get<double>();
        double transition=parameters.at("transition_re").get<double>();
        double delta=local_re>=re_min?shift*std::log10(local_re/transition):
            shift*(std::log10(re_min/transition)+(local_re/re_min-1)/std::log(10.));
        double twist=row.at("twist_rad").get<double>()*180/pi;
        for(const char* name:{"stall_start_deg","stall_end_deg"}) {
            double edge=parameters.at(name).get<double>()+delta;
            add_azimuth(twist+edge);
            add_azimuth(twist-edge);
        }
    }
    return groups;
}
inline std::set<std::pair<double,double>> uniform_transition_cases(
        const UniformJson& model,double speed,double nu) {
    std::set<std::pair<double,double>> cases;
    for(const auto& group:uniform_transition_groups(model,speed,nu)) {
        cases.insert(group.center);
        cases.insert(group.sides.begin(),group.sides.end());
    }
    return cases;
}
inline UniformJson compare_uniform_h1(const UniformJson& model,const UniformJson& result,
                                      const UniformJson& reference,const UniformJson& finer_reference,
                                      const UniformJson& direct_reference,double speed,double rho) {
    const double cref=model.at("c_ref_m"),area=model.at("S_m2");
    const double q=0.5*rho*speed*speed,scale=q*area;
    double worst=-1;
    UniformJson diagnostic;
    auto compare=[&](UniformVector a,UniformVector b,double reference_area,bool moment,int id,const char* kind) {
        double limit=0.001*q*reference_area*(moment?cref:1)+1e-9;
        UniformVector diff{a[0]-b[0],a[1]-b[1],a[2]-b[2]};
        double error=std::hypot(std::hypot(diff[0],diff[1]),diff[2]);
        if(!std::isfinite(limit)||limit<=0||!std::isfinite(error))
            throw std::runtime_error("H1-Vergleich: Abweichung oder Grenze nicht darstellbar");
        double ratio=error/limit;
        if(!std::isfinite(ratio))
            throw std::runtime_error("H1-Vergleich: Grenzquotient nicht darstellbar");
        if(ratio>worst) {
            worst=ratio;
            diagnostic={{"quotient",ratio},{"abweichung",error},{"grenze",limit},
                        {"flaeche_m2",reference_area},
                        {"wert_a",a},{"wert_b",b},
                        {"streifen_id",id<0?UniformJson(nullptr):UniformJson(id)},{"groesse",kind}};
        }
    };
    UniformVector original_force=reference.at("CF"),original_moment=reference.at("CM");
    UniformVector direct_force=direct_reference.at("CF"),direct_moment=direct_reference.at("CM");
    UniformVector refined_force=finer_reference.at("CF"),refined_moment=finer_reference.at("CM");
    for(int k=0;k<3;++k) {
        original_force[k]*=scale;direct_force[k]*=scale;refined_force[k]*=scale;
        original_moment[k]*=scale*cref;direct_moment[k]*=scale*cref;refined_moment[k]*=scale*cref;
    }
    compare(original_force,direct_force,area,false,-1,"h1_split_force");
    compare(original_moment,direct_moment,area,true,-1,"h1_split_moment");
    compare(original_force,refined_force,area,false,-1,"h1_refinement_force");
    compare(original_moment,refined_moment,area,true,-1,"h1_refinement_moment");
    std::vector<UniformVector> ref_forces(result.size()),ref_moments(result.size());
    std::vector<UniformVector> fine_forces(result.size()),fine_moments(result.size());
    auto accumulate=[&](const UniformJson& loads,std::vector<UniformVector>& forces,
                        std::vector<UniformVector>& moments) {
        for(const auto& load:loads) {
            UniformVector position=load.at("r"),f=load.at("f"),m=load.at("m");
            size_t id=std::min(result.size()-1,static_cast<size_t>(
                std::floor((position[2]-model.at("z_min_m").get<double>())/
                           model.at("hoehe_m").get<double>()*result.size())));
            auto query=uniform_point(model.at("streifen")[id]);
            UniformVector arm{position[0]-query[0],position[1]-query[1],position[2]-query[2]};
            for(double& value:f)value*=scale;
            auto moment=uniform_cross(arm,f);
            for(int k=0;k<3;++k) {
                forces[id][k]+=f[k];
                moments[id][k]+=moment[k]+m[k]*scale*cref;
            }
        }
    };
    accumulate(reference.at("loads"),ref_forces,ref_moments);
    accumulate(finer_reference.at("loads"),fine_forces,fine_moments);
    UniformVector actual_force{},actual_moment{};
    for(size_t id=0;id<result.size();++id) {
        UniformVector f=result[id].at("kraft_N"),m=result[id].at("moment_Nm");
        double strip_area=model.at("streifen")[id].at("flaeche_m2");
        compare(f,ref_forces[id],strip_area,false,static_cast<int>(id),"strip_force");
        compare(m,ref_moments[id],strip_area,true,static_cast<int>(id),"strip_moment");
        compare(ref_forces[id],fine_forces[id],strip_area,false,static_cast<int>(id),"h1_strip_refinement_force");
        compare(ref_moments[id],fine_moments[id],strip_area,true,static_cast<int>(id),"h1_strip_refinement_moment");
        auto arm=uniform_cross(uniform_point(model.at("streifen")[id]),f);
        for(int k=0;k<3;++k) {actual_force[k]+=f[k];actual_moment[k]+=m[k]+arm[k];}
    }
    compare(actual_force,original_force,area,false,-1,"total_force");
    compare(actual_moment,original_moment,area,true,-1,"total_moment");
    return diagnostic;
}
}
