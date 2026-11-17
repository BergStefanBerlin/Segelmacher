#pragma once
#include "job.hpp"
#include <arrow/api.h>
#include <arrow/io/api.h>
#include <parquet/arrow/writer.h>
#include <parquet/properties.h>
#include <cmath>
#include <fstream>
#include <set>

namespace cli::detail {
namespace fs=std::filesystem;
inline void ensure(bool condition,int code,const std::string& text) {
    if(!condition)throw Error(code,text);
}
inline void fields(const J& value,std::initializer_list<const char*> names,const std::string& where) {
    ensure(value.is_object(),2,where+": Objekt erforderlich");
    std::set<std::string> allowed;
    for(auto name:names)allowed.insert(name);
    for(auto it=value.begin();it!=value.end();++it)
        ensure(allowed.count(it.key())!=0,2,where+"."+it.key()+": unbekanntes Feld");
}
inline double numeric(const J& value,const std::string& key) {
    ensure(value.contains(key)&&value.at(key).is_number(),2,key+": endliche Zahl erforderlich");
    double result=value.at(key).get<double>();
    ensure(std::isfinite(result),2,key+": nicht endlich");
    return result;
}
inline void write(const fs::path& path,const J& value) {
    std::ofstream out(path,std::ios::binary);
    out.exceptions(std::ios::badbit|std::ios::failbit);
    out<<value.dump(2)<<'\n';
    out.close();
}
inline std::shared_ptr<arrow::Scalar> number_scalar(double value) {
    ensure(std::isfinite(value),4,"Parquet-Ausgabe enthaelt einen nicht-endlichen Zahlenwert");
    return std::make_shared<arrow::DoubleScalar>(value);
}
constexpr int64_t parquet_row_group_rows=16384;
class ParquetTableWriter {
public:
    ParquetTableWriter(const fs::path& path,std::shared_ptr<arrow::Schema> schema)
        :schema_(std::move(schema)) {
        auto output=arrow::io::FileOutputStream::Open(path.u8string());
        ensure(output.ok(),5,"Parquet-Datei kann nicht geoeffnet werden: "+path.u8string()+": "+output.status().ToString());
        sink_=*output;
        auto properties=parquet::WriterProperties::Builder()
            .compression(parquet::Compression::ZSTD)
            ->max_row_group_length(parquet_row_group_rows)
            ->build();
        auto arrow_properties=parquet::ArrowWriterProperties::Builder()
            .store_schema()->set_use_threads(false)->build();
        auto maybe_writer=parquet::arrow::FileWriter::Open(*schema_,arrow::default_memory_pool(),sink_,
            properties,arrow_properties);
        ensure(maybe_writer.ok(),5,"Parquet-Writer kann nicht gestartet werden: "+maybe_writer.status().ToString());
        writer_=std::move(maybe_writer).ValueOrDie();
        for(const auto& field:schema_->fields()) {
            std::unique_ptr<arrow::ArrayBuilder> builder;
            auto status=arrow::MakeBuilder(arrow::default_memory_pool(),field->type(),&builder);
            ensure(status.ok(),5,"Parquet-Spaltenpuffer kann nicht angelegt werden: "+status.ToString());
            builders_.push_back(std::move(builder));
        }
    }
    void append(std::vector<std::shared_ptr<arrow::Scalar>> values) {
        ensure(values.size()==builders_.size(),5,"Interner Parquet-Zeilenschemafehler");
        for(size_t i=0;i<values.size();++i) {
            auto status=builders_[i]->AppendScalar(*values[i]);
            ensure(status.ok(),5,"Parquet-Zeile kann nicht gepuffert werden: "+status.ToString());
        }
        ++pending_rows_;
        ++row_count_;
        if(pending_rows_==parquet_row_group_rows)flush();
    }
    void close() {
        flush();
        auto status=writer_->Close();
        ensure(status.ok(),5,"Parquet-Datei kann nicht abgeschlossen werden: "+status.ToString());
        writer_.reset();
        status=sink_->Close();
        ensure(status.ok(),5,"Parquet-Datei kann nicht geschlossen werden: "+status.ToString());
    }
    int64_t row_count() const { return row_count_; }
    int64_t row_group_count() const { return row_group_count_; }
private:
    void flush() {
        if(!pending_rows_)return;
        std::vector<std::shared_ptr<arrow::Array>> arrays;
        arrays.reserve(builders_.size());
        for(auto& builder:builders_) {
            std::shared_ptr<arrow::Array> array;
            auto status=builder->Finish(&array);
            ensure(status.ok(),5,"Parquet-Spaltenpuffer kann nicht abgeschlossen werden: "+status.ToString());
            arrays.push_back(std::move(array));
        }
        auto table=arrow::Table::Make(schema_,std::move(arrays));
        auto status=writer_->WriteTable(*table,pending_rows_);
        ensure(status.ok(),5,"Parquet-Row-Group kann nicht geschrieben werden: "+status.ToString());
        ++row_group_count_;
        pending_rows_=0;
    }
    std::shared_ptr<arrow::Schema> schema_;
    std::shared_ptr<arrow::io::FileOutputStream> sink_;
    std::unique_ptr<parquet::arrow::FileWriter> writer_;
    std::vector<std::unique_ptr<arrow::ArrayBuilder>> builders_;
    int64_t pending_rows_=0,row_count_=0,row_group_count_=0;
};
}
