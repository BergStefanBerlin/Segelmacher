#include "job.hpp"
#include "progress.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
int main() {
    using cli::J;
    try {
        int checks=0;
        auto check=[&](bool ok){++checks;if(!ok)throw std::runtime_error("CLI contract assertion "+std::to_string(checks));};
        check(cli::progress_line("Gesamt",5,6,"\x1b[94m","Export").find("5 / 6  | Export")!=std::string::npos);
        check(cli::progress_line("Paket ",999,49680,"\x1b[92m").find("999 / 49680")!=std::string::npos);
        check(cli::progress_line("Paket ",0,0,"\x1b[92m").find("0 / ?")!=std::string::npos);
        check(cli::progress_line("Paket ",8,6,"\x1b[92m").find("6 / 6")!=std::string::npos);
        check(cli::progress_time(233,233,5656)=="(00:03:53 / 01:34:16) | Verbleibende Zeit: 01:30:23");
        check(cli::progress_time(10,0,100)=="(00:00:10 / wird ermittelt) | Verbleibende Zeit: wird ermittelt");
        check(cli::progress_time(10,100,100)=="(00:00:10 / 00:00:10) | Verbleibende Zeit: 00:00:00");
        check(cli::progress_time(20,20,100)=="(00:00:20 / 00:01:40) | Verbleibende Zeit: 00:01:20");
        check(cli::progress_duration(59)=="00:00:59");
        check(cli::progress_duration(60)=="00:01:00");
        check(cli::progress_duration(3599)=="00:59:59");
        check(cli::progress_duration(3600)=="01:00:00");
        check(cli::progress_duration(5)=="00:00:05");
        check(cli::dispatch({"--hilfe"})["exitcode"]==0);
        check(cli::dispatch({"--version"})["exitcode"]==0);
        check(cli::dispatch({})["exitcode"]==2);
        check(cli::dispatch({"lookup","--auftrag","missing.json"})["exitcode"]==2);
        check(cli::dispatch({"lookup","--auftrag","a","--max-prozessorzahl","0"})["exitcode"]==2);
        check(cli::dispatch({"lookup","--auftrag","a","--max-prozessorzahl","2","--max-prozessorzahl","2"})["exitcode"]==2);
        check(cli::dispatch({"pruefen","--auftrag","a","--unknown","1"})["exitcode"]==2);
        std::filesystem::path p="cli_duplicate_test.json";
        {std::ofstream out(p);out<<"{\"a\":1,\"a\":2}";}
        bool rejected=false;try{cli::read(p);}catch(const cli::Error&){rejected=true;}
        std::filesystem::remove(p);check(rejected);
        p="cli_hash_test.txt";{std::ofstream out(p,std::ios::binary);out<<"abc";}
        check(cli::hash(p)=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
        std::filesystem::remove(p);
        const std::filesystem::path source=TEST_SOURCE_ROOT;
        const auto token=std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        const auto work=std::filesystem::current_path()/("cli_contract_"+token);
        std::filesystem::create_directory(work);
        std::filesystem::copy_file(source/"Backups"/"420_main"/"main_20261105_195154_358.json",work/"main.json");
        std::filesystem::copy_file(source/"documentation"/"aerodynamik_konventionen.json",work/"konventionen.json");
        J job=cli::read(source/"examples"/"Auftragsdatei.json");
        job["ressourcen"]["max_prozessorzahl"]=1;job["ressourcen"]["gpu"]="aus";
        {std::ofstream out(work/"Auftragsdatei.json");out<<job.dump();}
        cli::cancelled=true;
        auto aborted=cli::dispatch({"lookup","--auftrag",(work/"Auftragsdatei.json").u8string()});
        cli::cancelled=false;
        check(aborted["exitcode"]==130);
        check(!std::filesystem::exists(work/"LookUp"));
        check(!std::filesystem::exists(work/"LookUp.lock"));
        check(!aborted["pruefbericht"].is_null());
        std::cout<<checks<<" CLI contract checks passed\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
