#include "../src/segelform_internal.hpp"
#include "../src/segelform_uniform_check.hpp"
#include "segel/core.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

using J=nlohmann::json;
using V=std::array<double,3>;
namespace {
J read(const char* path) {
    std::ifstream input(path);
    if(!input)throw std::runtime_error(std::string("Cannot open ")+path);
    return J::parse(input);
}
double norm(V a) {return std::hypot(std::hypot(a[0],a[1]),a[2]);}
}
int main(int argc,char** argv) {
    bool jsonl_loads=argc>4&&std::string(argv[argc-1])=="--jsonl-loads";
    bool jsonl=jsonl_loads||(argc>4&&std::string(argv[argc-1])=="--jsonl");
    if(jsonl)--argc;
    bool full_matrix=argc>=5&&std::string(argv[4])=="--matrix";
    bool model_transitions=argc>=5&&std::string(argv[4])=="--model-transitions";
    bool model_poles=argc==5&&std::string(argv[4])=="--model-poles";
    bool transitions=(argc==5||argc==6||argc==8)&&
        (std::string(argv[4])=="--transitions"||full_matrix||model_transitions);
    bool single_case=(argc==7||argc==8)&&std::string(argv[4])=="--case";
    bool lifting_diagnostic=argc==5&&std::string(argv[4])=="--lifting";
    if(argc!=4&&!transitions&&!single_case&&!lifting_diagnostic&&!model_poles) {
        std::cerr<<"Usage: strip_uniform_probe geometry.json job.json speed_m_s [--transitions|--matrix|--model-transitions [grid-refinement [part parts]]|--model-poles|--case alpha beta [grid-refinement]|--lifting] [--jsonl|--jsonl-loads]\n";
        return 2;
    }
    if(jsonl&&lifting_diagnostic) {
        std::cerr<<"--jsonl ist fuer Gleichwindfaelle vorgesehen, nicht fuer --lifting\n";
        return 2;
    }
    if((model_transitions||model_poles)&&!jsonl_loads) {
        std::cerr<<"Modell-Winkelpruefung erfordert --jsonl-loads\n";
        return 2;
    }
    std::string phase="Eingaben";
    double current_alpha=0,current_beta=0;
    try {
        constexpr double pi=3.14159265358979323846,rho=1.225,nu=1.5e-5;
        double speed=std::stod(argv[3]);
        if(!std::isfinite(speed)||speed<0||speed>50)throw std::invalid_argument("Invalid speed");
        J job=read(argv[2]),input={{"geometrie",read(argv[1])},{"numerik",job.at("numerik")},
                                  {"parameter",job.at("berechnung").at("parameter")}};
        J original_input=input;
        double refinement=single_case&&argc==8?std::stod(argv[7]):
            (transitions&&argc>=6?std::stod(argv[5]):1);
        if(!std::isfinite(refinement)||refinement<1)throw std::invalid_argument("Invalid grid refinement");
        auto& base_grid=input["numerik"]["hoehenraster"];
        base_grid["schritt_m"]=base_grid["schritt_m"].get<double>()/refinement;
        base_grid["verfeinerter_schritt_m"]=base_grid["verfeinerter_schritt_m"].get<double>()/refinement;
        std::string source=input.dump();
        phase="Modellaufbereitung";
        J model=J::parse(segel::detail::prepare_model_data(source));
        phase="Vorbereitetes Modell";
        segel::detail::PreparedModel solver(model.dump());
        phase="H1-Referenzen";
        segel::Context direct(original_input.dump());
        J refined_input=input;
        auto& grid=refined_input["numerik"]["hoehenraster"];
        grid["schritt_m"]=grid["schritt_m"].get<double>()/2;
        grid["verfeinerter_schritt_m"]=grid["verfeinerter_schritt_m"].get<double>()/2;
        std::string refined_source=refined_input.dump();
        double cref=model.at("c_ref_m"),re=speed*cref/nu;
        J direct_grid=J::parse(direct.geometry());
        segel::detail::SplitReference split_reference(source),refined_reference(refined_source);
        bool valid=segel::detail::uniform_h1_valid(speed,nu,model,direct_grid)&&
                   split_reference.valid(re)&&refined_reference.valid(re);
        if(lifting_diagnostic) {
            if(!valid)throw std::runtime_error("H1-Liftingdiagnose ausserhalb des Gueltigkeitsbereichs");
            double worst=0;
            auto uniform=[&](double alpha,double beta) {
                double a=alpha*pi/180,b=beta*pi/180;
                V wind{-speed*std::cos(b)*std::cos(a),-speed*std::cos(b)*std::sin(a),-speed*std::sin(b)};
                J winds=J::array();
                for(const auto& strip:model.at("streifen"))
                    winds.push_back({{"streifen_id",strip.at("streifen_id")},{"wind_m_s",wind}});
                return winds;
            };
            for(const auto& angles:std::array<std::array<double,2>,3>{{{-5,0},{5,30},{0,-30}}}) {
                auto winds=uniform(angles[0],angles[1]);
                auto result=solver.solve_with_lifting_diagnostics(winds.dump(),rho,nu);
                auto reference=split_reference.lifting_modes(angles[0],angles[1]);
                if(result.at("lifting").at("solved")!=true||reference.size()!=48)
                    throw std::runtime_error("H1-Liftingdiagnose: Loesung fehlt");
                double horizontal=speed*std::cos(angles[1]*pi/180);
                for(size_t i=0;i<reference.size();++i) {
                    double expected=horizontal*reference[i].get<double>();
                    double actual=result.at("lifting").at("coefficients")[i].get<double>();
                    worst=std::max(worst,std::abs(actual-expected)/(1e-10*std::max(1.0,std::abs(expected))));
                }
            }
            for(double alpha:{-180.,-120.,120.,180.}) {
                auto winds=uniform(alpha,0);
                auto result=solver.solve_with_lifting_diagnostics(winds.dump(),rho,nu);
                if(result.at("lifting").at("solved")!=false)
                    throw std::runtime_error("H1-Liftingdiagnose: abgeloeste Stroemung loest Traglinie");
            }
            std::cout<<model.at("segel_id")<<" lifting_cases=3 separated_cases=4 max_ratio="<<worst<<'\n';
            return worst<=1?0:1;
        }
        phase="Fallauswahl";
        std::set<std::pair<double,double>> cases;
        if(model_poles) {
            for(double beta:{-90.,90.})
                for(int alpha=-180;alpha<180;alpha+=30)cases.emplace(alpha,beta);
        } else if(single_case) {
            double alpha=std::stod(argv[5]),beta=std::stod(argv[6]);
            if(!std::isfinite(alpha)||!std::isfinite(beta)||beta< -90||beta>90)
                throw std::invalid_argument("Invalid wind angle");
            cases.emplace(alpha,beta);
        } else if(!transitions) {
            for(double beta:{-90.,-80.,-60.,-30.,0.,30.,60.,80.,90.})
                for(int alpha=-180;alpha<180;alpha+=30) cases.emplace(alpha,beta);
        } else {
            cases=segel::detail::uniform_transition_cases(model,speed,nu);
            if(full_matrix)
                for(double beta:{-90.,-80.,-60.,-30.,0.,30.,60.,80.,90.})
                    for(int alpha=-180;alpha<180;alpha+=30)cases.emplace(alpha,beta);
        }
        size_t all_cases=cases.size();
        int part=0,parts=1;
        if(transitions&&argc==8) {
            part=std::stoi(argv[6]);parts=std::stoi(argv[7]);
            if(parts<1||part<0||part>=parts)throw std::invalid_argument("Invalid case partition");
            size_t index=0;
            for(auto it=cases.begin();it!=cases.end();) {
                if(index++%static_cast<size_t>(parts)!=static_cast<size_t>(part)) it=cases.erase(it);
                else ++it;
            }
        }
        if(jsonl) {
            std::cout.exceptions(std::ios::badbit|std::ios::failbit);
            std::cout<<J({{"typ","lauf"},{"segel_id",model.at("segel_id")},
                          {"geometriepfad",argv[1]},{"auftragspfad",argv[2]},
                          {"modelleingabe",original_input},
                          {"geschwindigkeit_m_s",speed},{"dichte_kg_m3",rho},
                          {"viskositaet_m2_s",nu},{"rasterverfeinerung",refinement},
                          {"fallmenge",full_matrix?"matrix":(transitions?"uebergaenge":(single_case?"einzelfall":"grundfaelle"))},
                          {"teil",part},{"teile",parts},{"ausgewaehlte_faelle",cases.size()}}).dump()<<'\n';
        }
        double worst=0,worst_error=0,worst_limit=0,worst_a=0,worst_b=0;
        size_t completed=0,h1_cases=0;
        std::map<double,bool> validity{{speed,valid}};
        int worst_id=-1;std::string worst_kind;
        for(const auto& [alpha,beta]:cases) {
            current_alpha=alpha;current_beta=beta;
            if(transitions&&completed%256==0)
                std::cerr<<"Fortschritt speed="<<speed<<" cases="<<completed<<'/'<<cases.size()
                         <<" alpha="<<std::setprecision(17)<<alpha<<" beta="<<beta<<'\n';
            V wind=segel::detail::uniform_wind(speed,alpha,beta);
            double actual_speed=norm(wind),case_re=actual_speed*cref/nu;
            auto case_valid=validity.find(actual_speed);
            if(case_valid==validity.end())
                case_valid=validity.emplace(actual_speed,
                    segel::detail::uniform_h1_valid(actual_speed,nu,model,direct_grid)&&
                    split_reference.valid(case_re)&&refined_reference.valid(case_re)).first;
            J winds=J::array();
            for(const auto& strip:model.at("streifen"))
                winds.push_back({{"streifen_id",strip.at("streifen_id")},{"wind_m_s",wind}});
            phase="Streifenrechnung";
            J lifting=nullptr,result;
            if(jsonl) {
                auto detailed=solver.solve_with_lifting_diagnostics(winds.dump(),rho,nu);
                result=std::move(detailed.at("loads"));
                lifting=std::move(detailed.at("lifting"));
                lifting.erase("coefficients");
                lifting.erase("rhs");
            } else result=J::parse(solver.solve(winds.dump(),rho,nu));
            for(const auto& row:result) {
                V force=row.at("kraft_N"),moment=row.at("moment_Nm");
                if(!std::isfinite(norm(force))||!std::isfinite(norm(moment))||
                   (speed==0&&(norm(force)!=0||norm(moment)!=0)))
                    throw std::runtime_error("Invalid uniform-wind load");
            }
            if(model_transitions||model_poles) {
                std::cout<<J({{"typ","fall"},{"azimut_deg",alpha},{"neigung_deg",beta},
                              {"wind_m_s",wind},{"q_max_Pa",0.5*rho*actual_speed*actual_speed},
                              {"traglinie",lifting},{"status","modell_diagnose"},
                              {"modelllasten",std::move(result)}}).dump()<<'\n';
                ++completed;
                continue;
            }
            if(!case_valid->second) {
                if(jsonl) {
                    J record={{"typ","fall"},{"azimut_deg",alpha},{"neigung_deg",beta},
                              {"wind_m_s",wind},{"q_max_Pa",0.5*rho*actual_speed*actual_speed},
                              {"traglinie",lifting},
                              {"status","h1_nicht_anwendbar"},{"modelllasten_endlich",true}};
                    if(jsonl_loads)record["modelllasten"]=result;
                    std::cout<<record.dump()<<'\n';
                }
                ++completed;
                continue;
            }
            phase="Geteilte H1-Referenz";
            J reference=split_reference.solve_data(alpha,beta,case_re);
            phase="Direkte H1-Referenz";
            J direct_reference=J::parse(direct.solve(alpha,beta,case_re,4,false,48));
            phase="Verfeinerte H1-Referenz";
            J finer_reference=refined_reference.solve_data(alpha,beta,case_re);
            phase="H1-Vergleich";
            J diagnostic=segel::detail::compare_uniform_h1(
                model,result,reference,finer_reference,direct_reference,actual_speed,rho);
            ++h1_cases;
            double ratio=diagnostic.at("quotient");
            if(jsonl) {
                J record={{"typ","fall"},{"azimut_deg",alpha},{"neigung_deg",beta},
                          {"wind_m_s",wind},{"traglinie",lifting},
                          {"status",ratio<=1?"bestanden":"fehlgeschlagen"},
                          {"q_max_Pa",0.5*rho*actual_speed*actual_speed},{"c_ref_m",cref},
                          {"schlechtester_vergleich",diagnostic}};
                if(jsonl_loads)record["modelllasten"]=result;
                std::cout<<record.dump()<<'\n';
            }
            if(ratio>worst) {
                worst=ratio;worst_error=diagnostic.at("abweichung");worst_limit=diagnostic.at("grenze");
                worst_id=diagnostic.at("streifen_id").is_null()?-1:diagnostic.at("streifen_id").get<int>();
                worst_kind=diagnostic.at("groesse").get<std::string>();worst_a=alpha;worst_b=beta;
            }
            ++completed;
        }
        if(h1_cases==0) {
            std::ostream& summary=jsonl?std::cerr:std::cout;
            summary<<model.at("segel_id")<<" speed="<<speed
                     <<" model_cases="<<cases.size()
                     <<((model_transitions||model_poles)?" H1=nicht_geprueft\n":" H1=outside_valid_range\n");
            return 0;
        }
        std::ostream& summary=jsonl?std::cerr:std::cout;
        summary<<std::setprecision(17)<<model.at("segel_id")<<" speed="<<speed
                 <<" grid_refinement="<<refinement<<" part="<<part<<" parts="<<parts
                 <<" total_cases="<<all_cases<<" model_cases="<<cases.size()
                 <<" H1_cases="<<h1_cases<<" max_ratio="<<worst
                 <<" alpha="<<worst_a<<" beta="<<worst_b<<" strip="<<worst_id
                 <<" quantity="<<worst_kind<<" error="<<worst_error<<" limit="<<worst_limit<<'\n';
        return worst<=1?0:1;
    } catch(const std::exception& e){
        std::cerr<<"phase="<<phase<<" speed="<<argv[3]<<" alpha="<<current_alpha
                 <<" beta="<<current_beta<<": "<<e.what()<<'\n';
        return 2;
    }
}
