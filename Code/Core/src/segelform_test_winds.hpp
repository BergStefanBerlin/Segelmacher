#pragma once
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

namespace segel::detail {
enum class EdgeWindCase { counterflow, azimuth_seam, local_vertical };
inline nlohmann::json edge_winds(const nlohmann::json& coarse,const nlohmann::json& target,
                                  EdgeWindCase type,bool reverse,double& vmax) {
    const auto& strips=coarse.at("streifen");
    if(strips.empty())throw std::invalid_argument("Windsonderfall erfordert Streifen");
    const double zmin=coarse.at("z_min_m"),height=coarse.at("hoehe_m");
    const double center=(strips[strips.size()/2].at("abfrage_z_m").get<double>()-zmin)/height;
    nlohmann::json winds=nlohmann::json::array();
    for(const auto& row:target.at("streifen")) {
        double t=(row.at("abfrage_z_m").get<double>()-zmin)/height-center;
        if(reverse)t=-t;
        std::array<double,3> v{};
        switch(type) {
        case EdgeWindCase::counterflow: v={10*t,0,0};break;
        case EdgeWindCase::azimuth_seam: v={5,2*t,0};break;
        case EdgeWindCase::local_vertical: v={10*t,0,5};break;
        }
        vmax=std::max(vmax,std::hypot(std::hypot(v[0],v[1]),v[2]));
        winds.push_back({{"streifen_id",row.at("streifen_id")},{"wind_m_s",v}});
    }
    return winds;
}
inline nlohmann::json local_zero_winds(const nlohmann::json& source,
                                        const nlohmann::json& target,
                                        bool reverse,double& vmax) {
    const auto& coarse=source.at("streifen");
    if(coarse.empty()) throw std::invalid_argument("Lokaler Nullwind erfordert Streifen");
    nlohmann::json winds=nlohmann::json::array();
    size_t zero=coarse.size()/3;
    if(coarse.size()>2) zero=std::clamp(zero,size_t{1},coarse.size()-2);
    if(reverse&&coarse.size()>1) zero=coarse.size()-1-zero;
    for(const auto& row:target.at("streifen")) {
        double z=row.at("abfrage_z_m"),speed=0;
        if(coarse.size()==1) {
            double t=(z-source.at("z_min_m").get<double>())/source.at("hoehe_m").get<double>();
            speed=5*(reverse?1-t:t);
        } else {
            size_t hi=0;
            while(hi<coarse.size()&&coarse[hi].at("abfrage_z_m").get<double>()<z) ++hi;
            auto value=[&](size_t id){return id==zero?0.0:5.0;};
            if(hi==0) speed=value(0);
            else if(hi==coarse.size()) speed=value(coarse.size()-1);
            else {
                double a=coarse[hi-1].at("abfrage_z_m"),b=coarse[hi].at("abfrage_z_m");
                double t=(z-a)/(b-a);
                speed=(1-t)*value(hi-1)+t*value(hi);
            }
        }
        vmax=std::max(vmax,speed);
        winds.push_back({{"streifen_id",row.at("streifen_id")},{"wind_m_s",{-speed,0,0}}});
    }
    return winds;
}
}
