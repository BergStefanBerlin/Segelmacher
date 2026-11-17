#include "segel/modell.hpp"
#include "../../CLI/job.hpp"
#include "../../Core/src/segelform_internal.hpp"
#include <nlohmann/json.hpp>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <system_error>

namespace segel::modell {
using J=nlohmann::json;
namespace {
std::string range_error(const char* name,double value,double low,double high) {
    std::ostringstream message;
    message<<std::setprecision(17)<<name<<" "<<value<<" ausserhalb ["<<low<<", "<<high<<"]";
    return message.str();
}
}
Fehler::Fehler(Fehlerart art,const std::string& kontext):std::runtime_error(kontext),art_(art) {}
Fehlerart Fehler::art() const noexcept { return art_; }
struct Modell::Impl {
    std::vector<Punkt> punkte;
    double rho_min,rho_max,nu_min,nu_max;
    detail::PreparedModel prepared;
    explicit Impl(const J& loaded)
        :rho_min(loaded.at("manifest").at("gueltigkeit").at("dichte_kg_m3")[0]),
         rho_max(loaded.at("manifest").at("gueltigkeit").at("dichte_kg_m3")[1]),
         nu_min(loaded.at("manifest").at("gueltigkeit").at("viskositaet_m2_s")[0]),
         nu_max(loaded.at("manifest").at("gueltigkeit").at("viskositaet_m2_s")[1]),
         prepared(loaded.at("modell").dump()) {
        for(const auto& row:loaded.at("modell").at("streifen"))
            punkte.push_back({row.at("streifen_id").get<StreifenId>(),
                {row.at("abfrage_x_m"),row.at("abfrage_y_m"),row.at("abfrage_z_m")}});
    }
};
Modell::Modell(std::unique_ptr<Impl> impl):impl_(std::move(impl)) {}
Modell::~Modell()=default;
std::vector<Punkt> Modell::punkte() const { return impl_->punkte; }
std::vector<Last> Modell::berechnen(const std::vector<Wind>& winde,double rho,double nu) const {
    if(!std::isfinite(rho)||!std::isfinite(nu))
        throw Fehler(Fehlerart::eingabe,"Luftdichte und Viskositaet muessen endlich sein");
    if(rho<impl_->rho_min||rho>impl_->rho_max)
        throw Fehler(Fehlerart::gueltigkeit,range_error("Luftdichte kg/m3",rho,impl_->rho_min,impl_->rho_max));
    if(nu<impl_->nu_min||nu>impl_->nu_max)
        throw Fehler(Fehlerart::gueltigkeit,range_error("Viskositaet m2/s",nu,impl_->nu_min,impl_->nu_max));
    if(winde.size()!=impl_->punkte.size())
        throw Fehler(Fehlerart::eingabe,"Anzahl der Windvektoren "+std::to_string(winde.size())+
            " stimmt nicht mit Streifenzahl "+std::to_string(impl_->punkte.size())+" ueberein");
    std::vector<bool> seen(impl_->punkte.size(),false);
    J values=J::array();
    for(const auto& wind:winde) {
        if(wind.id>=seen.size()||seen[wind.id])
            throw Fehler(Fehlerart::eingabe,"Unbekannte oder doppelte Streifen-ID: "+std::to_string(wind.id));
        seen[wind.id]=true;
        const auto& u=wind.stroemung_m_s;
        if(!std::isfinite(u[0])||!std::isfinite(u[1])||!std::isfinite(u[2]))
            throw Fehler(Fehlerart::eingabe,"Nicht endlicher Wind an Streifen "+std::to_string(wind.id));
        double velocity=std::hypot(std::hypot(u[0],u[1]),u[2]);
        if(!std::isfinite(velocity)||velocity>50)
            throw Fehler(Fehlerart::gueltigkeit,"Windgeschwindigkeit ausserhalb 0..50 m/s an Streifen "+
                std::to_string(wind.id)+": "+std::to_string(velocity));
        values.push_back({{"streifen_id",wind.id},{"wind_m_s",u}});
    }
    try {
        auto result=J::parse(impl_->prepared.solve(values.dump(),rho,nu));
        std::vector<Last> loads;
        loads.reserve(result.size());
        for(const auto& row:result)
            loads.push_back({row.at("streifen_id").get<StreifenId>(),row.at("kraft_N").get<Vektor>(),
                             row.at("moment_Nm").get<Vektor>()});
        return loads;
    } catch(const std::invalid_argument& e) {
        throw Fehler(Fehlerart::numerik,std::string("Segelmodell-Berechnung: ")+e.what());
    } catch(const J::exception& e) {
        throw Fehler(Fehlerart::numerik,std::string("Segelmodell-Ergebnis: ")+e.what());
    }
}
std::unique_ptr<const Modell> laden(const std::filesystem::path& ordner) {
    std::error_code ec;
    if(!std::filesystem::is_regular_file(ordner/"manifest.json",ec))
        throw Fehler(Fehlerart::integritaet,"Segelmodell-Manifest nicht lesbar: "+ordner.u8string()+
            (ec?": "+ec.message():""));
    try {
        J loaded=cli::read_model_manifest(ordner);
        auto impl=std::make_unique<Modell::Impl>(loaded);
        return std::unique_ptr<const Modell>(new Modell(std::move(impl)));
    } catch(const cli::Error& e) {
        auto art=e.code==5?Fehlerart::integritaet:e.code==6?Fehlerart::gueltigkeit:Fehlerart::modell;
        throw Fehler(art,std::string("Segelmodell ")+ordner.u8string()+": "+e.what());
    } catch(const std::invalid_argument& e) {
        throw Fehler(Fehlerart::modell,std::string("Segelmodell ")+ordner.u8string()+": "+e.what());
    } catch(const J::exception& e) {
        throw Fehler(Fehlerart::modell,std::string("Segelmodell ")+ordner.u8string()+": "+e.what());
    } catch(const std::filesystem::filesystem_error& e) {
        throw Fehler(Fehlerart::integritaet,std::string("Segelmodell ")+ordner.u8string()+": "+e.what());
    }
}
}
