#include "job.hpp"
#include "../Core/src/segelform_internal.hpp"
#include "segel/modell.hpp"
#include <chrono>
#include <cmath>
#include <filesystem>
#include <future>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace fs=std::filesystem;
using J=cli::J;

int main(int argc,char** argv) {
    if(argc!=4&&(argc!=5||(std::string(argv[4])!="--performance"&&std::string(argv[4])!="--model-json"&&
                          std::string(argv[4])!="--parallel-check"&&std::string(argv[4])!="--published-check"))&&
       (argc!=6||std::string(argv[4])!="--workers"||
        (std::string(argv[5])!="1"&&std::string(argv[5])!="2"&&std::string(argv[5])!="4"))) {
        std::cerr<<"Usage: model_roundtrip_probe geometry.json job.json new-directory "
                   "[--performance|--model-json|--parallel-check|--published-check|--workers 1|2|4]\n";
        return 2;
    }
    try {
        std::cout.exceptions(std::ios::badbit|std::ios::failbit);
        const bool published_check=argc==5&&std::string(argv[4])=="--published-check";
        const bool performance=published_check||(argc==5&&std::string(argv[4])=="--performance");
        const bool model_json=argc==5&&std::string(argv[4])=="--model-json";
        const bool parallel_check=published_check||(argc==5&&std::string(argv[4])=="--parallel-check");
        const int workers=argc==6?std::stoi(argv[5]):1;
        const auto geometry_path=fs::u8path(argv[1]),job_path=fs::u8path(argv[2]),stage=fs::u8path(argv[3]);
        const auto geometry_snapshot=cli::read_hashed_json(geometry_path),job_snapshot=cli::read_hashed_json(job_path);
        const auto& geometry=geometry_snapshot.value;
        const auto& job=job_snapshot.value;
        const J source={{"geometrie",geometry},{"numerik",job.at("numerik")},
                        {"parameter",job.at("berechnung").at("parameter")}};
        const auto model=J::parse(segel::detail::prepare_model_data(source.dump()));
        if(!published_check) {
            if(!fs::create_directory(stage))
                throw std::runtime_error("Diagnostic directory already exists or cannot be created");
            auto metadata=cli::write_model_tables(stage,model,{},workers);
            J provenance={
                {"gueltigkeit",{{"geschwindigkeit_m_s",{0,50}},{"dichte_kg_m3",{1.0,1.4}},
                                 {"viskositaet_m2_s",{1.4e-5,1.6e-5}}}},
                {"eingaben",{{"auftrag",{{"pfad",job_path.u8string()},{"sha256",job_snapshot.sha256},
                                         {"schema_version",job.at("schema_version")}}},
                             {"geometrie",{{"pfad",geometry_path.u8string()},{"sha256",geometry_snapshot.sha256},
                                           {"schema_version",geometry.at("schema_version")}}}}},
                {"erzeuger",{{"werkzeug","diagnose"},{"arrow","diagnose"},
                              {"parquet","diagnose"},{"zstd","diagnose"}}},
                {"numerik",job.at("numerik")},
                {"nachweise",{{"ausgefuehrt",J::array()},{"bestanden",J::array()},
                               {"fehlgeschlagen",J::array()},{"nicht_ausgefuehrt",J::array({"numerikabnahme"})},
                               {"physikalisch_validiert",false}}}
            };
            cli::write_model_manifest(stage,model,metadata,provenance);
        }
        if(cli::read_model_manifest(stage).at("modell")!=model)
            throw std::runtime_error("Model roundtrip differs");
        const auto load_start=std::chrono::steady_clock::now();
        auto loaded=segel::modell::laden(stage);
        const auto load_end=std::chrono::steady_clock::now();
        auto points=loaded->punkte();
        if(points.size()!=model.at("streifen").size())
            throw std::runtime_error("Loaded point count differs");
        J input=J::array();
        std::vector<segel::modell::Wind> winds;
        for(const auto& point:points) {
            double speed=5.0+0.02*point.id;
            winds.push_back({point.id,{-speed,0.5,0.1}});
            input.push_back({{"streifen_id",point.id},{"wind_m_s",{-speed,0.5,0.1}}});
        }
        segel::detail::PreparedModel direct(model.dump());
        auto expected=J::parse(direct.solve(input.dump(),1.225,1.5e-5));
        auto actual=loaded->berechnen(winds,1.225,1.5e-5);
        if(actual.size()!=expected.size())throw std::runtime_error("Load count differs");
        for(size_t i=0;i<actual.size();++i) {
            if(actual[i].id!=expected[i].at("streifen_id").get<segel::modell::StreifenId>())
                throw std::runtime_error("Strip ID differs");
            auto force=expected[i].at("kraft_N").get<segel::modell::Vektor>();
            auto moment=expected[i].at("moment_Nm").get<segel::modell::Vektor>();
            for(int k=0;k<3;++k)
                if(!std::isfinite(actual[i].kraft_N[k])||!std::isfinite(actual[i].moment_Nm[k])||
                   actual[i].kraft_N[k]!=force[k]||actual[i].moment_Nm[k]!=moment[k])
                    throw std::runtime_error("Loaded model result differs");
        }
        if(parallel_check) {
            std::vector<std::vector<segel::modell::Wind>> inputs(4,winds);
            for(size_t task=0;task<inputs.size();++task)
                for(auto& wind:inputs[task])
                    wind.stroemung_m_s={-5.0-0.02*wind.id,0.2*task,0.1*task};
            std::vector<std::vector<segel::modell::Last>> expected_parallel;
            for(const auto& input:inputs)
                expected_parallel.push_back(loaded->berechnen(input,1.225,1.5e-5));
            std::promise<void> start;
            auto ready=start.get_future().share();
            std::vector<std::future<std::vector<segel::modell::Last>>> tasks;
            tasks.reserve(inputs.size());
            try {
                for(size_t i=0;i<inputs.size();++i)
                    tasks.push_back(std::async(std::launch::async,[&,i]{
                        ready.wait();
                        return loaded->berechnen(inputs[i],1.225,1.5e-5);
                    }));
            } catch(...) {
                start.set_value();
                throw;
            }
            start.set_value();
            for(size_t i=0;i<tasks.size();++i) {
                auto result=tasks[i].get();
                if(result.size()!=expected_parallel[i].size())
                    throw std::runtime_error("Parallel load count differs");
                for(size_t j=0;j<result.size();++j)
                    if(result[j].id!=expected_parallel[i][j].id||
                       result[j].kraft_N!=expected_parallel[i][j].kraft_N||
                       result[j].moment_Nm!=expected_parallel[i][j].moment_Nm)
                        throw std::runtime_error("Parallel model result differs from sequential result");
            }
        }
        std::uintmax_t bytes=0;
        double load_ms=0,case_ms=0;
        constexpr int repetitions=20;
        if(performance) {
            for(const auto& item:fs::directory_iterator(stage))
                if(item.is_regular_file())bytes+=item.file_size();
            const auto start=std::chrono::steady_clock::now();
            for(int i=0;i<repetitions;++i) {
                auto result=loaded->berechnen(winds,1.225,1.5e-5);
                if(result.size()!=points.size())
                    throw std::runtime_error("Measured load count differs");
            }
            const auto end=std::chrono::steady_clock::now();
            load_ms=std::chrono::duration<double,std::milli>(load_end-load_start).count();
            case_ms=std::chrono::duration<double,std::milli>(end-start).count()/repetitions;
        }
        if(model_json) {
            std::cout<<model.dump()<<'\n';
            return 0;
        }
        std::cout<<model.at("segel_id")<<" strips="<<points.size()<<" roundtrip=passed";
        if(parallel_check)std::cout<<" parallel_tasks=4 parallel_comparison=passed";
        if(performance) {
            std::cout<<std::setprecision(9)<<" bytes="<<bytes<<" load_ms="<<load_ms
                     <<" case_ms="<<case_ms<<" repetitions="<<repetitions<<" cache=warm";
        }
        std::cout<<'\n';
        return 0;
    } catch(const std::exception& e) {
        std::cerr<<"Model roundtrip failed: "<<e.what()<<'\n';
        return 1;
    }
}
