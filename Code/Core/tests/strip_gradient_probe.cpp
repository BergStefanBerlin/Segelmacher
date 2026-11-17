#include "../src/segelform_internal.hpp"
#include "../src/segelform_test_winds.hpp"
#include "../src/segelform_load_compare.hpp"
#include "../src/segelform_edge_check.hpp"
#include "../src/segelform_re_check.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

using J=nlohmann::json;
using V=std::array<double,3>;
namespace {
J read(const char* path) {
    std::ifstream file(path);
    if(!file) throw std::runtime_error(std::string("Cannot open ")+path);
    return J::parse(file);
}
V cross(V a,V b) {return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};}
double norm(V a) {return std::hypot(std::hypot(a[0],a[1]),a[2]);}
V point(const J& strip) {return {strip["abfrage_x_m"],strip["abfrage_y_m"],strip["abfrage_z_m"]};}
J winds(const J& model,int type,bool reverse,double& vmax) {
    J out=J::array();double zmin=model["z_min_m"],height=model["hoehe_m"];
    for(const auto& row:model["streifen"]) {
        double t=(row["abfrage_z_m"].get<double>()-zmin)/height;
        if(reverse)t=1-t;
        double velocity=type==1?10:1+9*t;
        double angle=type==0?0:(-45+90*t)*3.141592653589793/180;
        V v{-velocity*std::cos(angle),-velocity*std::sin(angle),0};
        vmax=std::max(vmax,norm(v));
        out.push_back({{"streifen_id",row["streifen_id"]},{"wind_m_s",v}});
    }
    return out;
}
J sample_coarse_field(const J& coarse,const J& coarse_winds,const J& fine) {
    J out=J::array();
    const auto& strips=coarse.at("streifen");
    for(const auto& row:fine.at("streifen")) {
        double z=row.at("abfrage_z_m");
        size_t hi=0;
        while(hi<strips.size()&&strips[hi].at("abfrage_z_m").get<double>()<z) ++hi;
        V v{};
        if(hi==0) v=coarse_winds[0].at("wind_m_s").get<V>();
        else if(hi==strips.size()) v=coarse_winds.back().at("wind_m_s").get<V>();
        else {
            double a=strips[hi-1].at("abfrage_z_m"),b=strips[hi].at("abfrage_z_m");
            double t=(z-a)/(b-a);
            V left=coarse_winds[hi-1].at("wind_m_s"),right=coarse_winds[hi].at("wind_m_s");
            for(int k=0;k<3;++k)v[k]=(1-t)*left[k]+t*right[k];
        }
        out.push_back({{"streifen_id",row.at("streifen_id")},{"wind_m_s",v}});
    }
    return out;
}
int continuity_diagnostic(const J& model,const segel::detail::PreparedModel& solver) {
    J results=segel::detail::check_wind_continuity(model,solver,[]{});
    const J* worst=&results.front();
    for(const auto& result:results)
        if(result["schlechtester_vergleich"]["quotient"].get<double>()>
           (*worst)["schlechtester_vergleich"]["quotient"].get<double>())worst=&result;
    const auto& comparison=worst->at("schlechtester_vergleich");
    std::cout<<model.at("segel_id")<<" continuity_case="<<worst->at("windfall")
             <<" strip="<<(comparison["streifen_id"].is_null()?-1:comparison["streifen_id"].get<int>())
             <<" quantity="<<comparison.at("groesse")<<" error="<<comparison.at("abweichung")
             <<" diagnostic_0.1_percent="<<comparison.at("grenze")<<" ratio="<<comparison.at("quotient")<<'\n';
    return comparison.at("quotient").get<double>()<=1?0:1;
}
int low_re_diagnostic(const J& model,const segel::detail::PreparedModel& solver) {
    J result=segel::detail::check_re_boundaries(model,solver,[]{});
    std::cout<<model.at("segel_id")<<" re_boundary_diagnostic="<<result.dump()<<'\n';
    return result.at("gepruefte_punkte").get<size_t>()>0&&
           result.at("schlechtester_vergleich").at("vergleich").at("quotient").get<double>()<=1?0:1;
}
int balance_diagnostic(const J& coarse,const J& fine,
                       const segel::detail::PreparedModel& coarse_solver,
                       const segel::detail::PreparedModel& fine_solver) {
    double worst=0;
    for(int type=0;type<4;++type)for(bool reverse:{false,true}) {
        double vmax=0;
        auto coarse_winds=type==3?segel::detail::local_zero_winds(coarse,coarse,reverse,vmax):
                                    winds(coarse,type,reverse,vmax);
        auto fine_winds=type==3?segel::detail::local_zero_winds(coarse,fine,reverse,vmax):
                                  winds(fine,type,reverse,vmax);
        double q=0.5*1.225*vmax*vmax;
        for(bool refined:{false,true}) {
            const auto& model=refined?fine:coarse;
            const auto& solver=refined?fine_solver:coarse_solver;
            const auto& wind=refined?fine_winds:coarse_winds;
            auto detail=J::parse(solver.solve_with_contributions(wind.dump(),1.225,1.5e-5));
            auto balance=segel::detail::compare_contribution_balance(model,detail,q);
            worst=std::max(worst,balance["schlechtester_vergleich"]["quotient"].get<double>());
        }
    }
    std::cout<<coarse.at("segel_id")<<" contribution_balance_cases=16 max_diagnostic_ratio="<<worst<<'\n';
    return worst<=1?0:1;
}
int zero_sequence_diagnostic(const J& model,const segel::detail::PreparedModel& solver) {
    J results=segel::detail::check_zero_sequence(model,solver,[]{});
    const auto& near=results[results.size()-2];
    const auto& zero=results.back();
    std::cout<<model.at("segel_id")<<" zero_sequence=passed force_at_1e-12_N="<<near.at("gesamtkraft_N")
             <<" moment_at_1e-12_Nm="<<near.at("gesamtmoment_Nm")<<" zero_force_N="<<zero.at("gesamtkraft_N")
             <<" zero_moment_Nm="<<zero.at("gesamtmoment_Nm")<<'\n';
    return 0;
}
}
int main(int argc,char** argv) {
    if(argc<3||argc>5||(argc==5&&std::string(argv[4])!="--sample-coarse"&&
                                      std::string(argv[4])!="--modes"&&
                                      std::string(argv[4])!="--details"&&
                                      std::string(argv[4])!="--continuity"&&
                                      std::string(argv[4])!="--re-boundaries"&&
                                      std::string(argv[4])!="--contributions"&&
                                      std::string(argv[4])!="--half-width"&&
                                      std::string(argv[4])!="--zero-sequence"&&
                                      std::string(argv[4])!="--edge-fields"&&
                                      std::string(argv[4])!="--raster-json"&&
                                      std::string(argv[4])!="--contributions-json"&&
                                      std::string(argv[4])!="--residual-json"&&
                                      std::string(argv[4])!="--local-zero")) {
        std::cerr<<"Usage: strip_gradient_probe geometry.json job.json [quadrature-refinement [--sample-coarse|--modes|--details|--continuity|--re-boundaries|--contributions|--half-width|--zero-sequence|--local-zero|--edge-fields|--raster-json|--contributions-json|--residual-json]]\n";
        return 2;
    }
    try {
        J job=read(argv[2]),input={{"geometrie",read(argv[1])},{"numerik",job["numerik"]},
                                   {"parameter",job["berechnung"]["parameter"]}};
        if(argc>=4) {
            double factor=std::stod(argv[3]);
            if(!std::isfinite(factor)||factor<1) throw std::invalid_argument("Invalid refinement factor");
            auto& grid=input["numerik"]["hoehenraster"];
            grid["schritt_m"]=grid["schritt_m"].get<double>()/factor;
            grid["verfeinerter_schritt_m"]=grid["verfeinerter_schritt_m"].get<double>()/factor;
        }
        std::string source=input.dump();
        int subdivision=argc==5&&std::string(argv[4])=="--half-width"?2:1;
        J coarse=J::parse(segel::detail::prepare_model_data(source,subdivision));
        J fine=J::parse(segel::detail::prepare_model_data(source,2*subdivision));
        segel::detail::PreparedModel coarse_solver(coarse.dump()),fine_solver(fine.dump());
        if(argc==5&&std::string(argv[4])=="--continuity")
            return continuity_diagnostic(coarse,coarse_solver);
        if(argc==5&&std::string(argv[4])=="--re-boundaries")
            return low_re_diagnostic(coarse,coarse_solver);
        if(argc==5&&std::string(argv[4])=="--contributions")
            return balance_diagnostic(coarse,fine,coarse_solver,fine_solver);
        if(argc==5&&std::string(argv[4])=="--zero-sequence")
            return zero_sequence_diagnostic(coarse,coarse_solver);
        double worst=0,error=0,limit=0;
        struct ModeResult {
            double worst=0,error=0,limit=0;
            int type=0,strip=0;
            bool reverse=false,fine=false,force=false;
        };
        std::array<ModeResult,2> mode_results{};
        int worst_strip=0,worst_type=0;bool worst_reverse=false,worst_is_force=false;
        const char* types[]={"speed","direction","combined","local_zero","counterflow","azimuth_seam","local_vertical"};
        const std::array<segel::detail::EdgeWindCase,3> edge_types={
            segel::detail::EdgeWindCase::counterflow,segel::detail::EdgeWindCase::azimuth_seam,
            segel::detail::EdgeWindCase::local_vertical};
        bool only_zero=argc==5&&std::string(argv[4])=="--local-zero";
        bool only_edges=argc==5&&std::string(argv[4])=="--edge-fields";
        bool raster_json=argc==5&&std::string(argv[4])=="--raster-json";
        bool contributions_json=argc==5&&std::string(argv[4])=="--contributions-json";
        bool residual_json=argc==5&&std::string(argv[4])=="--residual-json";
        J raw_cases=J::array();
        for(int type=0;type<7;++type)for(bool reverse:{false,true}) {
            if(!raster_json&&!contributions_json&&!residual_json&&
               (only_edges ? type<4 : (type>=4||(type==3)!=only_zero))) continue;
            double vmax=0;
            J wc=type>=4?segel::detail::edge_winds(coarse,coarse,edge_types[type-4],reverse,vmax):
                (type==3?segel::detail::local_zero_winds(coarse,coarse,reverse,vmax):winds(coarse,type,reverse,vmax));
            J wf=type>=4?segel::detail::edge_winds(coarse,fine,edge_types[type-4],reverse,vmax):
                (type==3?segel::detail::local_zero_winds(coarse,fine,reverse,vmax):winds(fine,type,reverse,vmax));
            if(argc==5&&std::string(argv[4])=="--sample-coarse") wf=sample_coarse_field(coarse,wc,fine);
            if(residual_json) {
                J diagnostics=J::array();
                for(bool refined:{false,true}) {
                    const auto& solver=refined?fine_solver:coarse_solver;
                    const auto& wind=refined?wf:wc;
                    for(int modes:{32,48,64}) {
                        auto result=solver.solve_with_lifting_diagnostics(wind.dump(),1.225,1.5e-5,modes);
                        diagnostics.push_back({{"halbiertes_raster",refined},{"moden",modes},
                                               {"lifting",std::move(result.at("lifting"))}});
                    }
                }
                raw_cases.push_back({{"windfeld",types[type]},{"umgekehrt",reverse},
                                     {"wind_grob",wc},{"wind_halbiert",wf},
                                     {"diagnosen",std::move(diagnostics)}});
                continue;
            }
            J c,f;
            if(contributions_json) {
                auto coarse_detail=J::parse(coarse_solver.solve_with_contributions(wc.dump(),1.225,1.5e-5));
                auto fine_detail=J::parse(fine_solver.solve_with_contributions(wf.dump(),1.225,1.5e-5));
                c=coarse_detail.at("streifen");
                f=fine_detail.at("streifen");
                raw_cases.push_back({{"windfeld",types[type]},{"umgekehrt",reverse},
                                     {"wind_grob",wc},{"wind_halbiert",wf},
                                     {"details_grob",std::move(coarse_detail)},
                                     {"details_halbiert",std::move(fine_detail)}});
            } else {
                c=J::parse(coarse_solver.solve(wc.dump(),1.225,1.5e-5));
                f=J::parse(fine_solver.solve(wf.dump(),1.225,1.5e-5));
            }
            if(raster_json)raw_cases.push_back({{"windfeld",types[type]},{"umgekehrt",reverse},
                                                {"wind_grob",wc},{"wind_halbiert",wf},
                                                {"last_grob",c},{"last_halbiert",f}});
            double q=0.5*1.225*vmax*vmax,cref=coarse["c_ref_m"];
            V coarse_force{},fine_force{},coarse_moment{},fine_moment{};
            double case_ratio=0,case_error=0,case_limit=0;
            int case_strip=-1;bool case_force=false;
            auto observe=[&](V df,V dm,double area,int id) {
                double flimit=0.05*q*area+1e-9,mlimit=0.05*q*area*cref+1e-9;
                double fratio=norm(df)/flimit,mratio=norm(dm)/mlimit;
                double ratio=std::max(fratio,mratio);
                bool force=fratio>=mratio;
                if(ratio>case_ratio) {
                    case_ratio=ratio;case_strip=id;case_force=force;
                    case_error=force?norm(df):norm(dm);case_limit=force?flimit:mlimit;
                }
                if(ratio>worst) {
                    worst=ratio;worst_type=type;worst_reverse=reverse;worst_strip=id;
                    worst_is_force=force;error=force?norm(df):norm(dm);
                    limit=force?flimit:mlimit;
                }
            };
            if(argc==5&&std::string(argv[4])=="--modes") {
                for(bool refined:{false,true}) {
                    const auto& geometry=refined?fine:coarse;
                    const auto& reference=refined?f:c;
                    const auto& winds=refined?wf:wc;
                    const auto& solver=refined?fine_solver:coarse_solver;
                    for(int modes:{32,64}) {
                        auto& stats=mode_results[modes==32?0:1];
                        J candidate=J::parse(solver.solve_modes(winds.dump(),1.225,1.5e-5,modes));
                        J comparison=segel::detail::compare_same_grid_loads(geometry,reference,candidate,q);
                        double ratio=comparison.at("quotient");
                        if(ratio>stats.worst) {
                            stats.worst=ratio;stats.type=type;stats.reverse=reverse;
                            stats.fine=refined;stats.strip=comparison["streifen_id"].is_null()
                                ?-1:comparison["streifen_id"].get<int>();
                            stats.force=comparison["groesse"]=="kraft_N";
                            stats.error=comparison["abweichung"];stats.limit=comparison["grenze"];
                        }
                    }
                }
            }
            for(size_t id=0;id<c.size();++id) {
                V cf=c[id]["kraft_N"],cm=c[id]["moment_Nm"],cp=point(coarse["streifen"][id]);
                V ff{},fm{};
                for(size_t j=2*id;j<2*id+2;++j) {
                    V x=f[j]["kraft_N"],m=f[j]["moment_Nm"],p=point(fine["streifen"][j]);
                    V r{p[0]-cp[0],p[1]-cp[1],p[2]-cp[2]},arm=cross(r,x);
                    for(int k=0;k<3;++k) {ff[k]+=x[k];fm[k]+=m[k]+arm[k];}
                }
                V df{ff[0]-cf[0],ff[1]-cf[1],ff[2]-cf[2]};
                V dm{fm[0]-cm[0],fm[1]-cm[1],fm[2]-cm[2]};
                V ca=cross(cp,cf),fa=cross(cp,ff);
                for(int k=0;k<3;++k) {
                    coarse_force[k]+=cf[k];fine_force[k]+=ff[k];
                    coarse_moment[k]+=cm[k]+ca[k];fine_moment[k]+=fm[k]+fa[k];
                }
                double area=coarse["streifen"][id]["flaeche_m2"];
                observe(df,dm,area,static_cast<int>(id));
            }
            V df{},dm{};
            for(int k=0;k<3;++k) {
                df[k]=fine_force[k]-coarse_force[k];
                dm[k]=fine_moment[k]-coarse_moment[k];
            }
            double area=coarse["S_m2"];
            observe(df,dm,area,-1);
            if(argc==5&&(std::string(argv[4])=="--details"||only_edges))
                std::cout<<"gradient_case="<<types[type]<<" reversed="<<reverse<<" strip="<<case_strip
                         <<" quantity="<<(case_force?"force_N":"moment_Nm")<<" error="<<case_error
                         <<" limit="<<case_limit<<" tolerance_ratio="<<case_ratio<<'\n';
        }
        if(residual_json) {
            std::cout<<J({{"kopplung_grob",coarse.at("kopplung")},{"kopplung_halbiert",fine.at("kopplung")},
                          {"dichte_kg_m3",1.225},{"viskositaet_m2_s",1.5e-5},{"faelle",raw_cases}}).dump()<<'\n';
            return 0;
        }
        if(raster_json||contributions_json) {
            std::cout<<J({{"streifen_grob",coarse.at("streifen")},{"streifen_halbiert",fine.at("streifen")},
                          {"faelle",raw_cases}}).dump()<<'\n';
            return worst<=1?0:1;
        }
        std::cout<<coarse["segel_id"]<<" gradient="<<types[worst_type]<<" reversed="<<worst_reverse
                 <<" strip="<<worst_strip<<" error="<<error<<" limit="<<limit
                 <<" quantity="<<(worst_is_force?"force_N":"moment_Nm")<<" tolerance_ratio="<<worst
                 <<" output_subdivision="<<subdivision
                 <<" sample_coarse="<<(argc==5&&std::string(argv[4])=="--sample-coarse")<<'\n';
        if(argc==5&&std::string(argv[4])=="--modes")
            for(size_t i=0;i<mode_results.size();++i) {
                const auto& stats=mode_results[i];
                std::cout<<"mode_diagnostic gradient="<<types[stats.type]<<" reversed="<<stats.reverse
                         <<" fine="<<stats.fine<<" strip="<<stats.strip<<" modes="<<(i==0?32:64)
                         <<" quantity="<<(stats.force?"force_N":"moment_Nm")<<" error="<<stats.error
                         <<" reference_0.1_percent="<<stats.limit<<" ratio="<<stats.worst<<'\n';
            }
        return worst<=1?0:1;
    } catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}
}
