#pragma once
#include <atomic>
#include <filesystem>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
namespace cli {
using J=nlohmann::json;
extern std::atomic<bool> cancelled;
struct Error : std::runtime_error {
    int code;
    Error(int c,const std::string& message):std::runtime_error(message),code(c){}
};
struct HashedJson {
    J value;
    std::string sha256;
};
struct JsonSnapshot {
    J value;
    std::string bytes;
};
J read(const std::filesystem::path& path);
JsonSnapshot read_json_snapshot(const std::filesystem::path& path);
std::string hash_json_snapshot(const JsonSnapshot& snapshot,const std::filesystem::path& path);
HashedJson read_hashed_json(const std::filesystem::path& path);
std::string hash(const std::filesystem::path& path);
J parquet_library_provenance(const std::string& package_version,const std::string& package_sha256);
J zstd_library_provenance(const std::string& arrow_package_sha256,
                          const std::string& parquet_package_version,const std::string& parquet_package_sha256);
// Bei mehreren Workern muss die Abbruchpruefung nebenlaeufig aufrufbar sein.
J write_model_tables(const std::filesystem::path& stage,const J& model,
                     const std::function<void()>& check_cancel={},int workers=1);
J read_model_tables(const std::filesystem::path& stage,const J& metadata,J model_header);
void validate_model_validity(const J& model,const J& validity);
J write_model_manifest(const std::filesystem::path& stage,const J& model,const J& metadata,const J& provenance);
J read_model_manifest(const std::filesystem::path& stage);
J create_model_stage(const std::filesystem::path& stage,const std::filesystem::path& output,
                     const J& model,const J& provenance,const std::function<void()>& check_cancel,
                     int workers=1);
bool publish_model_stage(const std::filesystem::path& stage,const std::filesystem::path& output,
                         bool overwrite,bool& published);
J publish_model_result(const std::filesystem::path& stage,const std::filesystem::path& output,
                       bool overwrite,const std::function<void(J&)>& after_publish);
void update_model_fingerprint(J& result,const std::filesystem::path& model_root);
J run(const std::filesystem::path& path,bool check,int limit);
J dispatch(const std::vector<std::string>& args);
}
