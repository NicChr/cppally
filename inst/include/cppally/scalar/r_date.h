#ifndef CPPALLY_R_DATE_H
#define CPPALLY_R_DATE_H

#include <cppally/r_concepts.h>
#include <cppally/r_sexp/protect.h>
#include <cppally/scalar/r_int.h>
#include <cppally/scalar/r_dbl.h>
#include <cppally/scalar/r_int64.h>
#include <cppally/scalar/r_str.h>
#include <cstdint>
#include <chrono> // For r_date/r_psxt

namespace cppally {

// R date that captures the number of days since epoch (1st Jan 1970)
struct r_date {

    r_dbl value;
    using value_type = r_dbl;

    constexpr r_date() noexcept : value{0.0} {}
    constexpr explicit operator r_dbl() const noexcept { return value; }
    constexpr operator double() const noexcept { return static_cast<double>(value); }
    
    private: 

    constexpr auto chrono_ymd() const noexcept {
        return std::chrono::year_month_day{
            std::chrono::sys_days{std::chrono::days{static_cast<int32_t>(value.value)}}
        };
    }

    public:

    explicit constexpr r_date(double days_since_epoch) noexcept : value{days_since_epoch} {}

    // Construct r_date year/month/day
    constexpr explicit r_date(int32_t year, uint32_t month, uint32_t day) noexcept {
        
        namespace chrono = std::chrono;
        auto ymd = chrono::year{year} / chrono::month{month} / chrono::day{day};

        r_dbl out;

        if (!ymd.ok()) {
            out = r_dbl::na();
        } else {
            out = r_dbl(static_cast<double>(chrono::sys_days{ymd}.time_since_epoch().count()));
        }

        value = out;
    }

    static constexpr r_date na() noexcept {
        return r_date(r_dbl::na());
    }

    constexpr bool is_na() const noexcept {
        return value.is_na();
    }

    r_str date_str() const {
        if (is_na()) return r_str::na();
        auto ymd = chrono_ymd();
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%04d-%02u-%02u", static_cast<int32_t>(ymd.year()), static_cast<uint32_t>(ymd.month()), static_cast<uint32_t>(ymd.day()));
        return r_str(static_cast<const char*>(buf));
    }
    
    constexpr r_date add_days(double n) const noexcept {
        return is_na() || r_dbl(n).is_na() ? na() : r_date(unwrap(*this) + n);
    }

    // Impossible dates are returned as NA
    constexpr r_date add_months(int n) const noexcept {
        
        using namespace std::chrono;

        if (is_na()){
            return *this;
        }

        if (r_int(n).is_na()){
            return na();
        }
    
        int days_since_epoch = static_cast<int>(unwrap(*this));
    
        year_month_day ymd = year_month_day(sys_days(days(days_since_epoch)));
        ymd += months{n};

        if (!ymd.ok()) {
            return na();
        }

        return r_date(static_cast<double>(sys_days{ymd}.time_since_epoch().count()));
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
