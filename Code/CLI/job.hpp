#pragma once
#include <atomic>
#include <filesystem>
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
J read(const std::filesystem::path& path);
std::string hash(const std::filesystem::path& path);
J run(const std::filesystem::path& path,bool check,int limit);
J dispatch(const std::vector<std::string>& args);
}
