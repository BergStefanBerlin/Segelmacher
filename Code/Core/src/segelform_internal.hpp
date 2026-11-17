#pragma once
#include <nlohmann/json.hpp>
#include <array>
#include <memory>
#include <string>
#include <string_view>

namespace segel::detail {
inline constexpr double max_output_strip_height_m = 0.25;
class PreparedModel {
public:
    explicit PreparedModel(std::string_view model);
    ~PreparedModel();
    PreparedModel(const PreparedModel&) = delete;
    PreparedModel& operator=(const PreparedModel&) = delete;
    std::string solve(std::string_view winds, double rho, double nu) const;
    std::string solve_modes(std::string_view winds, double rho, double nu, int modes) const;
    std::string solve_with_contributions(std::string_view winds, double rho, double nu) const;
    std::string solve_with_sdp(std::string_view winds, double rho, double nu,
                              const std::array<std::array<double,3>,3>& rotation,
                              const std::array<double,3>& translation) const;
    nlohmann::json solve_with_lifting_diagnostics(std::string_view winds, double rho, double nu,
                                                 int modes = 48) const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
class SplitReference {
public:
    explicit SplitReference(std::string_view input);
    ~SplitReference();
    SplitReference(const SplitReference&) = delete;
    SplitReference& operator=(const SplitReference&) = delete;
    bool valid(double re) const;
    nlohmann::json lifting_modes(double alpha, double beta) const;
    nlohmann::json solve_data(double alpha, double beta, double re) const;
    std::string solve(double alpha, double beta, double re) const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
std::string prepare_strip_geometry(std::string_view input, int subdivision = 1);
std::string prepare_coupling_geometry(std::string_view input);
std::string prepare_model_data(std::string_view input, int subdivision = 1);
void validate_model_data_json(std::string_view model);
std::string solve_prepared_case(std::string_view model, std::string_view winds, double rho, double nu);
std::string solve_strip_case(std::string_view input, std::string_view winds, double rho, double nu, int subdivision = 1);
std::string h1_split_reference(std::string_view input, double alpha, double beta, double re);
}
