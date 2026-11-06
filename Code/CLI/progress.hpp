#pragma once
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>

namespace cli {
inline std::string progress_duration(unsigned long long seconds) {
    std::ostringstream out;
    out<<std::setfill('0');
    out<<std::setw(2)<<seconds/3600<<":";
    out<<std::setw(2)<<(seconds/60)%60<<":";
    out<<std::setw(2)<<seconds%60;
    return out.str();
}
inline std::string progress_time(double elapsed,size_t done,size_t total) {
    std::ostringstream out;
    out<<"("<<progress_duration(static_cast<unsigned long long>(elapsed))<<" / ";
    if(!done||!total)out<<"wird ermittelt) | Verbleibende Zeit: wird ermittelt";
    else {
        const double remaining=elapsed/static_cast<double>(done)*static_cast<double>(total-std::min(done,total));
        const auto seconds=static_cast<unsigned long long>(std::ceil(remaining));
        out<<progress_duration(static_cast<unsigned long long>(std::ceil(elapsed+remaining)))
           <<") | Verbleibende Zeit: "<<progress_duration(seconds);
    }
    return out.str();
}
inline std::string progress_line(const char* label,size_t done,size_t total,
                                 const char* color,const std::string& suffix={}) {
    constexpr size_t width=28;
    done=std::min(done,total);
    const size_t filled=total?width*done/total:0;
    std::ostringstream out;
    out<<label<<"  "<<color;
    for(size_t i=0;i<filled;++i)out<<u8"\u2501";
    out<<"\x1b[90m";
    for(size_t i=filled;i<width;++i)out<<u8"\u2501";
    out<<"\x1b[0m  "<<done<<" / ";
    if(total)out<<total;else out<<"?";
    if(!suffix.empty())out<<"  | "<<suffix;
    return out.str();
}
}
