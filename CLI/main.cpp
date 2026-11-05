#include "job.hpp"
#include <atomic>
#include <csignal>
#include <iostream>
#ifdef _WIN32
#include <windows.h>
#endif
namespace {
static_assert(std::atomic<bool>::is_always_lock_free,"Signal handler requires lock-free atomic<bool>");
void stop(int) { cli::cancelled.store(true); }
int execute(const std::vector<std::string>& args) {
    std::signal(SIGINT,stop);
    std::signal(SIGTERM,stop);
#ifdef SIGBREAK
    std::signal(SIGBREAK,stop);
#endif
    auto result=cli::dispatch(args);
    std::cout<<result.dump()<<'\n';
    return result.at("exitcode").get<int>();
}
}
#ifdef _WIN32
int wmain(int argc,wchar_t** argv) {
    SetConsoleOutputCP(CP_UTF8);
    std::vector<std::string> args;
    for(int i=1;i<argc;++i) {
        int n=WideCharToMultiByte(CP_UTF8,0,argv[i],-1,nullptr,0,nullptr,nullptr);
        std::string s(n,'\0');
        WideCharToMultiByte(CP_UTF8,0,argv[i],-1,s.data(),n,nullptr,nullptr);s.pop_back();args.push_back(s);
    }
    return execute(args);
}
#else
int main(int argc,char** argv) {
    std::vector<std::string> args;for(int i=1;i<argc;++i)args.emplace_back(argv[i]);
    return execute(args);
}
#endif
