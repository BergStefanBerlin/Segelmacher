#include <arrow/api.h>
#include <arrow/io/api.h>
#include <parquet/arrow/reader.h>
#include <array>
#include <filesystem>
#include <iostream>
#include <string>
#include <utility>

namespace {
bool check_table(const std::filesystem::path& directory,const char* filename,
                 const std::array<const char*,3>& expected) {
    const std::string path=(directory/filename).u8string();
    auto maybe_input=arrow::io::ReadableFile::Open(path);
    if(!maybe_input.ok()) {
        std::cerr<<filename<<": "<<maybe_input.status().ToString()<<'\n';
        return false;
    }
    std::shared_ptr<arrow::io::ReadableFile> input=*maybe_input;
    auto maybe_reader=parquet::arrow::OpenFile(input,arrow::default_memory_pool());
    if(!maybe_reader.ok()) {
        std::cerr<<maybe_reader.status().ToString()<<'\n';
        return false;
    }
    auto reader=std::move(maybe_reader).ValueOrDie();
    if(reader->num_row_groups()<1) {
        std::cerr<<filename<<": no row groups\n";
        return false;
    }
    std::shared_ptr<arrow::Schema> schema;
    auto status=reader->GetSchema(&schema);
    if(!status.ok()) {
        std::cerr<<filename<<": "<<status.ToString()<<'\n';
        return false;
    }
    if(schema->num_fields()<static_cast<int>(expected.size())) {
        std::cerr<<filename<<": too few columns\n";
        return false;
    }
    for(size_t i=0;i<expected.size();++i)
        if(schema->field(static_cast<int>(i))->name()!=expected[i]) {
            std::cerr<<filename<<": unexpected column "<<i<<'\n';
            return false;
        }
    std::shared_ptr<arrow::Table> projected;
    status=reader->ReadRowGroup(0,{0,1,2},&projected);
    if(!status.ok()) {
        std::cerr<<filename<<": "<<status.ToString()<<'\n';
        return false;
    }
    if(projected->num_columns()!=3||projected->num_rows()<1) {
        std::cerr<<filename<<": projected read returned an empty table\n";
        return false;
    }
    return true;
}
int run(const std::filesystem::path& directory) {
    const auto directory_utf8=directory.u8string();
    const std::string directory_text(directory_utf8.begin(),directory_utf8.end());
    if(!check_table(directory,"summen.parquet",{"form_id","alpha_deg","windneigung_deg"})||
       !check_table(directory,"kraefte.parquet",{"form_id","alpha_deg","windneigung_deg"})||
       !check_table(directory,"profile.parquet",{"profil_id","alpha_deg","re"})) {
        std::cerr<<"Parquet schema or projected C++ read failed in "<<directory_text<<'\n';
        return 1;
    }
    std::cout<<"C++ Parquet column-projection reads passed\n";
    return 0;
}
}

#ifdef _WIN32
int wmain(int argc,wchar_t** argv) {
    if(argc!=2) {
        std::cerr<<"usage: parquet_reader <lookup-directory>\n";
        return 2;
    }
    return run(std::filesystem::path(argv[1]));
}
#else
int main(int argc,char** argv) {
    if(argc!=2) {
        std::cerr<<"usage: parquet_reader <lookup-directory>\n";
        return 2;
    }
    return run(std::filesystem::u8path(argv[1]));
}
#endif
