#pragma once
#include "segelform_load_compare.hpp"
#include <map>
#include <utility>

namespace segel::detail {
template<class CheckCancellation>
LoadJson check_re_boundaries(const LoadJson& model,const PreparedModel& solver,CheckCancellation check_cancel) {
    constexpr double rho=1.225,nu=1.5e-5,angle=-5*3.14159265358979323846/180;
    constexpr double offset=1e-6;
    const auto& strips=model.at("streifen");
    const double re_min=model.at("parameter").at("re_min");
    std::map<double,LoadJson> checked;
    size_t outside=0;
    LoadJson worst=nullptr;
    LoadJson cases=LoadJson::array();
    auto solve=[&](double speed) {
        LoadJson winds=LoadJson::array();
        for(const auto& strip:strips)
            winds.push_back({{"streifen_id",strip.at("streifen_id")},
                             {"wind_m_s",{-speed*std::cos(angle),-speed*std::sin(angle),0}}});
        return LoadJson::parse(solver.solve(winds.dump(),rho,nu));
    };
    for(const auto& row:model.at("quadratur")) {
        check_cancel();
        double chord=row.at("chord_m");
        double speed=re_min*nu/chord;
        if(!std::isfinite(speed)||speed<=0)
            throw std::runtime_error("Re_min-Diagnose: ungueltige Grenzgeschwindigkeit");
        if(speed*(1+offset)>50) {
            ++outside;
            cases.push_back({{"punkt_id",row.at("punkt_id")},{"streifen_id",row.at("streifen_id")},
                             {"chord_m",chord},{"geschwindigkeit_m_s",speed},
                             {"status","ausserhalb_0_bis_50_m_s"},{"vergleiche",LoadJson::array()}});
            continue;
        }
        auto found=checked.find(speed);
        if(found==checked.end()) {
            auto reference=solve(speed);
            LoadJson comparisons=LoadJson::array();
            double q=0.5*rho*speed*speed*(1+offset)*(1+offset);
            for(double multiplier:{1-offset,1+offset}) {
                check_cancel();
                comparisons.push_back({{"faktor",multiplier},
                    {"vergleich",compare_same_grid_loads(model,reference,solve(speed*multiplier),q)}});
            }
            found=checked.emplace(speed,std::move(comparisons)).first;
        }
        for(const auto& comparison:found->second) {
            const auto& value=comparison.at("vergleich");
            if(worst.is_null()||value.at("quotient").get<double>()>
                                worst.at("vergleich").at("quotient").get<double>()) {
                double upper=speed*(1+offset);
                worst={{"punkt_id",row.at("punkt_id")},{"streifen_id",row.at("streifen_id")},
                       {"chord_m",chord},{"geschwindigkeit_m_s",speed},
                       {"faktor",comparison.at("faktor")},{"q_max_Pa",0.5*rho*upper*upper},
                       {"vergleich",value}};
            }
        }
        cases.push_back({{"punkt_id",row.at("punkt_id")},{"streifen_id",row.at("streifen_id")},
                         {"chord_m",chord},{"geschwindigkeit_m_s",speed},{"status","geprueft"},
                         {"vergleiche",found->second}});
    }
    return {{"quadraturpunkte",model.at("quadratur").size()},
            {"gepruefte_punkte",model.at("quadratur").size()-outside},
            {"ausserhalb_0_bis_50_m_s",outside},{"verschiedene_grenzgeschwindigkeiten",checked.size()},
            {"dichte_kg_m3",rho},{"viskositaet_m2_s",nu},{"azimut_deg",-5},
            {"relativer_abstand",offset},{"schlechtester_vergleich",worst},{"faelle",std::move(cases)}};
}
}
