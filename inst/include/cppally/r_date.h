#ifndef CPPALLY_R_DATE_H
#define CPPALLY_R_DATE_H

#include <cppally/r_concepts.h>
#include <cppally/r_protect.h>
#include <cppally/r_dbl.h>
#include <cppally/r_str.h>
#include <cstdint>
#include <chrono> // For r_date/r_psxt

namespace cppally {
    
namespace internal {
// Construct r_date from year/month/day
inline int64_t get_days_since_epoch(int32_t year, uint32_t month, uint32_t day) {
    namespace chrono = std::chrono;
    auto ymd = chrono::year{year} / chrono::month{month} / chrono::day{day};
    if (!ymd.ok()) {
        abort("Invalid date: %d-%u-%u", year, month, day);
    }
    return chrono::sys_days{ymd}.time_since_epoch().count();
}
}

// R date that captures the number of days since epoch (1st Jan 1970)
struct r_date : r_dbl {

    using inherited_type = r_dbl;
    
    private: 

    auto chrono_ymd() const {
    return std::chrono::year_month_day{
        std::chrono::sys_days{std::chrono::days{static_cast<int32_t>(value)}}
    };
    }

    public: 

    r_date() : r_dbl{0.0} {}

    explicit constexpr r_date(double days_since_epoch) : r_dbl{days_since_epoch} {}

    // Construct r_date year/month/day
    explicit r_date(int32_t year, uint32_t month, uint32_t day) : r_dbl(internal::get_days_since_epoch(year, month, day)) {}

    r_str date_str() const {
    auto ymd = chrono_ymd();
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%04d-%02u-%02u", static_cast<int32_t>(ymd.year()), static_cast<uint32_t>(ymd.month()), static_cast<uint32_t>(ymd.day()));
    return r_str(static_cast<const char*>(buf));
    }
};

// A more flexible templated version that allows for more integer storage
// template <typename T>
// requires (any<T, r_int, r_dbl>)
// struct r_date_t : T {

//     using inherited_type = T;
    
//     private: 

//     auto chrono_ymd() const {
//     return std::chrono::year_month_day{
//         std::chrono::sys_days{std::chrono::days{static_cast<int32_t>(T::value)}}
//     };
//     }

//     public: 

//     r_date_t() : T{0} {}
//     template <CppMathType U>
//     explicit constexpr r_date_t(U days_since_epoch) : T{days_since_epoch} {}
//     explicit constexpr r_date_t(T days_since_epoch) : T{days_since_epoch} {}

//     // Construct r_date year/month/day
//     explicit r_date_t(int32_t year, uint32_t month, uint32_t day) : T(internal::get_days_since_epoch(year, month, day)) {}

//     r_str date_str() const {
//     auto ymd = chrono_ymd();
//     char buf[16];
//     std::snprintf(buf, sizeof(buf), "%04d-%02u-%02u", static_cast<int32_t>(ymd.year()), static_cast<uint32_t>(ymd.month()), static_cast<uint32_t>(ymd.day()));
//     return r_str(static_cast<const char*>(buf));
//     }
// };

}

#endif
