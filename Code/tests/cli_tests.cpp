#include "job.hpp"
#include "progress.hpp"
#include "segel/modell.hpp"
#include "../Core/src/segelform_internal.hpp"
#include "../Core/src/segelform_uniform_check.hpp"
#include "../Core/src/segelform_load_compare.hpp"
#include <arrow/api.h>
#include <arrow/config.h>
#include <arrow/io/api.h>
#include <parquet/arrow/reader.h>
#include <parquet/file_reader.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <fstream>
#include <future>
#include <iostream>
#include <iterator>
#include <limits>
#include <mutex>
#include <set>
#include <streambuf>
#include <thread>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif
namespace {
class PhaseWatcher : public std::streambuf {
public:
    explicit PhaseWatcher(std::streambuf* destination):destination_(destination) {}
    bool wait_for_uniform() {
        std::unique_lock<std::mutex> lock(mutex_);
        return condition_.wait_for(lock,std::chrono::minutes(5),[&]{return uniform_started_;});
    }
protected:
    std::streamsize xsputn(const char* data,std::streamsize count) override {
        inspect(data,static_cast<size_t>(count));
        return destination_->sputn(data,count);
    }
    int_type overflow(int_type value) override {
        if(traits_type::eq_int_type(value,traits_type::eof()))return traits_type::not_eof(value);
        const char c=traits_type::to_char_type(value);
        inspect(&c,1);
        return destination_->sputc(c);
    }
    int sync() override { return destination_->pubsync(); }
private:
    void inspect(const char* data,size_t count) {
        std::lock_guard<std::mutex> lock(mutex_);
        tail_.append(data,count);
        if(tail_.find("INFO [Gleichwindpruefung]")!=std::string::npos) {
            uniform_started_=true;
            condition_.notify_one();
        }
        if(tail_.size()>256)tail_.erase(0,tail_.size()-128);
    }
    std::streambuf* destination_;
    std::mutex mutex_;
    std::condition_variable condition_;
    std::string tail_;
    bool uniform_started_=false;
};
void check_model_roundtrip(const std::filesystem::path& path,const cli::J& rows,const cli::J& metadata) {
    auto file=arrow::io::ReadableFile::Open(path.u8string());
    if(!file.ok())throw std::runtime_error(file.status().ToString());
    auto maybe_reader=parquet::arrow::OpenFile(*file,arrow::default_memory_pool());
    if(!maybe_reader.ok())throw std::runtime_error(maybe_reader.status().ToString());
    auto reader=std::move(maybe_reader).ValueOrDie();
    std::shared_ptr<arrow::Table> table;
    auto status=reader->ReadTable(&table);
    if(!status.ok())throw std::runtime_error(status.ToString());
    const auto& schema=metadata.at("schema");
    if(table->num_rows()!=static_cast<int64_t>(rows.size())||
       table->num_columns()!=static_cast<int>(schema.size()))
        throw std::runtime_error("Segelmodell-Parquet-Dimensionen ungueltig: "+path.u8string());
    auto file_metadata=reader->parquet_reader()->metadata();
    for(int group=0;group<file_metadata->num_row_groups();++group)
        for(int column=0;column<table->num_columns();++column)
            if(file_metadata->RowGroup(group)->ColumnChunk(column)->compression()!=parquet::Compression::ZSTD)
                throw std::runtime_error("Segelmodell-Parquet-Codec ist nicht ZSTD");
    for(size_t c=0;c<schema.size();++c) {
        auto name=schema[c].at("name").get<std::string>();
        bool integer=name=="streifen_id"||name=="punkt_id"||name=="teil_id"||name=="moden"||
                     name=="index"||name=="zeile"||name=="spalte";
        std::string unit="1";
        if(name.size()>=3&&name.substr(name.size()-3)=="_m2")unit="m^2";
        else if(name.size()>=2&&name.substr(name.size()-2)=="_m")unit="m";
        else if(name=="twist_rad"||name=="a0"||name=="a1"||name=="a2")unit="rad";
        if(schema[c].at("typ")!=(integer?"i32":"f64")||schema[c].at("einheit")!=unit)
            throw std::runtime_error("Segelmodell-Metadaten fuer Spalte ungueltig: "+name);
        if(table->schema()->field(static_cast<int>(c))->name()!=name||
           table->schema()->field(static_cast<int>(c))->nullable())
            throw std::runtime_error("Segelmodell-Parquet-Spaltenname ungueltig: "+name);
        for(size_t r=0;r<rows.size();++r) {
            auto scalar=table->column(static_cast<int>(c))->GetScalar(static_cast<int64_t>(r));
            if(!scalar.ok())throw std::runtime_error(scalar.status().ToString());
            const auto& value=**scalar;
            if(!value.is_valid)throw std::runtime_error("Nullwert in Segelmodell-Parquet: "+name);
            if(schema[c].at("typ")=="i32") {
                if(value.type->id()!=arrow::Type::INT32||
                   static_cast<const arrow::Int32Scalar&>(value).value!=rows[r].at(name).get<int32_t>())
                    throw std::runtime_error("Segelmodell-Parquet-Index stimmt nicht: "+name);
            } else {
                if(value.type->id()!=arrow::Type::DOUBLE)throw std::runtime_error("Parquet-Typ ungueltig: "+name);
                double actual=static_cast<const arrow::DoubleScalar&>(value).value;
                double expected=rows[r].at(name).get<double>();
                if(std::memcmp(&actual,&expected,sizeof(double))!=0)
                    throw std::runtime_error("Segelmodell-Parquet-Wert stimmt nicht: "+name);
            }
        }
    }
}
}
int main(int argc,char** argv) {
    using cli::J;
    try {
        if(argc==3&&std::string(argv[1])=="--test-load-model") {
            try {
                segel::modell::laden(std::filesystem::u8path(argv[2]));
                return 0;
            } catch(const segel::modell::Fehler& e) {
                std::cerr<<e.what()<<'\n';
                if(e.art()==segel::modell::Fehlerart::modell)return 2;
                if(e.art()==segel::modell::Fehlerart::integritaet)return 3;
                return 1;
            }
        }
        if(argc==5&&std::string(argv[1])=="--test-publish-fingerprint") {
            const auto stage=std::filesystem::u8path(argv[2]);
            const auto output=std::filesystem::u8path(argv[3]);
            const auto model_root=std::filesystem::u8path(argv[4]);
            auto result=cli::publish_model_result(stage,output,true,[&](J& published) {
                cli::update_model_fingerprint(published,model_root);
            });
            std::cout<<result.dump()<<'\n';
            return result.at("exitcode").get<int>();
        }
        if(argc!=1)throw std::invalid_argument("Ungueltige CLI-Testargumente");
        int checks=0;
        auto check=[&](bool ok){++checks;if(!ok)throw std::runtime_error("CLI contract assertion "+std::to_string(checks));};
        check(cli::progress_line("Gesamt",5,6,"\x1b[94m","Export").find("5 / 6  | Export")!=std::string::npos);
        check(cli::progress_line("Paket ",999,49680,"\x1b[92m").find("999 / 49680")!=std::string::npos);
        check(cli::progress_line("Paket ",0,0,"\x1b[92m").find("0 / ?")!=std::string::npos);
        check(cli::progress_line("Paket ",8,6,"\x1b[92m").find("6 / 6")!=std::string::npos);
        check(cli::progress_time(233,233,5656)=="(00:03:53 / 01:34:16) | Verbleibende Zeit: 01:30:23");
        check(cli::progress_time(10,0,100)=="(00:00:10 / wird ermittelt) | Verbleibende Zeit: wird ermittelt");
        check(cli::progress_time(10,100,100)=="(00:00:10 / 00:00:10) | Verbleibende Zeit: 00:00:00");
        check(cli::progress_time(20,20,100)=="(00:00:20 / 00:01:40) | Verbleibende Zeit: 00:01:20");
        check(cli::progress_duration(59)=="00:00:59");
        check(cli::progress_duration(60)=="00:01:00");
        check(cli::progress_duration(3599)=="00:59:59");
        check(cli::progress_duration(3600)=="01:00:00");
        check(cli::progress_duration(5)=="00:00:05");
        check(cli::dispatch({"--hilfe"})["exitcode"]==0);
        check(cli::dispatch({"--version"})["exitcode"]==0);
        check(cli::dispatch({})["exitcode"]==2);
        check(cli::dispatch({"lookup","--auftrag","missing.json"})["exitcode"]==2);
        check(cli::dispatch({"lookup","--auftrag","a","--max-prozessorzahl","0"})["exitcode"]==2);
        check(cli::dispatch({"lookup","--auftrag","a","--max-prozessorzahl","2","--max-prozessorzahl","2"})["exitcode"]==2);
        check(cli::dispatch({"pruefen","--auftrag","a","--unknown","1"})["exitcode"]==2);
        check(cli::dispatch({"lookup","--auftrag","a","--fingerprint","invalid"})["exitcode"]==2);
        check(cli::dispatch({"lookup","--auftrag","a","--output-policy","invalid"})["exitcode"]==2);
        check(cli::dispatch({"lookup","--auftrag","a","--fingerprint","skip","--fingerprint","update"})["exitcode"]==2);
        check(cli::dispatch({"lookup","--auftrag","a","--output-policy","overwrite","--output-policy","create-only"})["exitcode"]==2);
        check(cli::dispatch({"pruefen","--auftrag","a","--fingerprint","update"})["exitcode"]==2);
        std::filesystem::path p="cli_duplicate_test.json";
        {std::ofstream out(p);out<<"{\"a\":1,\"a\":2}";}
        bool rejected=false;try{cli::read(p);}catch(const cli::Error&){rejected=true;}
        bool snapshot_rejected=false;try{cli::read_hashed_json(p);}catch(const cli::Error&){snapshot_rejected=true;}
        std::filesystem::remove(p);check(rejected&&snapshot_rejected);
        p="cli_hash_test.txt";{std::ofstream out(p,std::ios::binary);out<<"abc";}
        check(cli::hash(p)=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
        std::filesystem::remove(p);
        p="cli_snapshot_test.json";
        {std::ofstream out(p,std::ios::binary);out<<"{\"n\":1}";}
        auto expected_snapshot_hash=cli::hash(p);
        auto snapshot=cli::read_hashed_json(p);
        const auto deferred_snapshot=cli::read_json_snapshot(p);
        {std::ofstream out(p,std::ios::binary|std::ios::trunc);out<<"{\"n\":2}";}
        check(snapshot.value.at("n")==1&&snapshot.sha256==expected_snapshot_hash&&
              snapshot.sha256!=cli::hash(p)&&deferred_snapshot.value.at("n")==1&&
              cli::hash_json_snapshot(deferred_snapshot,p)==expected_snapshot_hash);
        std::filesystem::remove(p);
        const std::filesystem::path source=TEST_SOURCE_ROOT;
        const auto token=std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        const auto work=std::filesystem::current_path()/("cli_contract_"+token);
        std::filesystem::create_directory(work);
        {
            const auto path=work/"snapshot_limit.json";
            std::string bytes="{\"n\":1}";
            bytes.resize(16*1024*1024,' ');
            {std::ofstream out(path,std::ios::binary);out.write(bytes.data(),static_cast<std::streamsize>(bytes.size()));}
            auto limit_snapshot=cli::read_hashed_json(path);
            check(limit_snapshot.value.at("n")==1&&limit_snapshot.sha256==cli::hash(path));
            {std::ofstream out(path,std::ios::binary|std::ios::app);out.put(' ');}
            bool too_large=false;
            try {cli::read_hashed_json(path);}
            catch(const cli::Error& e) {too_large=e.code==3;}
            check(too_large);
            check(std::filesystem::remove(path));
        }
        const auto reference=source/"Code"/"tests"/"daten"/"main_20261105_195154_358.json";
        check(cli::hash(reference)=="e8c47ffca62459c08ae525320794b19884a72544a2857c77606473ccea862eb2");
        std::filesystem::copy_file(reference,work/"main.json");
        std::filesystem::copy_file(source/"Dokumentationen"/"aerodynamik_konventionen.json",work/"konventionen.json");
        J job=cli::read(source/"Code"/"ressourcen"/"examples"/"Auftragsdatei.json");
        job["ressourcen"]["max_prozessorzahl"]=1;job["ressourcen"]["gpu"]="aus";
        J input={{"geometrie",cli::read(reference)},{"numerik",job["numerik"]},{"parameter",job["berechnung"]["parameter"]}};
        J model=J::parse(segel::detail::prepare_model_data(input.dump()));
        auto first_stage=work/"segelform_stage1",second_stage=work/"segelform_stage2";
        std::filesystem::create_directory(first_stage);std::filesystem::create_directory(second_stage);
        auto damaged=model;
        damaged["quadratur"][0]["dz_m"]=0.001;
        bool invalid_rejected=false;
        try { cli::write_model_tables(second_stage,damaged); }
        catch(const std::exception&) { invalid_rejected=true; }
        check(invalid_rejected&&std::filesystem::is_empty(second_stage));
        auto cancelled_stage=work/"segelform_cancelled";
        check(std::filesystem::create_directory(cancelled_stage));
        bool cancelled_rejected=false;
        try {
            cli::write_model_tables(cancelled_stage,model,[]{throw cli::Error(130,"Testabbruch");});
        } catch(const cli::Error& e) { cancelled_rejected=e.code==130; }
        check(cancelled_rejected&&std::filesystem::is_empty(cancelled_stage));
        cancelled_rejected=false;
        try {
            cli::write_model_tables(cancelled_stage,model,[&]{
                if(std::filesystem::exists(cancelled_stage/"streifen.parquet"))
                    throw cli::Error(130,"Testabbruch");
            });
        } catch(const cli::Error& e) { cancelled_rejected=e.code==130; }
        check(cancelled_rejected&&
              std::filesystem::exists(cancelled_stage/"streifen.parquet")&&
              !std::filesystem::exists(cancelled_stage/"manifest.json")&&
              !std::filesystem::exists(cancelled_stage/"quadratur.parquet"));
        check(std::filesystem::remove_all(cancelled_stage)>0);
        J metadata=cli::write_model_tables(first_stage,model);
        bool occupied_rejected=false;
        try { cli::write_model_tables(first_stage,model); }
        catch(const cli::Error&) { occupied_rejected=true; }
        check(occupied_rejected);
        J second_metadata=cli::write_model_tables(second_stage,model);
        for(int workers:{2,4}) {
            auto parallel_stage=work/("segelform_parallel_"+std::to_string(workers));
            check(std::filesystem::create_directory(parallel_stage));
            const auto main_thread=std::this_thread::get_id();
            std::mutex mutex;
            std::condition_variable ready;
            std::set<std::thread::id> thread_ids;
            auto parallel_metadata=cli::write_model_tables(parallel_stage,model,[&]{
                const auto id=std::this_thread::get_id();
                if(id==main_thread)return;
                std::unique_lock<std::mutex> lock(mutex);
                thread_ids.insert(id);
                ready.notify_all();
                if(!ready.wait_for(lock,std::chrono::seconds(30),[&]{
                    return thread_ids.size()>=static_cast<size_t>(workers);
                }))throw std::runtime_error("Tabellen-Worker nicht gestartet");
            },workers);
            check(thread_ids.size()==static_cast<size_t>(workers)&&parallel_metadata==metadata);
            for(const auto& item:metadata.items()) {
                std::ifstream serial(first_stage/item.key(),std::ios::binary);
                std::ifstream parallel(parallel_stage/item.key(),std::ios::binary);
                check(serial.good()&&parallel.good()&&
                      std::equal(std::istreambuf_iterator<char>(serial),std::istreambuf_iterator<char>(),
                                 std::istreambuf_iterator<char>(parallel),std::istreambuf_iterator<char>())&&
                      !serial.bad()&&!parallel.bad());
            }
            check(std::filesystem::remove_all(parallel_stage)>0);
        }
        J provenance={
            {"gueltigkeit",{{"geschwindigkeit_m_s",{0,50}},{"dichte_kg_m3",{1.0,1.4}},
                             {"viskositaet_m2_s",{1.4e-5,1.6e-5}}}},
            {"eingaben",{{"auftrag",{{"pfad","Auftragsdatei.json"},{"sha256",cli::hash(source/"Code"/"ressourcen"/"examples"/"Auftragsdatei.json")},{"schema_version","0.1"}}},
                         {"geometrie",{{"pfad","main.json"},{"sha256",cli::hash(reference)},{"schema_version","0.1"}}},
                         {"konventionen",{{"pfad","konventionen.json"},{"sha256",cli::hash(work/"konventionen.json")},{"schema_version","0.1"}}}}},
            {"erzeuger",{{"werkzeug","test"},{"arrow","test"},{"parquet","test"},{"zstd","test"}}},
            {"numerik",job["numerik"]},
            {"nachweise",{{"ausgefuehrt",J::array()},{"bestanden",J::array()},{"fehlgeschlagen",J::array()},
                           {"nicht_ausgefuehrt",J::array({"numerikabnahme"})},{"physikalisch_validiert",false}}}
        };
        auto unsupported_range=provenance;
        unsupported_range["gueltigkeit"]["viskositaet_m2_s"]={1e-12,1e-5};
        bool range_rejected=false;
        try { cli::write_model_manifest(second_stage,model,second_metadata,unsupported_range); }
        catch(const std::exception&) { range_rejected=true; }
        check(range_rejected&&!std::filesystem::exists(second_stage/"manifest.json"));
        auto contradictory=provenance;
        contradictory["nachweise"]["bestanden"]=J::array({"numerikabnahme"});
        bool evidence_rejected=false;
        try { cli::write_model_manifest(second_stage,model,second_metadata,contradictory); }
        catch(const std::exception&) { evidence_rejected=true; }
        check(evidence_rejected&&!std::filesystem::exists(second_stage/"manifest.json"));
        {
            namespace fs=std::filesystem;
            const auto parent=work/"stage_construction";
            check(fs::create_directory(parent));
            const auto output=parent/"Segelmodell";
            check(fs::create_directory(output));
            {std::ofstream out(output/"keep.txt");out<<"old";}
            auto without_conventions=provenance;
            without_conventions["eingaben"].erase("konventionen");
            auto no_conventions=parent/"Segelmodell.arbeiten.no_conventions";
            bool missing_conventions=false;
            try {cli::create_model_stage(no_conventions,output,model,without_conventions,[]{});}
            catch(const cli::Error& e) {missing_conventions=e.code==2;}
            check(missing_conventions&&!fs::exists(no_conventions)&&fs::exists(output/"keep.txt"));
            bool bad_path_rejected=false;
            try {cli::create_model_stage(output,output,model,provenance,[]{});}
            catch(const cli::Error& e) {bad_path_rejected=e.code==5;}
            check(bad_path_rejected&&fs::exists(output/"keep.txt"));
            auto no_cancel=parent/"Segelmodell.arbeiten.no_cancel";
            bool missing_cancel_rejected=false;
            try {cli::create_model_stage(no_cancel,output,model,provenance,{});}
            catch(const cli::Error& e) {missing_cancel_rejected=e.code==4;}
            check(missing_cancel_rejected&&!fs::exists(no_cancel)&&fs::exists(output/"keep.txt"));
            auto before=parent/"Segelmodell.arbeiten.before";
            bool cancelled=false;
            try {
                cli::create_model_stage(before,output,model,provenance,
                                        []{throw cli::Error(130,"Testabbruch");});
            } catch(const cli::Error& e) { cancelled=e.code==130; }
            check(cancelled&&!fs::exists(before)&&fs::exists(output/"keep.txt"));
            auto during=parent/"Segelmodell.arbeiten.during";
            cancelled=false;
            try {
                cli::create_model_stage(during,output,model,provenance,[&]{
                    if(fs::exists(during/"streifen.parquet"))throw cli::Error(130,"Testabbruch");
                });
            } catch(const cli::Error& e) { cancelled=e.code==130; }
            check(cancelled&&!fs::exists(during)&&fs::exists(output/"keep.txt"));
            auto parallel_cancel=parent/"Segelmodell.arbeiten.parallel_cancel";
            cancelled=false;
            try {
                cli::create_model_stage(parallel_cancel,output,model,provenance,[&]{
                    if(fs::exists(parallel_cancel/"streifen.parquet"))
                        throw cli::Error(130,"Paralleler Testabbruch");
                },4);
            } catch(const cli::Error& e) { cancelled=e.code==130; }
            check(cancelled&&!fs::exists(parallel_cancel)&&fs::exists(output/"keep.txt"));
#ifdef _WIN32
            auto blocked_cleanup=parent/"Segelmodell.arbeiten.cleanup";
            HANDLE held=INVALID_HANDLE_VALUE;
            bool cleanup_reported=false;
            try {
                cli::create_model_stage(blocked_cleanup,output,model,provenance,[&]{
                    held=CreateFileW(blocked_cleanup.c_str(),FILE_LIST_DIRECTORY,FILE_SHARE_READ|FILE_SHARE_WRITE,
                                     nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,nullptr);
                    if(held==INVALID_HANDLE_VALUE)throw std::runtime_error("Testsperre konnte nicht geoeffnet werden");
                    throw cli::Error(130,"Testabbruch bei gesperrtem Ordner");
                });
            } catch(const cli::Error& e) {
                auto message=std::string(e.what());
                cleanup_reported=e.code==5&&message.find(blocked_cleanup.u8string())!=std::string::npos&&
                    message.find("Testabbruch bei gesperrtem Ordner")!=std::string::npos;
            }
            if(held!=INVALID_HANDLE_VALUE)CloseHandle(held);
            check(cleanup_reported&&fs::is_directory(blocked_cleanup)&&fs::exists(output/"keep.txt"));
            check(fs::remove(blocked_cleanup));
#endif
            auto invalid=parent/"Segelmodell.arbeiten.invalid";
            bool rejected=false;
            try {cli::create_model_stage(invalid,output,model,contradictory,[]{});}
            catch(const cli::Error&) {rejected=true;}
            check(rejected&&!fs::exists(invalid)&&fs::exists(output/"keep.txt"));
            auto occupied=parent/"Segelmodell.arbeiten.occupied";
            check(fs::create_directory(occupied));
            {std::ofstream out(occupied/"keep.txt");out<<"unrelated";}
            rejected=false;
            try {cli::create_model_stage(occupied,output,model,provenance,[]{});}
            catch(const cli::Error& e) {rejected=e.code==5;}
            check(rejected&&fs::exists(occupied/"keep.txt")&&fs::exists(output/"keep.txt"));
            auto stage=parent/"Segelmodell.arbeiten.complete";
            auto generated=cli::create_model_stage(stage,output,model,provenance,[]{});
            check(cli::read_model_manifest(stage).at("manifest")==generated&&
                  cli::read_model_manifest(stage).at("modell")==model&&fs::exists(output/"keep.txt"));
            check(fs::remove_all(parent)>0);
        }
        J manifest=cli::write_model_manifest(first_stage,model,metadata,provenance);
        cli::write_model_manifest(second_stage,model,second_metadata,provenance);
        J loaded=cli::read_model_manifest(first_stage);
        check(loaded["manifest"]==manifest&&loaded["modell"]==model);
        {
            auto negative_zero=model;
            negative_zero["streifen"][0]["abfrage_y_m"]=-0.0;
            auto positive_zero=negative_zero;
            positive_zero["streifen"][0]["abfrage_y_m"]=0.0;
            check(negative_zero==positive_zero);
            auto zero_stage=work/"segelform_signed_zero";
            check(std::filesystem::create_directory(zero_stage));
            auto zero_metadata=cli::write_model_tables(zero_stage,negative_zero);
            bool bit_mismatch_rejected=false;
            try {cli::write_model_manifest(zero_stage,positive_zero,zero_metadata,provenance);}
            catch(const cli::Error& e) {
                bit_mismatch_rejected=e.code==5&&
                    std::string(e.what()).find("nicht bitgleich")!=std::string::npos;
            }
            check(bit_mismatch_rejected&&!std::filesystem::exists(zero_stage/"manifest.json"));
            cli::write_model_manifest(zero_stage,negative_zero,zero_metadata,provenance);
            auto roundtrip=cli::read_model_manifest(zero_stage).at("modell");
            check(std::signbit(roundtrip["streifen"][0]["abfrage_y_m"].get<double>()));
            check(std::filesystem::remove_all(zero_stage)>0);
        }
#ifdef _WIN32
        {
            namespace fs=std::filesystem;
            const auto parent=work/"publication";
            check(fs::create_directory(parent));
            const auto output=parent/"Segelmodell";
            const J required=J::array({"rastervergleich_vorpruefung","nullwindfolge","gleichwind_vorpruefung",
                                       "h1_referenzvergleich_vorpruefung","tabellen_roundtrip_vorpruefung",
                                       "modell_lade_und_rechenprobe"});
            auto make_stage=[&](const std::string& token,bool complete) {
                auto stage=parent/("Segelmodell.arbeiten."+token);
                fs::copy(first_stage,stage,fs::copy_options::recursive);
                if(complete) {
                    auto ready=manifest;
                    ready["nachweise"]["ausgefuehrt"]=required;
                    ready["nachweise"]["bestanden"]=required;
                    ready["nachweise"]["nicht_ausgefuehrt"]=J::array();
                    std::ofstream out(stage/"manifest.json");out<<ready.dump();
                }
                return stage;
            };
            bool published=false,rejected=false;
            auto incomplete=make_stage("incomplete",false);
            try {cli::publish_model_stage(incomplete,output,false,published);}
            catch(const cli::Error& e) {rejected=e.code==4;}
            check(rejected&&!published&&fs::is_directory(incomplete)&&!fs::exists(output));
            bool callback_called=false;
            rejected=false;
            try {
                cli::publish_model_result(incomplete,output,false,[&](J&) {callback_called=true;});
            } catch(const cli::Error& e) {rejected=e.code==4;}
            check(rejected&&!callback_called&&fs::is_directory(incomplete)&&!fs::exists(output));

            auto missing_evidence=make_stage("missing_evidence",true);
            for(const auto& missing:required) {
                auto ready=manifest;
                J remaining=J::array();
                for(const auto& name:required)if(name!=missing)remaining.push_back(name);
                ready["nachweise"]["ausgefuehrt"]=remaining;
                ready["nachweise"]["bestanden"]=remaining;
                ready["nachweise"]["nicht_ausgefuehrt"]=J::array();
                {std::ofstream out(missing_evidence/"manifest.json");out<<ready.dump();}
                rejected=false;
                try {cli::publish_model_stage(missing_evidence,output,false,published);}
                catch(const cli::Error& e) {
                    rejected=e.code==4&&std::string(e.what()).find(missing.get<std::string>())!=std::string::npos;
                }
                check(rejected&&!published&&fs::is_directory(missing_evidence)&&!fs::exists(output));
            }

            auto missing_input=make_stage("missing_input",true);
            {
                auto ready=cli::read(missing_input/"manifest.json");
                ready["eingaben"].erase("konventionen");
                std::ofstream out(missing_input/"manifest.json");out<<ready.dump();
            }
            rejected=false;
            try {cli::publish_model_stage(missing_input,output,false,published);}
            catch(const cli::Error& e) {rejected=e.code==4;}
            check(rejected&&!published&&fs::is_directory(missing_input)&&!fs::exists(output));

            auto generated_provenance=provenance;
            generated_provenance["nachweise"]["ausgefuehrt"]=required;
            generated_provenance["nachweise"]["bestanden"]=required;
            generated_provenance["nachweise"]["nicht_ausgefuehrt"]=J::array(
                {"vollstaendige_numerikabnahme","datenformat_und_integritaet",
                 "end_to_end_integration","geschuetzte_publikation"});
            auto created=parent/"Segelmodell.arbeiten.create";
            auto generated=cli::create_model_stage(created,output,model,generated_provenance,[]{});
            check(generated["eingaben"]==generated_provenance["eingaben"]&&
                  generated["nachweise"]==generated_provenance["nachweise"]&&!fs::exists(output));
            {
                auto unready=generated;
                unready["nachweise"]["nicht_ausgefuehrt"].push_back("fehlende_modellpruefung");
                {std::ofstream out(created/"manifest.json");out<<unready.dump();}
                rejected=false;
                try {cli::publish_model_stage(created,output,false,published);}
                catch(const cli::Error& e) {rejected=e.code==4;}
                check(rejected&&!published&&fs::is_directory(created)&&!fs::exists(output));
                {std::ofstream out(created/"manifest.json");out<<generated.dump();}
            }
            check(!cli::publish_model_stage(created,output,false,published)&&published&&
                  !fs::exists(created)&&cli::read_model_manifest(output).at("modell")==model);
            {std::ofstream out(output/"sentinel.txt");out<<"old";}
            auto replacement=make_stage("replace",true);
            rejected=false;
            try {cli::publish_model_stage(replacement,output,false,published);}
            catch(const cli::Error& e) {rejected=e.code==5;}
            check(rejected&&!published&&fs::exists(output/"sentinel.txt")&&fs::is_directory(replacement));
            check(cli::publish_model_stage(replacement,output,true,published)&&published&&
                  !fs::exists(output/"sentinel.txt")&&!fs::exists(parent/"Segelmodell.ersetzen.replace")&&
                  cli::read_model_manifest(output).at("modell")==model);

            auto cancelled_publish=make_stage("cancelled_publish",true);
            cli::cancelled=true;
            rejected=false;
            try {cli::publish_model_result(cancelled_publish,output,true,{});}
            catch(const cli::Error& e) {rejected=e.code==130;}
            cli::cancelled=false;
            check(rejected&&fs::is_directory(cancelled_publish)&&
                  cli::read_model_manifest(output).at("modell")==model);

            auto fingerprint_failure=make_stage("fingerprint_failure",true);
            auto failed_result=cli::publish_model_result(fingerprint_failure,output,true,[](J&) {
                throw cli::Error(5,"Simulierter Fingerprintfehler");
            });
            check(failed_result.at("status")=="fehlgeschlagen"&&failed_result.at("exitcode")==5&&
                  failed_result.at("ergebnisordner")==output.u8string()&&
                  failed_result.at("output_action")=="written"&&
                  failed_result.at("meldung")=="Simulierter Fingerprintfehler"&&
                  !fs::exists(fingerprint_failure)&&
                  !fs::exists(parent/"Segelmodell.ersetzen.fingerprint_failure")&&
                  cli::read_model_manifest(output).at("modell")==model);
            auto input_failure=make_stage("input_failure",true);
            auto input_failure_result=cli::publish_model_result(input_failure,output,true,[](J&) {
                throw std::invalid_argument("Simulierter Eingabefehler");
            });
            check(input_failure_result.at("exitcode")==2&&
                  input_failure_result.at("ergebnisordner")==output.u8string()&&
                  !fs::exists(input_failure)&&cli::read_model_manifest(output).at("modell")==model);
            auto successful=make_stage("with_callback",true);
            auto successful_result=cli::publish_model_result(successful,output,true,[](J& result) {
                result["fingerprint"]={{"bereich","aerodynamics"},{"status","test"}};
            });
            check(successful_result.at("status")=="erfolgreich"&&successful_result.at("exitcode")==0&&
                  successful_result.at("ergebnisordner")==output.u8string()&&
                  successful_result.at("output_action")=="overwritten"&&
                  successful_result.at("fingerprint").at("bereich")=="aerodynamics"&&
                  !fs::exists(successful)&&cli::read_model_manifest(output).at("modell")==model);

            auto corrupt=make_stage("corrupt",true);
            {
                std::fstream file(corrupt/"kopplung.parquet",std::ios::in|std::ios::out|std::ios::binary);
                file.exceptions(std::ios::badbit|std::ios::failbit);
                file.seekg(10);char byte=0;file.get(byte);
                file.seekp(10);file.put(static_cast<char>(static_cast<unsigned char>(byte)^0xff));
            }
            rejected=false;
            try {cli::publish_model_stage(corrupt,output,true,published);}
            catch(const cli::Error&) {rejected=true;}
            check(rejected&&!published&&fs::is_directory(corrupt)&&
                  cli::read_model_manifest(output).at("modell")==model);

            auto extra=make_stage("extra",true);
            {std::ofstream out(extra/"unexpected.txt");out<<"unexpected";}
            rejected=false;
            try {cli::publish_model_stage(extra,output,true,published);}
            catch(const cli::Error& e) {rejected=e.code==5;}
            check(rejected&&!published&&fs::is_directory(extra)&&
                  cli::read_model_manifest(output).at("modell")==model);

            auto collision=make_stage("collision",true);
            const auto occupied_backup=parent/"Segelmodell.ersetzen.collision";
            check(fs::create_directory(occupied_backup));
            {std::ofstream out(occupied_backup/"keep.txt");out<<"unrelated";}
            rejected=false;
            try {cli::publish_model_stage(collision,output,true,published);}
            catch(const cli::Error& e) {rejected=e.code==5;}
            check(rejected&&!published&&fs::is_directory(collision)&&
                  fs::exists(occupied_backup/"keep.txt")&&cli::read_model_manifest(output).at("modell")==model);

            auto blocked=make_stage("rollback",true);
            {std::ofstream out(output/"sentinel.txt");out<<"old";}
            HANDLE held=CreateFileW(blocked.c_str(),FILE_LIST_DIRECTORY,FILE_SHARE_READ|FILE_SHARE_WRITE,
                                    nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,nullptr);
            check(held!=INVALID_HANDLE_VALUE);
            rejected=false;
            try {cli::publish_model_stage(blocked,output,true,published);}
            catch(const cli::Error& e) {rejected=e.code==5;}
            CloseHandle(held);
            check(rejected&&!published&&fs::exists(output/"sentinel.txt")&&fs::is_directory(blocked)&&
                  !fs::exists(parent/"Segelmodell.ersetzen.rollback")&&
                  cli::read_model_manifest(output).at("modell")==model);
            check(fs::remove_all(parent)>0);
        }
#endif
        auto runtime=segel::modell::laden(first_stage);
        auto points=runtime->punkte();
        check(points.size()==model["streifen"].size());
        auto modified_points=points;
        modified_points[0].position_m[0]=1000;
        check(runtime->punkte()[0].position_m==points[0].position_m);
        std::vector<segel::modell::Wind> runtime_winds;
        for(const auto& point:points)runtime_winds.push_back({point.id,{-10,1,0}});
        std::reverse(runtime_winds.begin(),runtime_winds.end());
        auto runtime_loads=runtime->berechnen(runtime_winds,1.225,1.5e-5);
        check(runtime_loads.size()==points.size());
        J raw_winds=J::array();
        for(const auto& wind:runtime_winds)raw_winds.push_back({{"streifen_id",wind.id},{"wind_m_s",wind.stroemung_m_s}});
        auto reference_loads=J::parse(segel::detail::solve_prepared_case(model.dump(),raw_winds.dump(),1.225,1.5e-5));
        for(size_t i=0;i<runtime_loads.size();++i)
            check(runtime_loads[i].id==i&&runtime_loads[i].kraft_N==reference_loads[i]["kraft_N"].get<segel::modell::Vektor>()&&
                  runtime_loads[i].moment_Nm==reference_loads[i]["moment_Nm"].get<segel::modell::Vektor>());
        auto expect_error=[&](segel::modell::Fehlerart category,auto fn) {
            bool failed=false;
            try { fn(); }
            catch(const segel::modell::Fehler& e) { failed=e.art()==category&&std::string(e.what()).size()>0; }
            check(failed);
        };
        auto duplicate=runtime_winds;
        duplicate[0].id=duplicate[1].id;
        expect_error(segel::modell::Fehlerart::eingabe,[&]{runtime->berechnen(duplicate,1.225,1.5e-5);});
        auto missing=runtime_winds;
        missing.pop_back();
        expect_error(segel::modell::Fehlerart::eingabe,[&]{runtime->berechnen(missing,1.225,1.5e-5);});
        auto unknown=runtime_winds;
        unknown[0].id=static_cast<segel::modell::StreifenId>(points.size());
        expect_error(segel::modell::Fehlerart::eingabe,[&]{runtime->berechnen(unknown,1.225,1.5e-5);});
        auto nonfinite=runtime_winds;
        nonfinite[0].stroemung_m_s[0]=std::numeric_limits<double>::quiet_NaN();
        expect_error(segel::modell::Fehlerart::eingabe,[&]{runtime->berechnen(nonfinite,1.225,1.5e-5);});
        expect_error(segel::modell::Fehlerart::gueltigkeit,[&]{runtime->berechnen(runtime_winds,2.0,1.5e-5);});
        expect_error(segel::modell::Fehlerart::gueltigkeit,[&]{runtime->berechnen(runtime_winds,1.225,1e-6);});
        expect_error(segel::modell::Fehlerart::eingabe,[&]{
            runtime->berechnen(runtime_winds,std::numeric_limits<double>::quiet_NaN(),1.5e-5);
        });
        auto fast=runtime_winds;
        fast[0].stroemung_m_s={51,0,0};
        expect_error(segel::modell::Fehlerart::gueltigkeit,[&]{runtime->berechnen(fast,1.225,1.5e-5);});
        auto zero_winds=runtime_winds;
        for(auto& wind:zero_winds)wind.stroemung_m_s={0,0,0};
        for(const auto& load:runtime->berechnen(zero_winds,1.0,1.4e-5))
            check(load.kraft_N==segel::modell::Vektor{0,0,0}&&load.moment_Nm==segel::modell::Vektor{0,0,0});
        auto limit_winds=runtime_winds;
        for(auto& wind:limit_winds)wind.stroemung_m_s={-50,0,0};
        check(runtime->berechnen(limit_winds,1.4,1.6e-5).size()==points.size());
        expect_error(segel::modell::Fehlerart::integritaet,[&]{segel::modell::laden(first_stage/"fehlt");});
        std::vector<std::future<std::vector<segel::modell::Last>>> parallel;
        for(int i=0;i<4;++i)parallel.push_back(std::async(std::launch::async,[&]{
            return runtime->berechnen(runtime_winds,1.225,1.5e-5);
        }));
        for(auto& task:parallel) {
            auto result=task.get();
            check(result.size()==runtime_loads.size());
            for(size_t i=0;i<result.size();++i)
                check(result[i].id==runtime_loads[i].id&&result[i].kraft_N==runtime_loads[i].kraft_N&&
                      result[i].moment_Nm==runtime_loads[i].moment_Nm);
        }
        auto invalid_manifest=manifest;
        invalid_manifest["modell_version"]="H1-lokal-2";
        {std::ofstream file(first_stage/"manifest.json");file<<invalid_manifest.dump();}
        bool version_rejected=false;
        try { cli::read_model_manifest(first_stage); }
        catch(const std::exception&) { version_rejected=true; }
        check(version_rejected);
        expect_error(segel::modell::Fehlerart::modell,[&]{segel::modell::laden(first_stage);});
        invalid_manifest=manifest;
        invalid_manifest["geometrie"]["streifenanzahl"]=1;
        {std::ofstream file(first_stage/"manifest.json");file<<invalid_manifest.dump();}
        expect_error(segel::modell::Fehlerart::modell,[&]{segel::modell::laden(first_stage);});
        {std::ofstream file(first_stage/"manifest.json");file<<manifest.dump();}
        J header=model;
        for(const char* key:{"streifen","quadratur","flaechenpunkte","kollokation","kopplung"})header.erase(key);
        check(cli::read_model_tables(first_stage,metadata,header)==model);
        auto rejects_model=[&](const std::filesystem::path& dir,const J& descriptions) {
            bool failed=false;
            try { cli::read_model_tables(dir,descriptions,header); }
            catch(const std::exception&) { failed=true; }
            check(failed);
        };
        auto invalid_metadata=metadata;
        invalid_metadata["streifen.parquet"]["schema_version"]="2";
        rejects_model(first_stage,invalid_metadata);
        invalid_metadata=metadata;
        invalid_metadata["streifen.parquet"]["schema"][0]["typ"]="f64";
        rejects_model(first_stage,invalid_metadata);
        invalid_metadata=metadata;
        invalid_metadata["streifen.parquet"]["zeilen"]=0;
        rejects_model(first_stage,invalid_metadata);
        invalid_metadata=metadata;
        invalid_metadata["streifen.parquet"]["sha256"]=std::string(64,'0');
        rejects_model(first_stage,invalid_metadata);
        {
            std::fstream file(second_stage/"kopplung.parquet",std::ios::in|std::ios::out|std::ios::binary);
            if(!file)throw std::runtime_error("Testdatei kann nicht beschaedigt werden");
            file.seekp(0);file.put('X');file.close();
            if(!file)throw std::runtime_error("Testdatei konnte nicht beschaedigt werden");
        }
        rejects_model(second_stage,second_metadata);
        auto corrupted_description=second_metadata;
        corrupted_description["kopplung.parquet"]["sha256"]=cli::hash(second_stage/"kopplung.parquet");
        rejects_model(second_stage,corrupted_description);
        expect_error(segel::modell::Fehlerart::integritaet,[&]{segel::modell::laden(second_stage);});
        auto corrupted_manifest=manifest;
        corrupted_manifest["tabellen"]=corrupted_description;
        {std::ofstream file(second_stage/"manifest.json");file<<corrupted_manifest.dump();}
        expect_error(segel::modell::Fehlerart::integritaet,[&]{segel::modell::laden(second_stage);});
        {
            std::fstream file(second_stage/"kopplung.parquet",std::ios::in|std::ios::out|std::ios::binary);
            if(!file)throw std::runtime_error("Testdatei kann nicht beschaedigt werden");
            file.seekp(0);file.write("PAR1",4);
            const char invalid_size[4]={char(0xff),char(0xff),char(0xff),char(0xff)};
            file.seekp(-8,std::ios::end);file.write(invalid_size,4);file.close();
            if(!file)throw std::runtime_error("Parquet-Metadaten konnten nicht beschaedigt werden");
        }
        corrupted_manifest["tabellen"]["kopplung.parquet"]["sha256"]=cli::hash(second_stage/"kopplung.parquet");
        {std::ofstream file(second_stage/"manifest.json");file<<corrupted_manifest.dump();}
        expect_error(segel::modell::Fehlerart::integritaet,[&]{segel::modell::laden(second_stage);});
        struct ExpectedTable { const char* file; const char* rows; std::vector<std::string> fields; };
        const std::array<ExpectedTable,5> expected{{
            {"streifen.parquet","streifen",{"streifen_id","z_min_m","z_max_m","flaeche_m2","abfrage_x_m","abfrage_y_m","abfrage_z_m"}},
            {"quadratur.parquet","quadratur",{"streifen_id","punkt_id","z_m","dz_m","chord_m","m","p","twist_rad","a0","a1","a2","viertel_x_m","viertel_y_m","viertel_z_m"}},
            {"flaechenpunkte.parquet","flaechenpunkte",{"streifen_id","punkt_id","teil_id","position_x_m","position_y_m","position_z_m","normal_x","normal_y","normal_z","flaeche_m2"}},
            {"kollokation.parquet","kollokation",{"moden","index","z_m","chord_m","twist_rad","a0"}},
            {"kopplung.parquet","kopplung",{"moden","zeile","spalte","wert"}}
        }};
        check(metadata.size()==expected.size());
        for(const auto& item:expected) {
            const auto& meta=metadata.at(item.file);
            check(meta["zeilen"]==model[item.rows].size()&&meta["schema_version"]=="1"&&meta["codec"]=="ZSTD");
            check(meta["schema"].size()==item.fields.size());
            for(size_t i=0;i<item.fields.size();++i) check(meta["schema"][i]["name"]==item.fields[i]);
            check(meta["sha256"]==second_metadata[item.file]["sha256"]);
            check(meta["sha256"]==cli::hash(first_stage/item.file));
            check_model_roundtrip(first_stage/item.file,model[item.rows],meta);
            std::filesystem::remove(first_stage/item.file);
            std::filesystem::remove(second_stage/item.file);
        }
        std::filesystem::remove(first_stage/"manifest.json");
        std::filesystem::remove(second_stage/"manifest.json");
        std::filesystem::remove(first_stage);std::filesystem::remove(second_stage);
        auto rectangular_geometry=input.at("geometrie");
        rectangular_geometry["grundkontur"]["punkte"]=J::array({
            {{"id","hals"},{"position_m",{0,0,0}}},{{"id","clew"},{"position_m",{-2,0,0}}},
            {{"id","aft"},{"position_m",{-2,0,4}}},{{"id","head"},{"position_m",{0,0,4}}}});
        rectangular_geometry["grundkontur"]["rand"]["kanten"]=J::array({
            {{"name","rand"},{"punkt_ids",{"hals","clew","aft","head","hals"}}}});
        rectangular_geometry["flaechenform"]["profilschnitte"]["stationen"]=J::array({
            {{"eta",0},{"woelbungsverhaeltnis",0},{"woelbungsposition_relativ",0.4},{"verwindung_deg",0}},
            {{"eta",1},{"woelbungsverhaeltnis",0},{"woelbungsposition_relativ",0.4},{"verwindung_deg",0}}});
        {std::ofstream out(work/"main.json");out<<rectangular_geometry.dump();}
        J model_job=cli::read(source/"Code"/"ressourcen"/"examples"/"Auftrag_Segelmodell.json");
        model_job["ressourcen"]["max_prozessorzahl"]=1;
        {std::ofstream out(work/"Auftragsdatei.json");out<<model_job.dump();}
        auto model_check=cli::dispatch({"pruefen","--auftrag",(work/"Auftragsdatei.json").u8string()});
        check(model_check["exitcode"]==0&&!std::filesystem::exists(work/"Segelmodell"));
        model_job["ressourcen"]["gpu"]="erforderlich";
        {std::ofstream out(work/"Auftragsdatei.json");out<<model_job.dump();}
        auto required_gpu=cli::dispatch({"pruefen","--auftrag",(work/"Auftragsdatei.json").u8string()});
        check(required_gpu["exitcode"]==3&&
              required_gpu["meldung"].get<std::string>().find("kein GPU-Backend")!=std::string::npos);
        auto required_gpu_model=cli::dispatch({"segelmodell","--auftrag",(work/"Auftragsdatei.json").u8string()});
        check(required_gpu_model["exitcode"]==3&&!std::filesystem::exists(work/"Segelmodell"));
        model_job["ressourcen"]["gpu"]="aus";
        {std::ofstream out(work/"Auftragsdatei.json");out<<model_job.dump();}
        std::filesystem::create_directory(work/"Segelmodell.lock");
        auto locked_check=cli::dispatch({"pruefen","--auftrag",(work/"Auftragsdatei.json").u8string()});
        check(locked_check["exitcode"]==0&&std::filesystem::exists(work/"Segelmodell.lock"));
        auto model_locked=cli::dispatch({"segelmodell","--auftrag",(work/"Auftragsdatei.json").u8string()});
        check(model_locked["exitcode"]==5&&std::filesystem::exists(work/"Segelmodell.lock"));
        std::filesystem::remove(work/"Segelmodell.lock");
        auto blocked=cli::dispatch({"segelmodell","--auftrag",(work/"Auftragsdatei.json").u8string()});
        check(blocked["exitcode"]==0&&std::filesystem::exists(work/"Segelmodell")&&
              !std::filesystem::exists(work/"Segelmodell.lock"));
        const auto& input_provenance=blocked.at("eingaben");
        for(const auto& item:std::array<std::pair<const char*,const char*>,3>{{
                {"auftrag","Auftragsdatei.json"},{"geometrie","main.json"},{"konventionen","konventionen.json"}}}) {
            const auto path=work/item.second;
            const auto& entry=input_provenance.at(item.first);
            check(entry.at("pfad")==path.u8string()&&entry.at("sha256")==cli::hash(path)&&
                  entry.at("schema_version")=="0.1");
        }
        check(blocked.at("ressourcen").at("cpu_effektiv")==1&&
              blocked.at("numerik_vorpruefung").at("gleichwind_worker_spitze")==1);
        check(blocked.at("output_action")=="created"&&
              blocked.at("nachweise").at("fehlgeschlagen").empty()&&
              blocked.at("nachweise").at("physikalisch_validiert")==false);
        std::filesystem::remove_all(work/"Segelmodell");
        model_job["ressourcen"]["max_prozessorzahl"]=4;
        {std::ofstream out(work/"Auftragsdatei.json");out<<model_job.dump();}
        auto parallel_uniform=cli::dispatch({"segelmodell","--auftrag",(work/"Auftragsdatei.json").u8string(),
                                             "--max-prozessorzahl","2"});
        const auto parallel_limit=parallel_uniform.at("ressourcen").at("cpu_effektiv").get<int>();
        const auto parallel_peak=parallel_uniform.at("numerik_vorpruefung").at("gleichwind_worker_spitze").get<int>();
        check(parallel_uniform.at("exitcode")==0&&parallel_limit>=1&&parallel_limit<=2&&
              parallel_peak>=1&&parallel_peak<=parallel_limit&&
              parallel_uniform.at("numerik_vorpruefung").at("gleichwind")==
                  blocked.at("numerik_vorpruefung").at("gleichwind")&&
              std::filesystem::exists(work/"Segelmodell")&&
              !std::filesystem::exists(work/"Segelmodell.lock"));
        std::filesystem::remove_all(work/"Segelmodell");
        {
            auto* previous=std::cerr.rdbuf();
            PhaseWatcher watcher(previous);
            struct Restore {
                std::streambuf* previous;
                ~Restore(){std::cerr.rdbuf(previous);cli::cancelled=false;}
            } restore{previous};
            std::cerr.rdbuf(&watcher);
            auto running=std::async(std::launch::async,[&]{
                return cli::dispatch({"segelmodell","--auftrag",(work/"Auftragsdatei.json").u8string(),
                                      "--max-prozessorzahl","2"});
            });
            bool started=watcher.wait_for_uniform();
            if(started)cli::cancelled=true;
            auto cancelled_uniform=running.get();
            check(started&&cancelled_uniform.at("exitcode")==130&&
                  !std::filesystem::exists(work/"Segelmodell")&&
                  !std::filesystem::exists(work/"Segelmodell.lock"));
            for(const auto& item:std::filesystem::directory_iterator(work))
                check(item.path().filename().u8string().find("Segelmodell.arbeiten.")!=0);
        }
        model_job["ressourcen"]["max_prozessorzahl"]=1;
        {std::ofstream out(work/"Auftragsdatei.json");out<<model_job.dump();}
        const auto& versions=blocked.at("bibliotheksversionen");
        check(blocked.at("werkzeugversion")==SEGELMACHER_VERSION);
        check(versions.at("arrow").at("status")=="nachgewiesen"&&
              versions.at("arrow").at("version")==arrow::GetBuildInfo().version_string&&
              !arrow::GetBuildInfo().version_string.empty());
#if defined(_WIN32) && defined(SEGELMACHER_ARROW_PACKAGE_SHA256) && defined(SEGELMACHER_PARQUET_PACKAGE_VERSION) && defined(SEGELMACHER_PARQUET_PACKAGE_SHA256)
        const auto& zstd_version=versions.at("zstd");
        if(zstd_version.at("status")=="nachgewiesen") {
            check(zstd_version.at("version").is_string()&&!zstd_version.at("version").get<std::string>().empty()&&
                  zstd_version.at("grundlage")=="parquet_arrow_21_paketidentitaet_und_zstd_import");
            HMODULE zstd_module=GetModuleHandleW(L"zstd.dll");
            if(zstd_module) {
                std::vector<wchar_t> path(32768);
                auto length=GetModuleFileNameW(zstd_module,path.data(),static_cast<DWORD>(path.size()));
                check(length>0&&length<path.size());
                if(cli::hash(std::filesystem::path(std::wstring(path.data(),length)))==zstd_version.at("sha256")) {
                    using ZstdVersion=const char* (__cdecl*)();
                    auto version_function=reinterpret_cast<ZstdVersion>(GetProcAddress(zstd_module,"ZSTD_versionString"));
                    check(version_function!=nullptr&&zstd_version.at("version")==version_function());
                }
            }
        } else check(zstd_version.at("status")=="nicht_nachgewiesen"&&zstd_version.at("version").is_null());
        auto wrong_arrow=cli::zstd_library_provenance(std::string(64,'0'),SEGELMACHER_PARQUET_PACKAGE_VERSION,
                                                       SEGELMACHER_PARQUET_PACKAGE_SHA256);
        check(wrong_arrow.at("status")=="nicht_nachgewiesen"&&wrong_arrow.at("version").is_null()&&
              wrong_arrow.at("grund")=="arrow_paket_stimmt_nicht_ueberein");
        auto wrong_parquet_for_zstd=cli::zstd_library_provenance(SEGELMACHER_ARROW_PACKAGE_SHA256,
                                        SEGELMACHER_PARQUET_PACKAGE_VERSION,std::string(64,'0'));
        check(wrong_parquet_for_zstd.at("status")=="nicht_nachgewiesen"&&wrong_parquet_for_zstd.at("version").is_null()&&
              wrong_parquet_for_zstd.at("grund")=="parquet_paket_stimmt_nicht_ueberein");
#else
        check(versions.at("zstd").at("status")=="nicht_nachgewiesen"&&
              versions.at("zstd").at("version").is_null());
#endif
        auto unknown_zstd=cli::zstd_library_provenance("","","");
        check(unknown_zstd.at("status")=="nicht_nachgewiesen"&&unknown_zstd.at("version").is_null());
#if defined(_WIN32) && defined(SEGELMACHER_PARQUET_PACKAGE_VERSION) && defined(SEGELMACHER_PARQUET_PACKAGE_SHA256)
        const auto& parquet_version=versions.at("parquet");
        check(parquet_version.at("status")=="nachgewiesen"&&
              parquet_version.at("version")==SEGELMACHER_PARQUET_PACKAGE_VERSION&&
              parquet_version.at("sha256")==SEGELMACHER_PARQUET_PACKAGE_SHA256&&
              parquet_version.at("grundlage")=="paketmetadaten_und_laufzeitdatei_sha256");
        auto wrong_parquet=cli::parquet_library_provenance(SEGELMACHER_PARQUET_PACKAGE_VERSION,std::string(64,'0'));
        check(wrong_parquet.at("status")=="nicht_nachgewiesen"&&wrong_parquet.at("version").is_null()&&
              wrong_parquet.at("grund")=="paketdatei_stimmt_nicht_ueberein");
#else
        check(versions.at("parquet").at("status")=="nicht_nachgewiesen"&&
              versions.at("parquet").at("version").is_null());
#endif
        auto unknown_parquet=cli::parquet_library_provenance("","");
        check(unknown_parquet.at("status")=="nicht_nachgewiesen"&&unknown_parquet.at("version").is_null());
        const auto& gradient_cases=blocked.at("numerik_vorpruefung").at("gradienten");
        check(gradient_cases.size()==14&&blocked["numerik_vorpruefung"]["weitere_nachweise"]=="nicht_ausgefuehrt");
        const auto& evidence=blocked.at("nachweise");
        auto evidence_has=[&](const char* state,const std::string& name) {
            const auto& entries=evidence.at(state);
            return std::find(entries.begin(),entries.end(),J(name))!=entries.end();
        };
        check(evidence.at("physikalisch_validiert")==false&&
              evidence_has("ausgefuehrt","rastervergleich_vorpruefung")&&
              evidence_has("ausgefuehrt","nullwindfolge")&&evidence_has("bestanden","nullwindfolge"));
        for(const char* missing:{"vollstaendige_numerikabnahme","datenformat_und_integritaet",
                                 "end_to_end_integration","geschuetzte_publikation"})
            check(evidence_has("nicht_ausgefuehrt",missing)&&!evidence_has("bestanden",missing));
        bool gradients_ok=std::all_of(gradient_cases.begin(),gradient_cases.end(),[](const J& item) {
            return item.at("status")=="bestanden";
        });
        const auto& uniform=blocked.at("numerik_vorpruefung").at("gleichwind");
        bool uniform_ok=gradients_ok&&std::none_of(uniform.begin(),uniform.end(),[](const J& item) {
            return item.at("status")=="fehlgeschlagen";
        });
        bool versions_ok=versions.at("parquet").at("status")=="nachgewiesen"&&
                         versions.at("zstd").at("status")=="nachgewiesen";
        const auto& table_roundtrip=blocked.at("numerik_vorpruefung").at("tabellen_roundtrip");
        if(uniform_ok&&versions_ok) {
            check(table_roundtrip.at("status")=="bestanden"&&
                  table_roundtrip.at("manifest_sha256").get<std::string>().size()==64&&
                  table_roundtrip.at("tabellen").size()==5&&
                  table_roundtrip.at("ladeprobe").at("status")=="bestanden"&&
                  table_roundtrip.at("ladeprobe").at("streifen")==16&&
                  evidence_has("ausgefuehrt","tabellen_roundtrip_vorpruefung")&&
                  evidence_has("bestanden","tabellen_roundtrip_vorpruefung")&&
                  evidence_has("ausgefuehrt","modell_lade_und_rechenprobe")&&
                  evidence_has("bestanden","modell_lade_und_rechenprobe"));
            for(const auto& item:table_roundtrip.at("tabellen").items())
                check(item.value().at("sha256").get<std::string>().size()==64&&
                      item.value().at("codec")=="ZSTD");
        } else {
            check(table_roundtrip.at("status")=="nicht_ausgefuehrt"&&
                  evidence_has("nicht_ausgefuehrt","tabellen_roundtrip_vorpruefung")&&
                  evidence_has("nicht_ausgefuehrt","modell_lade_und_rechenprobe"));
        }
        for(const auto& item:std::filesystem::directory_iterator(work))
            check(item.path().filename().u8string().find("Segelmodell.arbeiten.")!=0);
        check(evidence_has(gradients_ok?"bestanden":"fehlgeschlagen","rastervergleich_vorpruefung"));
        check(evidence_has(gradients_ok?"ausgefuehrt":"nicht_ausgefuehrt","gleichwind_vorpruefung"));
        for(const char* state:{"bestanden","fehlgeschlagen"})
            for(const auto& item:evidence.at(state))
                check(evidence_has("ausgefuehrt",item.get<std::string>())&&
                      !evidence_has("nicht_ausgefuehrt",item.get<std::string>()));
        for(const char* name:{"gegenlaeufig","azimut_naht","lokal_vertikal"})
            for(bool reverse:{false,true})
                check(std::count_if(gradient_cases.begin(),gradient_cases.end(),[&](const J& item) {
                    return item.at("windfeld")==name&&item.at("umgekehrt")==reverse;
                })==1);
        for(const auto& item:gradient_cases) {
            const auto& worst=item.at("schlechtester_vergleich");
            double error=worst.at("abweichung"),limit=worst.at("grenze"),ratio=worst.at("quotient");
            check(item.at("q_max_Pa").get<double>()>0&&worst.at("flaeche_m2").get<double>()>0&&
                  limit>0&&std::abs(ratio-error/limit)<1e-12&&
                  item.at("status")==J(ratio<=1?"bestanden":"fehlgeschlagen"));
            const auto& coarse_winds=item.at("wind_grob");
            const auto& fine_winds=item.at("wind_halbiert");
            check(coarse_winds.size()==16&&fine_winds.size()==32);
            double vmax=0;
            for(const auto* winds:{&coarse_winds,&fine_winds}) {
                for(size_t id=0;id<winds->size();++id) {
                    const auto& wind=(*winds)[id];
                    check(wind.at("streifen_id")==id&&wind.at("wind_m_s").size()==3);
                    auto v=wind.at("wind_m_s").get<std::array<double,3>>();
                    vmax=std::max(vmax,std::hypot(std::hypot(v[0],v[1]),v[2]));
                    if(item.at("windfeld")=="geschwindigkeit") {
                        double t=(id+0.5)/winds->size();
                        if(item.at("umgekehrt").get<bool>())t=1-t;
                        check(std::abs(v[0]+1+9*t)<1e-12&&v[1]==0&&v[2]==0);
                    }
                }
            }
            double q=item.at("q_max_Pa"),cref=item.at("c_ref_m");
            check(std::abs(q-0.5*1.225*vmax*vmax)<1e-12*std::max(1.0,q));
            const auto& comparisons=item.at("vergleiche");
            check(comparisons.size()==2*(coarse_winds.size()+1));
            double max_ratio=-1;
            for(size_t i=0;i<comparisons.size();++i) {
                const auto& comparison=comparisons[i];
                const bool moment=i%2==1;
                check(comparison.at("groesse")==J(moment?"moment_Nm":"kraft_N"));
                check(i/2<coarse_winds.size()?comparison.at("streifen_id")==J(i/2):
                                               comparison.at("streifen_id").is_null());
                const auto& coarse=comparison.at("wert_grob");
                const auto& fine=comparison.at("wert_halbiert");
                check(coarse.is_array()&&coarse.size()==3&&fine.is_array()&&fine.size()==3);
                double dx=fine[0].get<double>()-coarse[0].get<double>();
                double dy=fine[1].get<double>()-coarse[1].get<double>();
                double dz=fine[2].get<double>()-coarse[2].get<double>();
                double difference=comparison.at("abweichung"),bound=comparison.at("grenze");
                double area=comparison.at("flaeche_m2"),quotient=comparison.at("quotient");
                double expected=0.05*q*area*(moment?cref:1)+1e-9;
                check(area>0&&std::abs(bound-expected)<1e-12*std::max(1.0,expected)&&
                      std::abs(std::hypot(std::hypot(dx,dy),dz)-difference)<1e-12*std::max(1.0,difference)&&
                      std::abs(quotient-difference/bound)<1e-12);
                max_ratio=std::max(max_ratio,quotient);
            }
            check(std::abs(ratio-max_ratio)<1e-12);
            const auto& balances=item.at("beitragsbilanz_diagnostisch");
            for(const char* grid:{"grob","halbiert"}) {
                const auto& balance=balances.at(grid);
                check(balance.at("anzahl").get<size_t>()>0&&
                      balance.at("schlechtester_vergleich").at("quotient").get<double>()<1e-8);
            }
            const auto& modes=item.at("modenvergleiche_diagnostisch");
            check(modes.size()==4);
            for(size_t i=0;i<modes.size();++i) {
                const auto& comparison=modes[i].at("schlechtester_vergleich");
                double mode_error=comparison.at("abweichung"),mode_limit=comparison.at("grenze");
                double mode_ratio=comparison.at("quotient"),area=comparison.at("flaeche_m2");
                bool moment=comparison.at("groesse")=="moment_Nm";
                double expected=0.001*item.at("q_max_Pa").get<double>()*area*
                    (moment?item.at("c_ref_m").get<double>():1)+1e-9;
                check(modes[i].at("moden")==J(i%2==0?32:64)&&modes[i].at("referenzmoden")==48&&
                      modes[i].at("halbiertes_raster")==J(i>=2)&&area>0&&mode_limit>0&&
                      std::abs(mode_ratio-mode_error/mode_limit)<1e-12&&
                      std::abs(mode_limit-expected)<1e-12);
            }
        }
        const auto& zero_sequence=blocked.at("numerik_vorpruefung").at("nullwindfolge");
        check(zero_sequence.size()==7&&zero_sequence.back().at("geschwindigkeit_m_s")==0&&
              zero_sequence.back().at("gesamtkraft_N")==0&&zero_sequence.back().at("gesamtmoment_Nm")==0&&
              zero_sequence.back().at("max_streifenkraft_N")==0&&
              zero_sequence.back().at("max_streifenmoment_Nm")==0);
        const auto& near_zero=zero_sequence[zero_sequence.size()-2];
        check(near_zero.at("geschwindigkeit_m_s")==1e-12&&
              near_zero.at("gesamtkraft_N").get<double>()<=1e-9&&
              near_zero.at("gesamtmoment_Nm").get<double>()<=1e-9&&
              near_zero.at("max_streifenkraft_N").get<double>()<=1e-9&&
              near_zero.at("max_streifenmoment_Nm").get<double>()<=1e-9);
        const auto& continuity=blocked.at("numerik_vorpruefung").at("stetigkeit_diagnostisch");
        check(continuity.size()==6);
        const std::array<const char*,3> continuity_names={"gegenwind_naht","lokal_vertikal","lokaler_nullwind"};
        for(size_t i=0;i<continuity.size();++i) {
            const auto& item=continuity[i];
            double delta=item.at("delta_m_s"),q=item.at("q_max_Pa"),cref=item.at("c_ref_m");
            const auto& comparison=item.at("schlechtester_vergleich");
            double error=comparison.at("abweichung"),limit=comparison.at("grenze");
            double area=comparison.at("flaeche_m2"),ratio=comparison.at("quotient");
            bool moment=comparison.at("groesse")=="moment_Nm";
            check(item.at("windfall")==continuity_names[i/2]&&delta==(i%2==0?-5e-8:5e-8)&&
                  q>0&&cref>0&&area>0&&limit>0&&
                  std::abs(ratio-error/limit)<1e-12&&
                  std::abs(limit-(0.001*q*area*(moment?cref:1)+1e-9))<1e-12);
        }
        const auto& re=blocked.at("numerik_vorpruefung").at("re_min_diagnostisch");
        check(re.at("quadraturpunkte").get<size_t>()==
              re.at("gepruefte_punkte").get<size_t>()+re.at("ausserhalb_0_bis_50_m_s").get<size_t>()&&
              re.at("gepruefte_punkte").get<size_t>()>0&&re.at("relativer_abstand")==1e-6&&
              !re.at("schlechtester_vergleich").is_null());
        const auto& re_cases=re.at("faelle");
        check(re_cases.size()==re.at("quadraturpunkte").get<size_t>()&&
              re_cases[0].at("status")=="geprueft"&&re_cases[0].at("vergleiche").size()==2&&
              re_cases[0].at("vergleiche")[0].at("faktor")==1-1e-6&&
              re_cases[0].at("vergleiche")[1].at("faktor")==1+1e-6);
        const auto& uniform_cases=blocked.at("numerik_vorpruefung").at("gleichwind");
        check(uniform_cases.is_array()&&uniform_cases.size()==23);
        const J rectangular_source={{"geometrie",rectangular_geometry},{"numerik",model_job.at("numerik")},
                                    {"parameter",model_job.at("berechnung").at("parameter")}};
        const J rectangular_model=J::parse(segel::detail::prepare_model_data(rectangular_source.dump()));
        segel::detail::PreparedModel transition_solver(rectangular_model.dump());
        bool compared_h1=false;
        for(const auto& item:uniform_cases) {
            double speed=item.at("geschwindigkeit_m_s"),rho=item.at("dichte_kg_m3");
            double q=item.at("q_max_Pa"),cref=item.at("c_ref_m");
            size_t model_cases=item.at("modellfaelle"),transition_cases=item.at("uebergangsfaelle");
            check(item.at("grundfaelle")==108&&model_cases>=108&&model_cases>=transition_cases&&
                  model_cases<=108+transition_cases&&transition_cases>0&&rho==1.225&&
                  item.at("viskositaet_m2_s")==1.5e-5&&
                  std::abs(q-0.5*rho*speed*speed)<=1e-12*std::max(1.0,q)&&
                  cref>0&&item.at("fehlgeschlagene_faelle").is_array());
            check(item.at("polpruefung").at("status")=="bestanden"&&
                  item.at("polpruefung").at("vergleiche")==24);
            const auto& transition=item.at("uebergang_diagnostisch");
            auto worst_transition=transition.at("schlechtester_vergleich");
            check(transition.at("vergleiche")==18+8*rectangular_model.at("quadratur").size()&&
                  transition.at("bezug")=="0,1 Prozent diagnostisch, keine eigenstaendige Abnahmegrenze");
            auto solve_transition=[&](double alpha,double beta) {
                auto wind=segel::detail::uniform_wind(speed,alpha,beta);
                J winds=J::array();
                for(const auto& strip:rectangular_model.at("streifen"))
                    winds.push_back({{"streifen_id",strip.at("streifen_id")},{"wind_m_s",wind}});
                return J::parse(transition_solver.solve(winds.dump(),rho,1.5e-5));
            };
            const auto reference=solve_transition(worst_transition.at("referenz_azimut_deg"),
                                                   worst_transition.at("referenz_neigung_deg"));
            const auto candidate=solve_transition(worst_transition.at("azimut_deg"),
                                                   worst_transition.at("neigung_deg"));
            const double transition_q=worst_transition.at("q_max_Pa");
            worst_transition.erase("referenz_azimut_deg");worst_transition.erase("referenz_neigung_deg");
            worst_transition.erase("azimut_deg");worst_transition.erase("neigung_deg");
            worst_transition.erase("q_max_Pa");
            check(worst_transition==segel::detail::compare_same_grid_loads(
                      rectangular_model,reference,candidate,transition_q));
            if(speed==0)check(worst_transition.at("quotient")==0);
            if(item.at("h1_faelle")==0) {
                check(item.at("status")=="h1_nicht_anwendbar"&&item.at("schlechtester_vergleich").is_null());
            } else {
                compared_h1=true;
                const auto& worst=item.at("schlechtester_vergleich");
                double error=worst.at("abweichung"),limit=worst.at("grenze"),ratio=worst.at("quotient");
                double area=worst.at("flaeche_m2");
                bool moment=worst.at("groesse").get<std::string>().find("moment")!=std::string::npos;
                auto wind=segel::detail::uniform_wind(speed,worst.at("azimut_deg"),worst.at("neigung_deg"));
                double actual_speed=std::hypot(std::hypot(wind[0],wind[1]),wind[2]);
                double case_q=0.5*rho*actual_speed*actual_speed;
                auto h1_cases=item.at("h1_faelle").get<size_t>();
                const char* status=ratio>1?"fehlgeschlagen":
                    (h1_cases<model_cases?"h1_teilweise_nicht_anwendbar":"bestanden");
                check(h1_cases<=model_cases&&worst.at("q_max_Pa")==case_q&&
                      limit>0&&std::abs(ratio-error/limit)<1e-12&&
                      area>0&&std::abs(limit-(0.001*case_q*area*(moment?cref:1)+1e-9))<1e-12&&
                      worst.at("azimut_deg").is_number()&&worst.at("neigung_deg").is_number()&&
                      item.at("fehlgeschlagene_faelle").empty()==(ratio<=1)&&
                      item.at("status")==J(status));
                for(const auto& failure:item.at("fehlgeschlagene_faelle"))
                    check(failure.at("quotient").get<double>()>1&&failure.at("azimut_deg").is_number()&&
                          failure.at("neigung_deg").is_number());
            }
        }
        check(compared_h1);
        std::filesystem::create_directory(work/"Segelmodell");
        {std::ofstream out(work/"Segelmodell"/"keep.txt");out<<"previous model";}
        auto model_skipped=cli::dispatch({"segelmodell","--auftrag",(work/"Auftragsdatei.json").u8string()});
        check(model_skipped["exitcode"]==0&&model_skipped["output_action"]=="skipped"&&
              !std::filesystem::exists(work/"Segelmodell.lock"));
        auto model_overwrite=cli::dispatch({"segelmodell","--auftrag",(work/"Auftragsdatei.json").u8string(),
                                             "--output-policy","overwrite"});
        check(model_overwrite["exitcode"]==0&&model_overwrite["output_action"]=="overwritten"&&
              !std::filesystem::exists(work/"Segelmodell"/"keep.txt")&&
              !std::filesystem::exists(work/"Segelmodell.lock"));
        std::filesystem::remove_all(work/"Segelmodell");
        std::filesystem::copy_file(reference,work/"main.json",std::filesystem::copy_options::overwrite_existing);
        {std::ofstream out(work/"Auftragsdatei.json");out<<job.dump();}
        auto valid_job=cli::dispatch({"pruefen","--auftrag",(work/"Auftragsdatei.json").u8string()});
        check(valid_job["exitcode"]==0);
        J legacy_job=job;
        legacy_job["betriebspunkte"]["windneigung_deg"]=J::array({0,10,20,30,40,50,60,70,80,90});
        {std::ofstream out(work/"Auftragsdatei.json");out<<legacy_job.dump();}
        auto legacy_status=cli::dispatch({"pruefen","--auftrag",(work/"Auftragsdatei.json").u8string()});
        check(legacy_status["exitcode"]==2&&
              legacy_status["meldung"].get<std::string>().find("alte Auftraege")!=std::string::npos);
        J expanded=job;
        expanded["betriebspunkte"]["anstellwinkel_deg"]["schritt"]=1;
        expanded["betriebspunkte"]["reynoldszahlen_referenz"]=J::array();
        for(int i=0;i<31;++i)expanded["betriebspunkte"]["reynoldszahlen_referenz"].push_back(200000+i*1000);
        {std::ofstream out(work/"Auftragsdatei.json");out<<expanded.dump();}
        auto oversized=cli::dispatch({"lookup","--auftrag",(work/"Auftragsdatei.json").u8string()});
        check(oversized["exitcode"]==3&&
              oversized["meldung"].get<std::string>().find("Erweiterter Rechenplan")!=std::string::npos);
        check(!std::filesystem::exists(work/"LookUp")&&!std::filesystem::exists(work/"LookUp.lock"));
        {std::ofstream out(work/"Auftragsdatei.json");out<<job.dump();}
        cli::cancelled=true;
        auto aborted=cli::dispatch({"lookup","--auftrag",(work/"Auftragsdatei.json").u8string()});
        cli::cancelled=false;
        check(aborted["exitcode"]==130);
        check(!std::filesystem::exists(work/"LookUp"));
        check(!std::filesystem::exists(work/"LookUp.lock"));
        check(!aborted["pruefbericht"].is_null());
        std::filesystem::create_directory(work/"LookUp");
        {std::ofstream out(work/"LookUp"/"keep.txt");out<<"previous lookup";}
        auto skipped=cli::dispatch({"lookup","--auftrag",(work/"Auftragsdatei.json").u8string()});
        check(skipped["exitcode"]==0&&skipped["output_action"]=="skipped");
        cli::cancelled=true;
        aborted=cli::dispatch({"lookup","--auftrag",(work/"Auftragsdatei.json").u8string(),
            "--output-policy","overwrite"});
        cli::cancelled=false;
        check(aborted["exitcode"]==130);
        check(std::filesystem::exists(work/"LookUp"/"keep.txt"));
        check(!std::filesystem::exists(work/"LookUp.lock"));
        std::cout<<checks<<" CLI contract checks passed\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
