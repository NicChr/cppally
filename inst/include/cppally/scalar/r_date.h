#ifndef CPPALLY_R_DATE_H
#define CPPALLY_R_DATE_H

#include <cppally/r_concepts.h>
#include <cppally/r_sexp/protect.h>
#include <cppally/scalar/r_int.h>
#include <cppally/scalar/r_dbl.h>
#include <cppally/scalar/r_str.h>
#include <cppally/scalar/arithmetic_ops.h>
#include <cstdint>
#include <chrono> // For r_date/r_psxt

namespace cppally {

// Rollover policy for month arithmetic that lands on a day that doesn't exist
// (e.g. Jan 31 + 1 month)
enum roll : uint8_t {
    none = 0,       // does not roll and impossible dates are returned as NA
    backward = 1,   // rolls backwards until the last day of the current month is reached
    forward = 2,    // rolls to first day of next month
    away = 3,       // rolls forward when adding and backward when subtracting (my favourite)
    nearest = 4     // rolls backward when adding and forward when subtracting
};

// R date that captures the number of days since epoch (1st Jan 1970)
// While r_date is stored as a double to match R storage, fractional dates are NOT supported and are discouraged - use `r_psxct` instead for date-times.
struct r_date {

    r_dbl value;
    using value_type = r_dbl;

    constexpr r_date() noexcept : value{0.0} {}
    constexpr explicit operator r_dbl() const noexcept { return value; }
    constexpr operator double() const noexcept { return static_cast<double>(value); }
    
    private: 

    constexpr auto chrono_ymd() const noexcept {
        return std::chrono::year_month_day{
            std::chrono::sys_days{std::chrono::days{static_cast<int32_t>(unwrap(value))}}
        };
    }

    public:

    explicit constexpr r_date(double days_since_epoch) noexcept : value{days_since_epoch} {}

    static constexpr r_date na() noexcept {
        return r_date(r_dbl::na());
    }

    constexpr bool is_na() const noexcept {
        return value.is_na();
    }

    // Construct r_date year/month/day
    constexpr explicit r_date(int year, int month, int day) noexcept {
        
        namespace chrono = std::chrono;

        if (r_int(year).is_na()){
            value = r_dbl::na();
            return;
        }
        
        if (year < static_cast<int>(chrono::year::min()) || year > static_cast<int>(chrono::year::max())) {
            value = r_dbl::na();
            return;
        }

        unsigned int m = static_cast<unsigned int>(month);
        unsigned int d = static_cast<unsigned int>(day);

        if (m > 12 || d > 31) {
            value = r_dbl::na();
            return;
        }

        auto ymd = chrono::year{year} / chrono::month{m} / chrono::day{d};

        r_dbl out;

        if (!ymd.ok()) {
            out = r_dbl::na();
        } else {
            out = r_dbl(static_cast<double>(chrono::sys_days{ymd}.time_since_epoch().count()));
        }

        value = out;
    }

    // Year number
    constexpr r_int year() const noexcept {
        return !value.is_finite() ? r_int::na() : r_int(static_cast<int>(chrono_ymd().year()));
    }
    
    // Month of the year
    constexpr r_int month() const noexcept {
        return !value.is_finite() ? r_int::na() : r_int(static_cast<int>(static_cast<unsigned int>(chrono_ymd().month())));
    }
    
    // Week of the year
    constexpr r_int week() const noexcept {
        r_int res = yday() - r_int(1);
        res /= r_int(7); // Floor integer division
        return res + r_int(1);
    }
    
    // Day of the month
    constexpr r_int day() const noexcept {
        return !value.is_finite() ? r_int::na() : r_int(static_cast<int>(static_cast<unsigned int>(chrono_ymd().day())));
    }

    // Day of the year
    constexpr r_int yday() const noexcept {
        return value.is_finite() ? r_int(static_cast<int>(unwrap(static_cast<r_dbl>(*this) - static_cast<r_dbl>(r_date(unwrap(year()), 1, 1)) + 1.0))) : r_int::na();
    }

    constexpr r_date add_days(int n) const noexcept {
        return r_date(static_cast<r_dbl>(*this) + r_int(n));
    }

    // Defined in r_psxct.h, where r_psxct is complete
    constexpr r_psxct as_datetime() const noexcept;

    // Impossible dates are handled via `roll` option, e.g. `roll::away` rolls 
    // to the start of the next month when `n >= 0` and to the last day of the current month when 
    // `n < 0`
    constexpr r_date add_months(int n, roll on_impossible_date = roll::none) const noexcept {
        
        using namespace std::chrono;

        if (r_int(n).is_na()){
            return na();
        }

        if (!value.is_finite()){
            return *this;
        }
        
        year_month_day ymd = chrono_ymd();
        ymd += months{n};
        
        if (!ymd.ok()) {
            bool forward = false;
            switch (on_impossible_date) {
                case roll::forward:  { forward = true;      break; }
                case roll::backward: { forward = false;     break; }
                case roll::away:     { forward = (n >= 0);  break; }
                case roll::nearest:  { forward = (n < 0);   break; }
                default:             { return na(); }
            }
            if (forward) {
                ymd = (year_month{ymd.year(), ymd.month()} + months{1}) / std::chrono::day{1};
            } else {
                ymd = ymd.year() / ymd.month() / last;
            }
        }
        return r_date(static_cast<double>(sys_days{ymd}.time_since_epoch().count()));
    }

    r_str date_str() const {
        
        if (is_na()){
            return r_str::na();
        }

        if (value.is_infinite()){
            return r_str(unwrap(value) > 0 ? "Inf" : "-Inf");
        }

        auto ymd = chrono_ymd();
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%04d-%02u-%02u", static_cast<int32_t>(ymd.year()), static_cast<uint32_t>(ymd.month()), static_cast<uint32_t>(ymd.day()));
        return r_str(static_cast<const char*>(buf));
    }

    constexpr r_psxct as_datetime() const noexcept;

};

}

#endif
