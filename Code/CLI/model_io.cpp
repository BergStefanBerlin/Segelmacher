#include "job.hpp"
#include "parquet_internal.hpp"
#include "../Core/src/segelform_internal.hpp"
#include <parquet/arrow/reader.h>
#include <parquet/file_reader.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <exception>
#include <iomanip>
#include <limits>
#include <map>
#include <mutex>
#include <sstream>
#include <system_error>
#include <thread>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#endif
namespace cli {
namespace fs=std::filesystem;
using detail::ensure;
using detail::fields;
using detail::numeric;
using detail::write;
using detail::number_scalar;
using detail::ParquetTableWriter;
namespace {
J parse_json(std::istream& in) {
    std::vector<std::set<std::string>> keys;
    return J::parse(in,[&keys](int,J::parse_event_t event,J& v) {
        if(event==J::parse_event_t::object_start)keys.emplace_back();
        if(event==J::parse_event_t::key)
            ensure(keys.back().insert(v.get<std::string>()).second,2,"Doppelter JSON-Schluessel: "+v.get<std::string>());
        if(event==J::parse_event_t::object_end)keys.pop_back();
        return true;
    });
}
J read_json(const fs::path& path,int io_code) {
    try {
        std::ifstream in(path,std::ios::binary);ensure(in.good(),io_code,"Eingabepfad nicht lesbar: "+path.u8string());
        ensure(std::filesystem::file_size(path)<=16*1024*1024,3,"JSON-Eingabe groesser als 16 MiB: "+path.u8string());
        return parse_json(in);
    } catch(const Error& e){throw Error(e.code,path.u8string()+": "+e.what());}
    catch(const std::bad_alloc&){throw Error(3,"JSON-Eingabe: Speicher erschoepft");}
    catch(const fs::filesystem_error& e){throw Error(io_code,path.u8string()+": "+e.what());}
    catch(const std::exception& e) {throw Error(2,path.u8string()+": "+e.what());}
}
}
J read(const fs::path& path) { return read_json(path,2); }
std::string hash(const fs::path& path) {
#ifdef _WIN32
    BCRYPT_ALG_HANDLE alg=nullptr;BCRYPT_HASH_HANDLE h=nullptr;
    ensure(BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0,5,"SHA256-Provider nicht verfuegbar");
    DWORD len=0,got=0;
    auto status=BCryptGetProperty(alg,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&len),sizeof(len),&got,0);
    std::vector<unsigned char> obj(len),digest(32);
    if(status>=0)status=BCryptCreateHash(alg,&h,obj.data(),len,nullptr,0,0);
    std::ifstream in(path,std::ios::binary);char data[65536];
    bool readable=in.good();
    while(status>=0&&in) {
        in.read(data,sizeof(data));
        if(in.gcount()>0)status=BCryptHashData(h,reinterpret_cast<PUCHAR>(data),static_cast<ULONG>(in.gcount()),0);
    }
    if(status>=0)status=BCryptFinishHash(h,digest.data(),32,0);
    if(h)BCryptDestroyHash(h);BCryptCloseAlgorithmProvider(alg,0);
    ensure(readable&&!in.bad()&&status>=0,5,"SHA256 fehlgeschlagen: "+path.u8string());
    std::ostringstream s;s<<std::hex<<std::setfill('0');for(auto x:digest)s<<std::setw(2)<<static_cast<int>(x);return s.str();
#else
    throw Error(3,"CLI-SHA256 derzeit nur Windows BCrypt; Core bleibt plattformunabhaengig");
#endif
}
namespace {
std::string hash_buffer(const arrow::Buffer& buffer,const fs::path& path) {
#ifdef _WIN32
    ensure(buffer.size()>=0&&static_cast<uint64_t>(buffer.size())<=std::numeric_limits<ULONG>::max(),
           3,"Segelmodell-Datei zu gross fuer SHA256: "+path.u8string());
    BCRYPT_ALG_HANDLE alg=nullptr;
    BCRYPT_HASH_HANDLE h=nullptr;
    ensure(BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0,5,"SHA256-Provider nicht verfuegbar");
    struct AlgorithmGuard {
        BCRYPT_ALG_HANDLE handle;
        ~AlgorithmGuard() { BCryptCloseAlgorithmProvider(handle,0); }
    } algorithm_guard{alg};
    DWORD len=0,got=0;
    auto status=BCryptGetProperty(alg,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&len),sizeof(len),&got,0);
    std::vector<unsigned char> obj(len),digest(32);
    if(status>=0)status=BCryptCreateHash(alg,&h,obj.data(),len,nullptr,0,0);
    struct HashGuard {
        BCRYPT_HASH_HANDLE handle;
        ~HashGuard() { if(handle)BCryptDestroyHash(handle); }
    } hash_guard{h};
    if(status>=0)status=BCryptHashData(h,const_cast<PUCHAR>(buffer.data()),static_cast<ULONG>(buffer.size()),0);
    if(status>=0)status=BCryptFinishHash(h,digest.data(),32,0);
    ensure(status>=0,5,"SHA256 fehlgeschlagen: "+path.u8string());
    std::ostringstream s;
    s<<std::hex<<std::setfill('0');
    for(auto x:digest)s<<std::setw(2)<<static_cast<int>(x);
    return s.str();
#else
    throw Error(3,"CLI-SHA256 derzeit nur Windows BCrypt; Core bleibt plattformunabhaengig");
#endif
}
}
JsonSnapshot read_json_snapshot(const fs::path& path) {
    try {
        std::ifstream in(path,std::ios::binary);
        ensure(in.good(),2,"Eingabepfad nicht lesbar: "+path.u8string());
        std::string bytes;
        std::array<char,65536> block{};
        while(in) {
            in.read(block.data(),static_cast<std::streamsize>(block.size()));
            const auto count=in.gcount();
            ensure(count>=0&&bytes.size()+static_cast<size_t>(count)<=16*1024*1024,3,
                   "JSON-Eingabe groesser als 16 MiB: "+path.u8string());
            bytes.append(block.data(),static_cast<size_t>(count));
        }
        ensure(in.eof()&&!in.bad(),2,"JSON-Eingabe kann nicht vollstaendig gelesen werden: "+path.u8string());
        std::istringstream input(bytes);
        J value=parse_json(input);
        return {std::move(value),std::move(bytes)};
    } catch(const Error& e){throw Error(e.code,path.u8string()+": "+e.what());}
    catch(const std::bad_alloc&){throw Error(3,"JSON-Eingabe: Speicher erschoepft");}
    catch(const fs::filesystem_error& e){throw Error(2,path.u8string()+": "+e.what());}
    catch(const std::exception& e){throw Error(2,path.u8string()+": "+e.what());}
}
std::string hash_json_snapshot(const JsonSnapshot& snapshot,const fs::path& path) {
    try {
        const arrow::Buffer buffer(reinterpret_cast<const uint8_t*>(snapshot.bytes.data()),
                                   static_cast<int64_t>(snapshot.bytes.size()));
        return hash_buffer(buffer,path);
    } catch(const std::bad_alloc&) {
        throw Error(3,"JSON-Eingabe: Speicher erschoepft");
    }
}
HashedJson read_hashed_json(const fs::path& path) {
    auto snapshot=read_json_snapshot(path);
    auto digest=hash_json_snapshot(snapshot,path);
    return {std::move(snapshot.value),std::move(digest)};
}
namespace {
struct ModelColumn { const char* name; bool integer; const char* unit; };
struct ModelTable { const char* file; const char* rows; int64_t max_rows; std::vector<ModelColumn> columns; };
const std::vector<ModelTable>& model_tables() {
    static const std::vector<ModelTable> tables={
        {"streifen.parquet","streifen",10000,{
            {"streifen_id",true,"1"},{"z_min_m",false,"m"},{"z_max_m",false,"m"},{"flaeche_m2",false,"m^2"},
            {"abfrage_x_m",false,"m"},{"abfrage_y_m",false,"m"},{"abfrage_z_m",false,"m"}}},
        {"quadratur.parquet","quadratur",10000,{
            {"streifen_id",true,"1"},{"punkt_id",true,"1"},{"z_m",false,"m"},{"dz_m",false,"m"},
            {"chord_m",false,"m"},{"m",false,"1"},{"p",false,"1"},{"twist_rad",false,"rad"},
            {"a0",false,"rad"},{"a1",false,"rad"},{"a2",false,"rad"},
            {"viertel_x_m",false,"m"},{"viertel_y_m",false,"m"},{"viertel_z_m",false,"m"}}},
        {"flaechenpunkte.parquet","flaechenpunkte",40000,{
            {"streifen_id",true,"1"},{"punkt_id",true,"1"},{"teil_id",true,"1"},
            {"position_x_m",false,"m"},{"position_y_m",false,"m"},{"position_z_m",false,"m"},
            {"normal_x",false,"1"},{"normal_y",false,"1"},{"normal_z",false,"1"},{"flaeche_m2",false,"m^2"}}},
        {"kollokation.parquet","kollokation",144,{
            {"moden",true,"1"},{"index",true,"1"},{"z_m",false,"m"},{"chord_m",false,"m"},
            {"twist_rad",false,"rad"},{"a0",false,"rad"}}},
        {"kopplung.parquet","kopplung",7424,{
            {"moden",true,"1"},{"zeile",true,"1"},{"spalte",true,"1"},{"wert",false,"1"}}}
    };
    return tables;
}
J model_table_schema(const ModelTable& table) {
    J schema=J::array();
    for(const auto& column:table.columns)
        schema.push_back({{"name",column.name},{"typ",column.integer?"i32":"f64"},{"einheit",column.unit}});
    return schema;
}
}
J write_model_tables(const fs::path& stage,const J& model_data,const std::function<void()>& check_cancel,
                     int workers) {
    ensure(fs::is_directory(stage)&&fs::is_empty(stage),5,"Segelmodell-Arbeitsordner fehlt oder ist nicht leer");
    ensure(workers>0,3,"Segelmodell-Workerzahl muss positiv sein");
    if(check_cancel)check_cancel();
    segel::detail::validate_model_data_json(model_data.dump());
    if(check_cancel)check_cancel();
    const auto& tables=model_tables();
    std::vector<J> results(tables.size());
    std::atomic<size_t> next{0};
    std::atomic<bool> failed{false};
    std::exception_ptr failure;
    std::mutex failure_mutex;
    auto write_table=[&](size_t index) {
        const auto& table=tables[index];
        if(check_cancel)check_cancel();
        std::vector<std::shared_ptr<arrow::Field>> fields;
        for(const auto& column:table.columns) {
            fields.push_back(arrow::field(column.name,column.integer?arrow::int32():arrow::float64(),false));
        }
        ParquetTableWriter writer(stage/table.file,arrow::schema(fields));
        size_t rows=0;
        for(const auto& row:model_data.at(table.rows)) {
            if(failed)return;
            if(check_cancel&&rows%256==0)check_cancel();
            std::vector<std::shared_ptr<arrow::Scalar>> values;
            values.reserve(table.columns.size());
            for(const auto& column:table.columns) {
                if(column.integer) {
                    auto value=row.at(column.name).get<long long>();
                    ensure(value>=0&&value<=std::numeric_limits<int32_t>::max(),4,"Segelmodell-Index ausserhalb i32");
                    values.push_back(std::make_shared<arrow::Int32Scalar>(static_cast<int32_t>(value)));
                } else values.push_back(number_scalar(row.at(column.name).get<double>()));
            }
            writer.append(std::move(values));
            ++rows;
        }
        if(failed)return;
        writer.close();
        if(check_cancel)check_cancel();
        results[index]={{"zeilen",writer.row_count()},{"schema_version","1"},{"codec","ZSTD"},
                        {"sha256",hash(stage/table.file)},{"schema",model_table_schema(table)}};
        if(check_cancel)check_cancel();
    };
    auto worker=[&] {
        try {
#ifdef _WIN32
            if(workers>1)ensure(SetThreadPriority(GetCurrentThread(),THREAD_MODE_BACKGROUND_BEGIN)!=0,3,
                                "Hintergrundprioritaet fuer Tabellen-Worker kann nicht gesetzt werden");
#endif
            while(!failed) {
                const auto index=next.fetch_add(1);
                if(index>=tables.size())break;
                write_table(index);
            }
#ifdef _WIN32
            if(workers>1)ensure(SetThreadPriority(GetCurrentThread(),THREAD_MODE_BACKGROUND_END)!=0,3,
                                "Hintergrundprioritaet fuer Tabellen-Worker kann nicht beendet werden");
#endif
        } catch(...) {
            std::lock_guard<std::mutex> guard(failure_mutex);
            if(!failure)failure=std::current_exception();
            failed=true;
        }
    };
    if(workers==1)worker();
    else {
        std::vector<std::thread> threads;
        try {
            for(size_t i=0;i<std::min(static_cast<size_t>(workers),tables.size());++i)
                threads.emplace_back(worker);
        } catch(const std::system_error& e) {
            failed=true;
            for(auto& thread:threads)thread.join();
            throw Error(3,std::string("Tabellen-Worker konnten nicht gestartet werden: ")+e.what());
        } catch(...) {
            failed=true;
            for(auto& thread:threads)thread.join();
            throw;
        }
        for(auto& thread:threads)thread.join();
    }
    if(failure)std::rethrow_exception(failure);
    J metadata=J::object();
    for(size_t i=0;i<tables.size();++i)metadata[tables[i].file]=std::move(results[i]);
    return metadata;
}
J read_model_tables(const fs::path& stage,const J& metadata,J model_data) {
    ensure(fs::is_directory(stage),5,"Segelmodell-Verzeichnis fehlt");
    ensure(metadata.is_object()&&metadata.size()==model_tables().size(),5,"Segelmodell-Tabellenverzeichnis ungueltig");
    ensure(model_data.is_object(),5,"Segelmodell-Kopfdaten ungueltig");
    for(const auto& table:model_tables()) {
        ensure(!model_data.contains(table.rows),5,"Segelmodell-Kopfdaten enthalten bereits Tabellen");
        ensure(metadata.contains(table.file)&&metadata.at(table.file).is_object(),5,
               std::string("Segelmodell-Tabellenbeschreibung fehlt: ")+table.file);
        const auto& meta=metadata.at(table.file);
        fields(meta,{"zeilen","schema_version","codec","sha256","schema"},table.file);
        ensure(meta.contains("zeilen")&&meta.at("zeilen").is_number_integer()&&
               meta.at("zeilen").get<int64_t>()>=0&&meta.at("zeilen").get<int64_t>()<=table.max_rows&&
               meta.value("schema_version",std::string{})=="1"&&meta.value("codec",std::string{})=="ZSTD"&&
               meta.contains("sha256")&&meta.at("sha256").is_string()&&
               meta.contains("schema")&&meta.at("schema")==model_table_schema(table),5,
               std::string("Ungueltige Segelmodell-Tabellenbeschreibung: ")+table.file);
        const auto path=stage/table.file;
        auto file=arrow::io::ReadableFile::Open(path.u8string());
        ensure(file.ok(),5,"Segelmodell-Datei kann nicht geoeffnet werden: "+file.status().ToString());
        auto size=(*file)->GetSize();
        ensure(size.ok(),5,"Segelmodell-Dateigroesse kann nicht gelesen werden: "+size.status().ToString());
        ensure(*size>=0&&*size<=128*1024*1024,3,"Segelmodell-Datei ueberschreitet Ressourcenlimit: "+path.u8string());
        auto buffer=(*file)->ReadAt(0,*size);
        ensure(buffer.ok()&&(*buffer)->size()==*size,5,"Segelmodell-Datei kann nicht vollstaendig gelesen werden: "+path.u8string());
        ensure(hash_buffer(**buffer,path)==meta.at("sha256").get<std::string>(),5,
               std::string("Segelmodell-Pruefsumme stimmt nicht: ")+table.file);
        ensure((*buffer)->size()>=8&&std::memcmp((*buffer)->data(),"PAR1",4)==0&&
               std::memcmp((*buffer)->data()+(*buffer)->size()-4,"PAR1",4)==0,5,
               std::string("Segelmodell-Parquet-Signatur ungueltig: ")+table.file);
        auto input=std::make_shared<arrow::io::BufferReader>(*buffer);
        auto maybe_reader=parquet::arrow::OpenFile(input,arrow::default_memory_pool());
        ensure(maybe_reader.ok(),5,"Segelmodell-Parquet kann nicht gelesen werden: "+maybe_reader.status().ToString());
        auto reader=std::move(maybe_reader).ValueOrDie();
        const auto file_meta=reader->parquet_reader()->metadata();
        ensure(file_meta->num_rows()==meta.at("zeilen").get<int64_t>()&&
               file_meta->num_columns()==static_cast<int>(table.columns.size()),5,
               std::string("Segelmodell-Parquet-Dimensionen ungueltig: ")+table.file);
        for(int group=0;group<file_meta->num_row_groups();++group)
            for(int column=0;column<file_meta->num_columns();++column)
                ensure(file_meta->RowGroup(group)->ColumnChunk(column)->compression()==parquet::Compression::ZSTD,5,
                       std::string("Segelmodell-Parquet-Codec ungueltig: ")+table.file);
        std::shared_ptr<arrow::Table> data;
        auto status=reader->ReadTable(&data);
        ensure(status.ok(),5,"Segelmodell-Parquet kann nicht dekodiert werden: "+status.ToString());
        for(size_t c=0;c<table.columns.size();++c) {
            const auto& column=table.columns[c];
            auto field=data->schema()->field(static_cast<int>(c));
            ensure(field->name()==column.name&&!field->nullable()&&
                   field->type()->id()==(column.integer?arrow::Type::INT32:arrow::Type::DOUBLE)&&
                   data->column(static_cast<int>(c))->null_count()==0,5,
                   std::string("Segelmodell-Parquet-Spalte ungueltig: ")+column.name);
        }
        J rows=J::array();
        for(int64_t r=0;r<data->num_rows();++r) {
            J row=J::object();
            for(size_t c=0;c<table.columns.size();++c) {
                auto scalar=data->column(static_cast<int>(c))->GetScalar(r);
                ensure(scalar.ok(),5,"Segelmodell-Parquet-Wert kann nicht gelesen werden: "+scalar.status().ToString());
                const auto& column=table.columns[c];
                if(column.integer) row[column.name]=static_cast<const arrow::Int32Scalar&>(**scalar).value;
                else {
                    double value=static_cast<const arrow::DoubleScalar&>(**scalar).value;
                    ensure(std::isfinite(value),5,std::string("Nicht endlicher Segelmodell-Wert: ")+column.name);
                    row[column.name]=value;
                }
            }
            rows.push_back(std::move(row));
        }
        model_data[table.rows]=std::move(rows);
    }
    segel::detail::validate_model_data_json(model_data.dump());
    return model_data;
}
namespace {
const J& model_conventions() {
    static const J value={{"ursprung","segelhals"},{"x_positiv","bug"},{"y_positiv","backbord"},
        {"z_positiv","oben"},{"system","segelfest_rechtshaendig"},{"stroemungsvektor","richtung_der_luftstroemung"},
        {"laenge","m"},{"geschwindigkeit","m/s"},{"kraft","N"},{"moment","Nm"}};
    return value;
}
const J& model_regularization() {
    static const J value={{"azimut_abblendung_grad",{120,180}},{"horizontal_schwelle_relativ",0.01}};
    return value;
}
void check_range(const J& value,const char* name) {
    ensure(value.is_array()&&value.size()==2&&value[0].is_number()&&value[1].is_number(),2,
           std::string(name)+": Bereich aus zwei Zahlen erforderlich");
    double a=value[0].get<double>(),b=value[1].get<double>();
    ensure(std::isfinite(a)&&std::isfinite(b)&&a>0&&b>a,2,std::string(name)+": ungueltiger positiver Bereich");
}
void check_sha256(const J& value) {
    ensure(value.is_string(),2,"SHA256 muss eine Zeichenkette sein");
    auto text=value.get<std::string>();
    ensure(text.size()==64&&std::all_of(text.begin(),text.end(),[](char c) {
        return (c>='0'&&c<='9')||(c>='a'&&c<='f');
    }),2,"SHA256 muss 64 kleine Hexadezimalzeichen enthalten");
}
void check_manifest_fields(const J& manifest) {
    fields(manifest,{"formatkennung","formatversion","modell_version","segel_id","geometrie","konventionen",
                     "gueltigkeit","parameter","tabellen","eingaben","erzeuger","numerik","nachweise"},"manifest");
    ensure(manifest.at("formatkennung")=="segelmodell"&&manifest.at("formatversion")=="1"&&
           manifest.at("modell_version")=="H1-lokal-1",2,"Unbekannte Segelmodell-Version");
    ensure(manifest.at("segel_id").is_string()&&!manifest.at("segel_id").get<std::string>().empty(),
           2,"Segelmodell-ID fehlt");
    const auto& geometry=manifest.at("geometrie");
    fields(geometry,{"S_m2","c_ref_m","z_min_m","hoehe_m","streifenanzahl","modenzahlen"},"manifest.geometrie");
    double area=numeric(geometry,"S_m2"),chord=numeric(geometry,"c_ref_m"),height=numeric(geometry,"hoehe_m");
    numeric(geometry,"z_min_m");
    ensure(area>0&&chord>0&&height>0&&std::abs(chord-area/height)<=1e-10*std::max(1.0,chord)&&
           geometry.at("streifenanzahl").is_number_integer()&&geometry.at("streifenanzahl").get<int64_t>()>0&&
           geometry.at("streifenanzahl").get<int64_t>()<=10000&&geometry.at("modenzahlen")==J::array({32,48,64}),
           2,"Ungueltige Segelmodell-Geometrie im Manifest");
    ensure(std::ceil(height/segel::detail::max_output_strip_height_m)==geometry.at("streifenanzahl").get<double>(),
           2,"Segelmodell-Streifenanzahl entspricht nicht ceil(H/0.25 m)");
    ensure(manifest.at("konventionen")==model_conventions(),2,"Nicht unterstuetzte Segelmodell-Konventionen");
    const auto& validity=manifest.at("gueltigkeit");
    fields(validity,{"geschwindigkeit_m_s","dichte_kg_m3","viskositaet_m2_s"},"manifest.gueltigkeit");
    ensure(validity.at("geschwindigkeit_m_s")==J::array({0,50}),2,"Ungueltiger Geschwindigkeitsbereich");
    check_range(validity.at("dichte_kg_m3"),"dichte_kg_m3");
    check_range(validity.at("viskositaet_m2_s"),"viskositaet_m2_s");
    const auto& parameters=manifest.at("parameter");
    ensure(parameters.is_object()&&parameters.at("regularisierung")==model_regularization(),2,
           "Unbekannte Segelmodell-Regularisierung");
    const auto& inputs=manifest.at("eingaben");
    ensure(inputs.is_object()&&inputs.contains("auftrag")&&inputs.contains("geometrie"),2,"Eingabenachweise fehlen");
    for(auto it=inputs.begin();it!=inputs.end();++it) {
        fields(it.value(),{"pfad","sha256","schema_version"},"manifest.eingabe");
        ensure(it.value().at("pfad").is_string()&&!it.value().at("pfad").get<std::string>().empty()&&
               it.value().at("schema_version").is_string()&&!it.value().at("schema_version").get<std::string>().empty(),
               2,"Ungueltiger Eingabenachweis");
        check_sha256(it.value().at("sha256"));
    }
    const auto& producer=manifest.at("erzeuger");
    fields(producer,{"werkzeug","arrow","parquet","zstd"},"manifest.erzeuger");
    for(const char* key:{"werkzeug","arrow","parquet","zstd"})
        ensure(producer.at(key).is_string()&&!producer.at(key).get<std::string>().empty(),2,"Erzeugerversion fehlt");
    ensure(manifest.at("numerik").is_object()&&!manifest.at("numerik").empty(),2,"Numerikbeschreibung fehlt");
    const auto& evidence=manifest.at("nachweise");
    fields(evidence,{"ausgefuehrt","bestanden","fehlgeschlagen","nicht_ausgefuehrt","physikalisch_validiert"},"manifest.nachweise");
    ensure(evidence.at("physikalisch_validiert")==false,2,"Physikalische Validierung nicht nachgewiesen");
    std::map<std::string,std::set<std::string>> states;
    for(const char* key:{"ausgefuehrt","bestanden","fehlgeschlagen","nicht_ausgefuehrt"}) {
        const auto& tests=evidence.at(key);
        ensure(tests.is_array(),2,"Nachweisstatus muss ein Array sein");
        for(const auto& test:tests) {
            ensure(test.is_string()&&!test.get<std::string>().empty(),2,"Ungueltiger Nachweisstatus");
            ensure(states[key].insert(test.get<std::string>()).second,2,"Doppelter Nachweisstatus");
        }
    }
    for(const auto& test:states["bestanden"])
        ensure(states["ausgefuehrt"].count(test)&&!states["fehlgeschlagen"].count(test),2,"Widerspruechlicher Nachweisstatus");
    for(const auto& test:states["fehlgeschlagen"])
        ensure(states["ausgefuehrt"].count(test),2,"Fehlgeschlagener Nachweis wurde nicht ausgefuehrt");
    for(const auto& test:states["nicht_ausgefuehrt"])
        ensure(!states["ausgefuehrt"].count(test),2,"Nicht ausgefuehrter Nachweis ist als ausgefuehrt markiert");
}
J load_manifest_model(const fs::path& stage,const J& manifest) {
    check_manifest_fields(manifest);
    auto geometry=manifest.at("geometrie");
    auto parameters=manifest.at("parameter");
    parameters.erase("regularisierung");
    J header={{"segel_id",manifest.at("segel_id")},{"S_m2",geometry.at("S_m2")},
              {"c_ref_m",geometry.at("c_ref_m")},{"z_min_m",geometry.at("z_min_m")},
              {"hoehe_m",geometry.at("hoehe_m")},{"parameter",parameters}};
    J model_data=read_model_tables(stage,manifest.at("tabellen"),std::move(header));
    ensure(model_data.at("streifen").size()==geometry.at("streifenanzahl").get<size_t>(),5,
           "Streifenanzahl stimmt nicht mit Manifest ueberein");
    validate_model_validity(model_data,manifest.at("gueltigkeit"));
    return model_data;
}
void ensure_model_roundtrip(const J& expected,const J& actual) {
    static_assert(sizeof(double)==8&&std::numeric_limits<double>::is_iec559,
                  "Segelmodell erfordert IEEE-754 binary64");
    ensure(actual==expected,5,"Segelmodell-Roundtrip stimmt nicht ueberein");
    for(const auto& table:model_tables()) {
        const auto& original_rows=expected.at(table.rows);
        const auto& restored_rows=actual.at(table.rows);
        for(size_t row=0;row<original_rows.size();++row) {
            for(const auto& column:table.columns) {
                if(column.integer)continue;
                const double original=original_rows.at(row).at(column.name).get<double>();
                const double restored=restored_rows.at(row).at(column.name).get<double>();
                ensure(std::memcmp(&original,&restored,sizeof(double))==0,5,
                       std::string("Segelmodell-Roundtrip nicht bitgleich: ")+table.file+
                       ", Zeile "+std::to_string(row)+", Spalte "+column.name);
            }
        }
    }
}
}
void validate_model_validity(const J& model_data,const J& validity) {
    fields(validity,{"geschwindigkeit_m_s","dichte_kg_m3","viskositaet_m2_s"},"Segelmodell-Gueltigkeit");
    ensure(validity.at("geschwindigkeit_m_s")==J::array({0,50}),2,"Ungueltiger Geschwindigkeitsbereich");
    check_range(validity.at("dichte_kg_m3"),"dichte_kg_m3");
    check_range(validity.at("viskositaet_m2_s"),"viskositaet_m2_s");
    segel::detail::validate_model_data_json(model_data.dump());
    double min_nu=validity.at("viskositaet_m2_s")[0].get<double>();
    double max_re=numeric(model_data.at("parameter"),"re_max");
    for(const char* name:{"quadratur","kollokation"})for(const auto& row:model_data.at(name)) {
        double re=50*numeric(row,"chord_m")/min_nu;
        ensure(std::isfinite(re)&&re<=max_re,6,"Reynolds-Worst-Case liegt ausserhalb des Modellbereichs");
    }
}
J write_model_manifest(const fs::path& stage,const J& model_data,const J& metadata,const J& provenance) {
    fields(provenance,{"gueltigkeit","eingaben","erzeuger","numerik","nachweise"},"Segelmodell-Provenienz");
    ensure(!fs::exists(stage/"manifest.json"),5,"Segelmodell-Manifest existiert bereits");
    J params=model_data.at("parameter");
    params["regularisierung"]=model_regularization();
    J manifest={{"formatkennung","segelmodell"},{"formatversion","1"},{"modell_version","H1-lokal-1"},
        {"segel_id",model_data.at("segel_id")},
        {"geometrie",{{"S_m2",model_data.at("S_m2")},{"c_ref_m",model_data.at("c_ref_m")},
                       {"z_min_m",model_data.at("z_min_m")},{"hoehe_m",model_data.at("hoehe_m")},
                       {"streifenanzahl",model_data.at("streifen").size()},{"modenzahlen",{32,48,64}}}},
        {"konventionen",model_conventions()},{"gueltigkeit",provenance.at("gueltigkeit")},{"parameter",params},
        {"tabellen",metadata},{"eingaben",provenance.at("eingaben")},{"erzeuger",provenance.at("erzeuger")},
        {"numerik",provenance.at("numerik")},{"nachweise",provenance.at("nachweise")}};
    ensure_model_roundtrip(model_data,load_manifest_model(stage,manifest));
    write(stage/"manifest.json",manifest);
    return manifest;
}
J read_model_manifest(const fs::path& stage) {
    J manifest=read_json(stage/"manifest.json",5);
    return {{"manifest",manifest},{"modell",load_manifest_model(stage,manifest)}};
}
namespace {
struct ModelStagePaths {
    fs::path stage;
    fs::path output;
    std::string token;
};
ModelStagePaths model_stage_paths(const fs::path& stage_path,const fs::path& output_path) {
    auto stage=fs::absolute(stage_path).lexically_normal();
    auto output=fs::absolute(output_path).lexically_normal();
    const std::string prefix="Segelmodell.arbeiten.";
    const auto name=stage.filename().u8string();
    ensure(stage.parent_path()==output.parent_path()&&output.filename()=="Segelmodell"&&
           name.size()>prefix.size()&&name.compare(0,prefix.size(),prefix)==0,5,
           "Ungueltiger Segelmodell-Arbeitsordner");
    const auto token=name.substr(prefix.size());
    ensure(std::all_of(token.begin(),token.end(),[](char c) {
        return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_';
    }),5,"Ungueltige Arbeitsordnerkennung");
    return {std::move(stage),std::move(output),token};
}
}
J create_model_stage(const fs::path& stage_path,const fs::path& output_path,
                     const J& model,const J& provenance,const std::function<void()>& check_cancel,
                     int workers) {
    ensure(static_cast<bool>(check_cancel),4,"Segelmodell-Arbeitsordner erfordert eine Abbruchpruefung");
    ensure(provenance.is_object()&&provenance.contains("eingaben")&&provenance.at("eingaben").is_object()&&
           provenance.at("eingaben").contains("konventionen"),2,
           "Segelmodell-Eingabenachweis fuer Konventionen fehlt");
    const auto paths=model_stage_paths(stage_path,output_path);
    std::error_code ec;
    bool created=fs::create_directory(paths.stage,ec);
    ensure(created&&!ec,5,"Segelmodell-Arbeitsordner kann nicht exklusiv angelegt werden: "+
           paths.stage.u8string()+"; Systemfehler "+std::to_string(ec.value()));
    try {
        auto metadata=write_model_tables(paths.stage,model,check_cancel,workers);
        if(check_cancel)check_cancel();
        auto manifest=write_model_manifest(paths.stage,model,metadata,provenance);
        if(check_cancel)check_cancel();
        ensure_model_roundtrip(model,read_model_manifest(paths.stage).at("modell"));
        if(check_cancel)check_cancel();
        return manifest;
    } catch(const std::exception& e) {
        const auto failure=std::current_exception();
        fs::remove_all(paths.stage,ec);
        ensure(!ec,5,"Segelmodell-Arbeitsordner kann nach Fehler nicht entfernt werden: "+
               paths.stage.u8string()+"; Systemfehler "+std::to_string(ec.value())+"; urspruenglicher Fehler: "+e.what());
        std::rethrow_exception(failure);
    }
}
bool publish_model_stage(const fs::path& stage_path,const fs::path& output_path,
                         bool overwrite,bool& published) {
    published=false;
    const auto paths=model_stage_paths(stage_path,output_path);
    const auto& stage=paths.stage;
    const auto& output=paths.output;
    const auto& token=paths.token;
    ensure(!fs::is_symlink(stage)&&fs::is_directory(stage),5,"Segelmodell-Arbeitsordner fehlt oder ist kein regulaerer Ordner");
    std::set<std::string> expected={"manifest.json","streifen.parquet","quadratur.parquet",
                                    "flaechenpunkte.parquet","kollokation.parquet","kopplung.parquet"};
    for(const auto& item:fs::directory_iterator(stage))
        ensure(!item.is_symlink()&&item.is_regular_file()&&expected.erase(item.path().filename().u8string())==1,
               5,"Ungueltige Datei im Segelmodell-Arbeitsordner: "+item.path().u8string());
    ensure(expected.empty(),5,"Segelmodell-Arbeitsordner ist unvollstaendig");
    const auto manifest=read_model_manifest(stage).at("manifest");
    ensure(manifest.at("eingaben").contains("konventionen"),4,
           "Segelmodell-Eingabenachweis fuer Konventionen fehlt");
    const auto evidence=manifest.at("nachweise");
    const std::set<std::string> project_evidence={"vollstaendige_numerikabnahme","datenformat_und_integritaet",
                                                   "end_to_end_integration","geschuetzte_publikation"};
    ensure(!evidence.at("ausgefuehrt").empty()&&
           evidence.at("ausgefuehrt").size()==evidence.at("bestanden").size()&&
           evidence.at("fehlgeschlagen").empty()&&
           std::all_of(evidence.at("nicht_ausgefuehrt").begin(),evidence.at("nicht_ausgefuehrt").end(),
                       [&](const J& item) { return project_evidence.count(item.get<std::string>())!=0; }),
           4,"Segelmodell-Nachweise unvollstaendig");
    for(const char* required:{"rastervergleich_vorpruefung","nullwindfolge","gleichwind_vorpruefung",
                              "h1_referenzvergleich_vorpruefung","tabellen_roundtrip_vorpruefung",
                              "modell_lade_und_rechenprobe"}) {
        const auto& passed=evidence.at("bestanden");
        ensure(std::find(passed.begin(),passed.end(),required)!=passed.end(),4,
               std::string("Segelmodell-Pflichtnachweis fehlt: ")+required);
    }
    const auto backup=output.parent_path()/fs::u8path("Segelmodell.ersetzen."+token);
    ensure(!fs::exists(backup)&&!fs::is_symlink(backup),5,
           "Segelmodell-Austauschordner existiert bereits: "+backup.u8string());
    const bool replacing=fs::exists(output)||fs::is_symlink(output);
    if(replacing) {
        ensure(!fs::is_symlink(output)&&fs::is_directory(output),5,"Segelmodell muss ein regulaerer Ordner sein");
        ensure(overwrite,5,"Segelmodell existiert bereits");
    }
#ifdef _WIN32
    auto move_no_replace=[](const fs::path& from,const fs::path& to) {
        if(MoveFileExW(from.c_str(),to.c_str(),0))return std::error_code{};
        return std::error_code(static_cast<int>(GetLastError()),std::system_category());
    };
    if(replacing) {
        auto ec=move_no_replace(output,backup);
        ensure(!ec,5,"Bisheriges Segelmodell kann nicht zum Austausch verschoben werden: "+
               output.u8string()+"; Systemfehler "+std::to_string(ec.value()));
    }
    auto ec=move_no_replace(stage,output);
    if(ec) {
        const auto error=ec.value();
        if(replacing) {
            auto rollback=move_no_replace(backup,output);
            ensure(!rollback,5,"Segelmodell-Veroeffentlichung und Wiederherstellung fehlgeschlagen; bisheriges Modell: "+
                   backup.u8string()+"; Systemfehler "+std::to_string(rollback.value()));
        }
        throw Error(5,"Segelmodell-Veroeffentlichung fehlgeschlagen; Systemfehler "+std::to_string(error));
    }
    published=true;
    if(replacing) {
        fs::remove_all(backup,ec);
        ensure(!ec,5,"Neues Segelmodell geschrieben, alter Austauschordner kann nicht entfernt werden: "+
               backup.u8string()+"; Systemfehler "+std::to_string(ec.value()));
    }
    return replacing;
#else
    throw Error(3,"Segelmodell-Austausch derzeit nur unter Windows unterstuetzt");
#endif
}
}
