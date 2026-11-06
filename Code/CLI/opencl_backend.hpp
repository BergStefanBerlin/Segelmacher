#pragma once
#include <memory>
#include <string>
#include <vector>

namespace cli {
struct PolarBatchParameters {
    double stall_start_deg;
    double stall_end_deg;
    double stall_re_shift_deg;
    double transition_re;
    double transition_width;
    double viscous_cl2;
    double normal_pressure;
};
struct PolarBatchInput {
    double a0;
    double a1;
    double a2;
    double alpha_deg;
    double re;
};
struct PolarBatchOutput {
    double cl;
    double cd;
    double cm;
};

class OpenClPolarBackend {
public:
    static std::unique_ptr<OpenClPolarBackend> create(std::string& reason);
    ~OpenClPolarBackend();
    OpenClPolarBackend(const OpenClPolarBackend&) = delete;
    OpenClPolarBackend& operator=(const OpenClPolarBackend&) = delete;
    std::vector<PolarBatchOutput> evaluate(const std::vector<PolarBatchInput>& input,
                                           const PolarBatchParameters& parameters) const;
    const std::string& platform() const;
    const std::string& device() const;
private:
    struct Impl;
    explicit OpenClPolarBackend(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};

std::vector<PolarBatchOutput> evaluate_polar_cpu(const std::vector<PolarBatchInput>& input,
                                                const PolarBatchParameters& parameters);
}
