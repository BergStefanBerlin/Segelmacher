#pragma once
#include "segelform_internal.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace segel::detail {
using LoadJson=nlohmann::json;
using LoadVector=std::array<double,3>;
using CompactLoads=std::vector<std::array<double,6>>;
inline LoadVector load_cross(const LoadVector& a,const LoadVector& b) {
    return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};
}
inline std::pair<LoadVector,LoadVector> load_components(const LoadJson& loads,size_t id) {
    return {loads[id].at("kraft_N").get<LoadVector>(),loads[id].at("moment_Nm").get<LoadVector>()};
}
inline std::pair<LoadVector,LoadVector> load_components(const CompactLoads& loads,size_t id) {
    const auto& row=loads[id];
    return {{row[0],row[1],row[2]},{row[3],row[4],row[5]}};
}
template<class Loads>
LoadJson compare_same_grid_loads(const LoadJson& geometry,const Loads& reference,
                                 const Loads& candidate,double q) {
    const auto& strips=geometry.at("streifen");
    if(reference.size()!=strips.size()||candidate.size()!=strips.size())
        throw std::runtime_error("Lastvergleich: Streifenzahl stimmt nicht");
    const double cref=geometry.at("c_ref_m");
    LoadVector total_ref{},total_candidate{},moment_ref{},moment_candidate{};
    double worst=-1;
    LoadJson diagnostic;
    auto compare=[&](const LoadVector& a,const LoadVector& b,double area,bool moment,const LoadJson& id) {
        LoadVector difference{a[0]-b[0],a[1]-b[1],a[2]-b[2]};
        double error=std::hypot(std::hypot(difference[0],difference[1]),difference[2]);
        double limit=0.001*q*area*(moment?cref:1)+1e-9;
        if(!std::isfinite(error)||!std::isfinite(limit)||limit<=0)
            throw std::runtime_error("Lastvergleich: Abweichung oder Bezug nicht darstellbar");
        double ratio=error/limit;
        if(!std::isfinite(ratio))throw std::runtime_error("Lastvergleich: Grenzquotient nicht darstellbar");
        if(ratio>worst) {
            worst=ratio;
            diagnostic={{"streifen_id",id},{"groesse",moment?"moment_Nm":"kraft_N"},
                        {"abweichung",error},{"grenze",limit},{"flaeche_m2",area},{"quotient",ratio}};
        }
    };
    for(size_t id=0;id<strips.size();++id) {
        auto [ref_force,ref_moment]=load_components(reference,id);
        auto [new_force,new_moment]=load_components(candidate,id);
        LoadVector position{strips[id].at("abfrage_x_m"),strips[id].at("abfrage_y_m"),
                            strips[id].at("abfrage_z_m")};
        auto ref_arm=load_cross(position,ref_force),new_arm=load_cross(position,new_force);
        for(int k=0;k<3;++k) {
            total_ref[k]+=ref_force[k];total_candidate[k]+=new_force[k];
            moment_ref[k]+=ref_moment[k]+ref_arm[k];
            moment_candidate[k]+=new_moment[k]+new_arm[k];
        }
        double area=strips[id].at("flaeche_m2");
        compare(ref_force,new_force,area,false,id);
        compare(ref_moment,new_moment,area,true,id);
    }
    double area=geometry.at("S_m2");
    compare(total_ref,total_candidate,area,false,nullptr);
    compare(moment_ref,moment_candidate,area,true,nullptr);
    return diagnostic;
}
inline LoadJson compare_contribution_balance(const LoadJson& geometry,const LoadJson& detailed,double q) {
    const auto& strips=geometry.at("streifen");
    const auto& contributions=detailed.at("beitraege");
    std::vector<LoadVector> forces(strips.size()),moments(strips.size());
    for(const auto& item:contributions) {
        size_t id=item.at("streifen_id");
        if(id>=strips.size())throw std::runtime_error("Beitragsbilanz: ungueltige Streifen-ID");
        LoadVector force=item.at("kraft_N"),position=item.at("ansatz_m"),intrinsic=item.at("eigenmoment_Nm");
        LoadVector arm{position[0]-strips[id].at("abfrage_x_m").get<double>(),
                       position[1]-strips[id].at("abfrage_y_m").get<double>(),
                       position[2]-strips[id].at("abfrage_z_m").get<double>()};
        auto shifted=load_cross(arm,force);
        for(int k=0;k<3;++k) {
            forces[id][k]+=force[k];
            moments[id][k]+=shifted[k]+intrinsic[k];
        }
    }
    LoadJson reconstructed=LoadJson::array();
    for(size_t id=0;id<strips.size();++id)
        reconstructed.push_back({{"streifen_id",id},{"kraft_N",forces[id]},{"moment_Nm",moments[id]}});
    return {{"anzahl",contributions.size()},
            {"schlechtester_vergleich",compare_same_grid_loads(geometry,detailed.at("streifen"),reconstructed,q)}};
}
template<class CheckCancellation>
LoadJson check_zero_sequence(const LoadJson& model,const PreparedModel& solver,CheckCancellation check_cancel) {
    const auto& strips=model.at("streifen");
    double previous_force=std::numeric_limits<double>::infinity();
    double previous_moment=std::numeric_limits<double>::infinity();
    LoadJson results=LoadJson::array();
    for(double speed:{1e-3,1e-4,1e-6,1e-8,1e-10,1e-12,0.0}) {
        check_cancel();
        LoadJson winds=LoadJson::array();
        for(const auto& strip:strips)
            winds.push_back({{"streifen_id",strip.at("streifen_id")},{"wind_m_s",{-speed,0,0}}});
        auto loads=LoadJson::parse(solver.solve(winds.dump(),1.225,1.5e-5));
        LoadVector total_force{},total_moment{};
        double max_force=0,max_moment=0;
        for(size_t id=0;id<strips.size();++id) {
            LoadVector force=loads[id].at("kraft_N"),moment=loads[id].at("moment_Nm");
            LoadVector position{strips[id].at("abfrage_x_m"),strips[id].at("abfrage_y_m"),
                                strips[id].at("abfrage_z_m")};
            auto arm=load_cross(position,force);
            for(int k=0;k<3;++k) {
                total_force[k]+=force[k];total_moment[k]+=moment[k]+arm[k];
            }
            double strip_force=std::hypot(std::hypot(force[0],force[1]),force[2]);
            double strip_moment=std::hypot(std::hypot(moment[0],moment[1]),moment[2]);
            if(!std::isfinite(strip_force)||!std::isfinite(strip_moment))
                throw std::runtime_error("Nullwindfolge: nicht endliche Streifenlast");
            max_force=std::max(max_force,strip_force);
            max_moment=std::max(max_moment,strip_moment);
        }
        double force=std::hypot(std::hypot(total_force[0],total_force[1]),total_force[2]);
        double moment=std::hypot(std::hypot(total_moment[0],total_moment[1]),total_moment[2]);
        if(!std::isfinite(force)||!std::isfinite(moment)||!std::isfinite(max_force)||!std::isfinite(max_moment))
            throw std::runtime_error("Nullwindfolge: nicht endliche Last");
        if(force>previous_force+1e-9||moment>previous_moment+1e-9)
            throw std::runtime_error("Nullwindfolge: Gesamtlast nicht abnehmend");
        if(speed<=1e-12&&(force>1e-9||moment>1e-9||max_force>1e-9||max_moment>1e-9))
            throw std::runtime_error("Nullwindfolge: Last oberhalb der absoluten Toleranz");
        if(speed==0&&(force!=0||moment!=0||max_force!=0||max_moment!=0))
            throw std::runtime_error("Nullwindfolge: Last bei global null nicht exakt null");
        results.push_back({{"geschwindigkeit_m_s",speed},{"gesamtkraft_N",force},{"gesamtmoment_Nm",moment},
                           {"max_streifenkraft_N",max_force},{"max_streifenmoment_Nm",max_moment}});
        previous_force=force;previous_moment=moment;
    }
    return results;
}
}
