#ifndef CPPALLY_R_PSXCT_H
#define CPPALLY_R_PSXCT_H

#include <cppally/r_concepts.h>
#include <cppally/r_protect.h>
#include <cppally/r_dbl.h>
#include <cppally/r_int64.h>
#include <cppally/r_str.h>
#include <cstdint>
#include <chrono> // For r_date/r_psxt

namespace cppally {

namespace internal {
inline r_int64 get_seconds_since_epoch(int32_t year, uint32_t month, uint32_t day, uint64_t hour, uint64_t min, uint64_t sec) {
    namespace chrono = std::chrono;
    auto ymd = chrono::year{year} / chrono::month{month} / chrono::day{day};
    if (!ymd.ok()) {
        return r_int64::na();
    }
    auto tp = chrono::sys_days{ymd} + chrono::hours{hour} + chrono::minutes{min} + chrono::seconds{sec};
    return r_int64(static_cast<int64_t>(tp.time_since_epoch().count()));
}
}
    
// R date-time that captures the number of seconds since epoch (1st Jan 1970)
struct r_psxct {

    r_dbl value;
    using value_type = r_dbl;

    constexpr r_psxct() noexcept : value{0.0} {}
    constexpr explicit operator r_dbl() const noexcept { return value; }
    constexpr operator double() const noexcept { return static_cast<double>(value); }

    explicit constexpr r_psxct(double seconds_since_epoch) noexcept : value{seconds_since_epoch} {}

    // Construct r_date year/month/day
    explicit r_psxct(
    int32_t year, uint32_t month, uint32_t day, 
    uint32_t hour, uint32_t minute, uint32_t second
    ) {
        r_int64 res = internal::get_seconds_since_epoch(year, month, day, hour, minute, second);
        r_dbl out = res.is_na() ? r_dbl::na() : r_dbl(static_cast<double>(res.value));
        value = out;
    }

    static constexpr r_psxct na() noexcept {
        return r_psxct(r_dbl::na());
    }

    constexpr bool is_na() const noexcept {
        return value.is_na();
    }

    private: 
    
    auto chrono_tp() const {
        return std::chrono::time_point{
            std::chrono::sys_seconds{std::chrono::seconds{static_cast<int64_t>(value.value)}}
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
        if (is_na()) return internal::na_str;
        auto ymd = chrono_ymd();
        auto hms = chrono_hms();
        char buf[34];
        std::snprintf(buf, sizeof(buf),
            "%04d-%02u-%02u %02u:%02u:%02u UTC",
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

namespace internal {
inline constexpr r_psxct na_psxct = r_psxct::na();
}


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
