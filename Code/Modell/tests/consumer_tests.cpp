#include "segel/modell.hpp"
#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>

int main() {
    const auto name="segelmodell_nicht_vorhanden_"+
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const auto path=std::filesystem::temp_directory_path()/name;
    try {
        segel::modell::laden(path);
        std::cerr<<"Fehlendes Modell wurde angenommen\n";
        return 1;
    } catch(const segel::modell::Fehler& e) {
        if(e.art()==segel::modell::Fehlerart::integritaet&&!std::string(e.what()).empty())return 0;
        std::cerr<<"Falscher Fehler beim Laden: "<<e.what()<<'\n';
        return 1;
    }
}
