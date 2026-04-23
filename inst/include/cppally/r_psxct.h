#ifndef CPPALLY_R_PSXCT_H
#define CPPALLY_R_PSXCT_H

#include <cppally/r_concepts.h>
#include <cppally/r_protect.h>
#include <cppally/r_dbl.h>
#include <cppally/r_str.h>
#include <cstdint>
#include <chrono> // For r_date/r_psxt

namespace cppally {

namespace internal {
inline int64_t get_seconds_since_epoch(int32_t year, uint32_t month, uint32_t day, uint64_t hour, uint64_t min, uint64_t sec) {
    namespace chrono = std::chrono;
    auto ymd = chrono::year{year} / chrono::month{month} / chrono::day{day};
    if (!ymd.ok()) {
        abort("Invalid date: %d-%u-%u", year, month, day);
    }
    auto tp = chrono::sys_days{ymd} + chrono::hours{hour} + chrono::minutes{min} + chrono::seconds{sec};
    return tp.time_since_epoch().count();
}
}
    
// R date-time that captures the number of seconds since epoch (1st Jan 1970)
struct r_psxct : r_dbl {

    using inherited_type = r_dbl;

    r_psxct() : r_dbl{0.0} {}
    explicit constexpr r_psxct(double seconds_since_epoch) : r_dbl{seconds_since_epoch} {}

    // Construct r_date year/month/day
    explicit r_psxct(
    int32_t year, uint32_t month, uint32_t day, 
    uint32_t hour, uint32_t minute, uint32_t second
    ) : r_dbl(internal::get_seconds_since_epoch(year, month, day, hour, minute, second)) {}

    private: 
    
    auto chrono_tp() const {
        return std::chrono::time_point{
            std::chrono::sys_seconds{std::chrono::seconds{static_cast<int64_t>(value)}}
        };
    }

    // Decomposed date + time-of-day
    auto chrono_ymd() const {
        using namespace std::chrono;
        auto tp = chrono_tp();
        auto dp = floor<days>(tp);
        return year_month_day{dp};
    }

    auto chrono_hms() const {
        using namespace std::chrono;
        auto tp = chrono_tp();
        auto dp = floor<days>(tp);
        return hh_mm_ss{tp - dp};
    }

    public: 

    r_str datetime_str() const {
        auto ymd = chrono_ymd();
        auto hms = chrono_hms();
        char buf[30];
        std::snprintf(buf, sizeof(buf),
            "%04d-%02u-%02u %02u:%02u:%02u",
            static_cast<int32_t>(ymd.year()),
            static_cast<uint32_t>(ymd.month()),
            static_cast<uint32_t>(ymd.day()),
            static_cast<uint32_t>(hms.hours().count()),
            static_cast<uint32_t>(hms.minutes().count()),
            static_cast<uint32_t>(hms.seconds().count())
        );
        return r_str(static_cast<const char*>(buf));
    }
};


// template <typename T>
// requires (any<T, r_int64, r_dbl>)
// struct r_psxct_t : T {

//     using inherited_type = T;

//     r_psxct_t() : T{0} {}
//     template <CppMathType U>
//     explicit constexpr r_psxct_t(U seconds_since_epoch) : T{seconds_since_epoch} {}
//     explicit constexpr r_psxct_t(T seconds_since_epoch) : T{seconds_since_epoch} {}

//     // Construct r_date year/month/day
//     explicit r_psxct_t(
//     int32_t year, uint32_t month, uint32_t day, 
//     uint32_t hour, uint32_t minute, uint32_t second
//     ) : T(internal::get_seconds_since_epoch(year, month, day, hour, minute, second)) {}

//     private: 
    
//     auto chrono_tp() const {
//     return std::chrono::time_point{
//         std::chrono::sys_seconds{std::chrono::seconds{static_cast<int64_t>(T::value)}}
//     };
//     }

//     // Decomposed date + time-of-day
//     auto chrono_ymd() const {
//     using namespace std::chrono;
//     auto tp = chrono_tp();
//     auto dp = floor<days>(tp);
//     return year_month_day{dp};
//     }

//     auto chrono_hms() const {
//     using namespace std::chrono;
//     auto tp = chrono_tp();
//     auto dp = floor<days>(tp);
//     return hh_mm_ss{tp - dp};
//     }

//     public: 

//     r_str datetime_str() const {
//     auto ymd = chrono_ymd();
//     auto hms = chrono_hms();
//     char buf[30];
//     std::snprintf(buf, sizeof(buf),
//         "%04d-%02u-%02u %02u:%02u:%02u",
//         static_cast<int32_t>(ymd.year()),
//         static_cast<uint32_t>(ymd.month()),
//         static_cast<uint32_t>(ymd.day()),
//         static_cast<uint32_t>(hms.hours().count()),
//         static_cast<uint32_t>(hms.minutes().count()),
//         static_cast<uint32_t>(hms.seconds().count())
//     );
//     return r_str(static_cast<const char*>(buf));
//     }
// };

}

#endif
