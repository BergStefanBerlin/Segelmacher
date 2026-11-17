#pragma once
#include "segelform_load_compare.hpp"
#include <algorithm>
#include <array>
#include <cmath>

namespace segel::detail {
template<class CheckCancellation>
LoadJson check_wind_continuity(const LoadJson& model,const PreparedModel& solver,CheckCancellation check_cancel) {
    const auto& strips=model.at("streifen");
    const std::array<const char*,3> names={"gegenwind_naht","lokal_vertikal","lokaler_nullwind"};
    LoadJson results=LoadJson::array();
    for(size_t kind=0;kind<names.size();++kind) {
        LoadJson base=LoadJson::array();
        for(size_t id=0;id<strips.size();++id) {
            LoadVector wind{-5,0,0};
            if(kind==0&&(id%2||strips.size()==1))wind={5,0,0};
            if(kind==1&&id==strips.size()/2)wind={0,0,5};
            if(kind==2&&id==strips.size()/2)wind={0,0,0};
            base.push_back({{"streifen_id",id},{"wind_m_s",wind}});
        }
        check_cancel();
        auto reference=LoadJson::parse(solver.solve(base.dump(),1.225,1.5e-5));
        for(double delta:{-5e-8,5e-8}) {
            check_cancel();
            auto perturbed=base;
            if(kind==0) {
                for(size_t id=strips.size()==1?0:1;id<strips.size();id+=2)
                    perturbed[id]["wind_m_s"]={5,delta,0};
            } else if(kind==1)perturbed[strips.size()/2]["wind_m_s"]={delta,0,5};
            else perturbed[strips.size()/2]["wind_m_s"]={delta,0,0};
            auto candidate=LoadJson::parse(solver.solve(perturbed.dump(),1.225,1.5e-5));
            double vmax=0;
            for(const LoadJson* winds:{&base,&perturbed})
                for(const auto& row:*winds) {
                    LoadVector v=row.at("wind_m_s");
                    vmax=std::max(vmax,std::hypot(std::hypot(v[0],v[1]),v[2]));
                }
            double q=0.5*1.225*vmax*vmax;
            results.push_back({{"windfall",names[kind]},{"delta_m_s",delta},{"q_max_Pa",q},
                               {"c_ref_m",model.at("c_ref_m")},
                               {"schlechtester_vergleich",compare_same_grid_loads(model,reference,candidate,q)}});
        }
    }
    return results;
}
}
