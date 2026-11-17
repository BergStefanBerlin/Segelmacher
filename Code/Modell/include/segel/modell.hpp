#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace segel::modell {
using Vektor = std::array<double,3>; // CPPI-0009
using StreifenId = std::uint32_t; // CPPI-0010
struct Punkt { StreifenId id; Vektor position_m; }; // CPPI-0011
struct Wind { StreifenId id; Vektor stroemung_m_s; }; // CPPI-0012
struct Last { StreifenId id; Vektor kraft_N; Vektor moment_Nm; }; // CPPI-0013
struct Transformation { std::array<Vektor,3> rotation; Vektor translation_m; }; // CPPI-0023
struct SdpSummen {
    std::string segel_id;
    double Q_N;
    double B_N;
    double X_Nm;
    double Z_Nm;
}; // CPPI-0024
struct Berechnung { std::vector<Last> streifenlasten; SdpSummen sdp; }; // CPPI-0025
enum class Fehlerart { eingabe, modell, integritaet, gueltigkeit, numerik }; // CPPI-0014
class Fehler : public std::runtime_error { // CPPI-0015
public:
    Fehler(Fehlerart art,const std::string& kontext); // CPPI-0016
    Fehlerart art() const noexcept; // CPPI-0017
private:
    Fehlerart art_;
};
class Modell { // CPPI-0018
public:
    ~Modell(); // CPPI-0019
    Modell(const Modell&) = delete;
    Modell& operator=(const Modell&) = delete;
    std::vector<Punkt> punkte() const; // CPPI-0020; neue Modelle: ceil(H / 0.25 m) Punkte
    std::vector<Last> berechnen(const std::vector<Wind>& winde,double dichte_kg_m3,
                                double viskositaet_m2_s) const; // CPPI-0021
    Berechnung berechnen_mit_sdp(const std::vector<Wind>& winde,double dichte_kg_m3,
                                 double viskositaet_m2_s,const Transformation& transformation) const; // CPPI-0026
private:
    struct Impl;
    explicit Modell(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
    friend std::unique_ptr<const Modell> laden(const std::filesystem::path&);
};
std::unique_ptr<const Modell> laden(const std::filesystem::path& ordner); // CPPI-0022
}
