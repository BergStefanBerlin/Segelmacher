#pragma once
#include <memory>
#include <string>
#include <string_view>

namespace segel {
// CPPI-0001; contract: Dokumentationen/Schnittstellenregister_CPP.md
class Context {
public:
    explicit Context(std::string_view input); // CPPI-0002
    ~Context(); // CPPI-0003
    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;
    std::string geometry() const; // CPPI-0004
    std::string solve(double alpha_deg, double beta_deg, double re_ref,
                      int factor, bool loads = true, int lifting_modes = 48) const; // CPPI-0005
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
std::string polar(std::string_view input); // CPPI-0006
std::string resultant(std::string_view input); // CPPI-0007
std::string transform(std::string_view input); // CPPI-0008
}
