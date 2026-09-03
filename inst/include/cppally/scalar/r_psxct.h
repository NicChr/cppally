#ifndef CPPALLY_R_PSXCT_H
#define CPPALLY_R_PSXCT_H

#include <cppally/r_concepts.h>
#include <cppally/r_sexp/protect.h>
#include <cppally/scalar/r_int.h>
#include <cppally/scalar/r_dbl.h>
#include <cppally/scalar/r_str.h>
#include <cppally/scalar/arithmetic_ops.h>
#include <cppally/scalar/r_date.h>
#include <cstdint>
#include <chrono> // For r_date/r_psxt

namespace cppally {

// R date-time that captures the number of seconds since epoch (1st Jan 1970) and hence implicitly UTC.
// Fractional seconds are supported.
struct r_psxct {

    r_dbl value;
    using value_type = r_dbl;

    private: 

    static constexpr double chrono_min_seconds() noexcept {
        using namespace std::chrono;
        return static_cast<double>(
            sys_seconds{sys_days{year::min()/January/1}}.time_since_epoch().count()
        );
    }

    static constexpr double chrono_max_seconds() noexcept {
        using namespace std::chrono;
        return static_cast<double>(
            sys_seconds{sys_days{year::max()/December/31} + days{1}}.time_since_epoch().count()
        );
    }

    // Is the current date-time finite and representable for use with chrono?
    constexpr bool is_chrono_safe() const noexcept {
        double s = unwrap(*this);
        return s >= chrono_min_seconds() && s < chrono_max_seconds();
    }

    // chrono_* members assume is_chrono_safe() is true, so make sure to call that before calling them

    constexpr std::chrono::sys_seconds chrono_tp() const noexcept {
        namespace chrono = std::chrono;
        double s = unwrap(value);
        int64_t w = static_cast<int64_t>(s);
        // Floor
        if (s < static_cast<double>(w)) {
            w -= 1;
        }
        return chrono::sys_seconds{chrono::seconds{w}};
    }

    // Whole days since epoch
    constexpr std::chrono::sys_days chrono_days() const noexcept {
        return std::chrono::floor<std::chrono::days>(chrono_tp());
    }

    constexpr auto chrono_ymd() const noexcept {
        return std::chrono::year_month_day{chrono_days()};
    }

    constexpr auto chrono_hms() const noexcept {
        return std::chrono::hh_mm_ss{chrono_tp() - chrono_days()};
    }

    // Sub-second fraction, always in [0, 1)
    constexpr double chrono_frac() const noexcept {
        return unwrap(value) - static_cast<double>(chrono_tp().time_since_epoch().count());
    }

    public:
    
    constexpr r_psxct() noexcept : value{0.0} {}
    constexpr explicit operator r_dbl() const noexcept { return value; }
    constexpr operator double() const noexcept { return static_cast<double>(value); }

    explicit constexpr r_psxct(double seconds_since_epoch) noexcept : value{seconds_since_epoch} {}

    static constexpr r_psxct na() noexcept {
        return r_psxct(r_dbl::na());
    }

    constexpr bool is_na() const noexcept {
        return value.is_na();
    }

    // Construct r_psxct from year/month/day hour:minute:second
    constexpr explicit r_psxct(
    int year, int month, int day,
    int hour, int minute, double second
    ) noexcept {

        namespace chrono = std::chrono;

        if (r_int(year).is_na()){
            value = r_dbl::na();
            return;
        }

        if (year < static_cast<int>(chrono::year::min()) || year > static_cast<int>(chrono::year::max())) {
            value = r_dbl::na();
            return;
        }

        unsigned int mo = static_cast<unsigned int>(month);
        unsigned int d = static_cast<unsigned int>(day);
        unsigned int h = static_cast<unsigned int>(hour);
        unsigned int mi = static_cast<unsigned int>(minute);

        if (mo > 12 || d > 31 || h > 23 || mi > 59 || !(second >= 0.0 && second < 60.0)) {
            value = r_dbl::na();
            return;
        }

        auto ymd = chrono::year{year} / chrono::month{mo} / chrono::day{d};

        r_dbl out;

        if (!ymd.ok()) {
            out = r_dbl::na();
        } else {
            int64_t whole = static_cast<int64_t>(second);
            auto tp = chrono::sys_days{ymd} + chrono::hours{h} + chrono::minutes{mi} + chrono::seconds{whole};
            out = r_dbl(static_cast<double>(tp.time_since_epoch().count()) + (second - static_cast<double>(whole)));
        }

        value = out;
    }

    constexpr r_int year() const noexcept {
        return as_date().year();
    }

    constexpr r_int month() const noexcept {
        return as_date().month();
    }

    constexpr r_int week() const noexcept {
        return as_date().week();
    }

    // ISO-8601 weeks.
    // A year has either 52 or 53 full ISO weeks, which has the advantage that all ISO weeks have 7 days.
    constexpr r_int iso_week() const noexcept {
        return as_date().iso_week();
    }

    constexpr r_int iso_year() const noexcept {
        return as_date().iso_year();
    }

    constexpr r_int day() const noexcept {
        return as_date().day();
    }

    constexpr r_int yday() const noexcept {
        return as_date().yday();
    }

    // Day of the week (1-based)
    // week_start = [1 = Monday, 7 = Sunday]
    constexpr r_int wday(int week_start = 7) const noexcept {
        return as_date().wday(week_start);
    }

    constexpr r_int hour() const noexcept {
        return is_chrono_safe() ? r_int(static_cast<int>(chrono_hms().hours().count())) : r_int::na();
    }

    constexpr r_int minute() const noexcept {
        return is_chrono_safe() ? r_int(static_cast<int>(chrono_hms().minutes().count())) : r_int::na();
    }

    constexpr r_dbl second() const noexcept {
        return is_chrono_safe() ? r_dbl(static_cast<double>(chrono_hms().seconds().count()) + chrono_frac()) : r_dbl::na();
    }

    constexpr r_lgl is_leap_year() const noexcept {
        return as_date().is_leap_year();
    }

    constexpr r_int days_in_month() const noexcept {
        return as_date().days_in_month();
    }

    constexpr r_psxct add_seconds(double n) const noexcept {
        return r_psxct(static_cast<r_dbl>(*this) + r_dbl(n));
    }

    // We can do an exact calculation using seconds because we're UTC and hence no DST
    constexpr r_psxct add_days(double n) const noexcept {
        return r_psxct(static_cast<r_dbl>(*this) + r_dbl(86400.0) * r_dbl(n));
    }

    // Impossible dates are handled via `roll` option, e.g. `roll::away` rolls 
    // to the start of the next month when `n >= 0` and to the last day of the current month when 
    // `n < 0`
    constexpr r_psxct add_months(int n, roll on_impossible_date = roll::none) const noexcept {

        using namespace std::chrono;

        if (r_int(n).is_na()){
            return na();
        }

        if (static_cast<r_dbl>(*this).is_infinite()){
            return *this;
        }

        if (!is_chrono_safe()){
            return na();
        }

        sys_days dp = chrono_days();

        year_month_day ymd = year_month_day(dp);
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

        double rem = unwrap(*this) - static_cast<double>(dp.time_since_epoch().count()) * 86400.0;

        return r_psxct(static_cast<double>(sys_days{ymd}.time_since_epoch().count()) * 86400.0 + rem);
    }

    template <typename F>
    requires (is<F, r_dbl> || CppFloatType<F>)
    constexpr r_psxct add_months(F n, roll on_impossible_date = roll::none) const noexcept {
        
        using namespace std::chrono;

        if (is_na() || r_dbl(n).is_na()){
            return na();
        }

        if (static_cast<r_dbl>(*this).is_infinite() || r_dbl(n).is_infinite()){
            return r_psxct(static_cast<r_dbl>(*this) + r_dbl(n));
        }

        r_int whole_months = internal::coerce_number<r_int>(r_dbl(internal::floor2(n)));

        if (whole_months.is_na()){
            return na();
        }

        r_psxct out = add_months(whole_months, on_impossible_date);

        if (unwrap(n) == unwrap(whole_months)){
            return out;
        }

        double fraction = n - whole_months;
        r_psxct next_month = add_months(whole_months + 1, on_impossible_date);
        
        // Number of seconds between result and next month
        double n_seconds = static_cast<r_dbl>(next_month) - static_cast<r_dbl>(out);

        // add (fraction * n_seconds) seconds to result
        return out.add_seconds(fraction * n_seconds);
    }

    r_str datetime_str() const {
    
        if (static_cast<r_dbl>(*this).is_infinite()){
            return r_str(unwrap(value) > 0 ? "Inf" : "-Inf");
        }

        if (!is_chrono_safe()){
            return r_str::na();
        }    

        auto ymd = chrono_ymd();
        auto hms = chrono_hms();

        // "0.500000" -> "0.5" -> ".5", and "0.000000" -> "0" -> "" once the leading digit is skipped
        char frac[10];
        int n = std::snprintf(frac, sizeof(frac), "%.6f", chrono_frac());
        while (frac[n - 1] == '0') {
            --n;
        }
        if (frac[n - 1] == '.') {
            --n;
        }
        frac[n] = '\0';

        char buf[48];
        std::snprintf(buf, sizeof(buf),
            "%04d-%02u-%02u %02u:%02u:%02u%s UTC",
            static_cast<int32_t>(ymd.year()),
            static_cast<uint32_t>(ymd.month()),
            static_cast<uint32_t>(ymd.day()),
            static_cast<uint32_t>(hms.hours().count()),
            static_cast<uint32_t>(hms.minutes().count()),
            static_cast<uint32_t>(hms.seconds().count()),
            frac + 1
        );
        return r_str(static_cast<const char*>(buf));
    }

    constexpr r_date as_date() const noexcept {
        return is_chrono_safe() ? r_date(static_cast<double>(chrono_days().time_since_epoch().count())) : r_date::na();
    }

    // today's time based on unix time in fractional seconds (to microsecond level)
    static r_psxct now() noexcept {
        return r_psxct(
            std::chrono::floor<std::chrono::microseconds>(std::chrono::system_clock::now()).time_since_epoch().count() / (1000.0 * 1000.0)
        );
    }

};

inline constexpr r_psxct r_date::as_datetime() const noexcept {
    return r_psxct(static_cast<r_dbl>(*this) * r_dbl(86400.0));
}

inline constexpr r_dbl diff_seconds(r_psxct x, r_psxct y) noexcept {
    return static_cast<r_dbl>(y) - static_cast<r_dbl>(x);
}

inline constexpr r_dbl diff_days(r_psxct x, r_psxct y) noexcept {
    return diff_seconds(x, y) / r_dbl(86400.0);
}

// Number of n-month periods between two dates
inline constexpr r_dbl diff_months(r_psxct x, r_psxct y, int n = 1, bool fractional = true, roll on_impossible_date = roll::none) noexcept {

    r_dbl out = diff_months(x.as_date(), y.as_date(), n, false, on_impossible_date);

    if (out.is_na() || !fractional){
        return out;
    }

    r_int whole = internal::coerce_number<r_int>(out);

    r_int months_add = whole * r_int(n);
    r_psxct small_int_start = x.add_months(months_add, on_impossible_date);

    if (static_cast<double>(y) == static_cast<double>(small_int_start)){
        return out;
    }

    r_psxct big_int_end = x.add_months(months_add + r_int(unwrap(y) > unwrap(x) ? n : -n), on_impossible_date);
    r_dbl fraction = diff_seconds(small_int_start, y) / r_dbl(internal::abs2(unwrap(diff_seconds(small_int_start, big_int_end))));

    return out + fraction;
}

}

#endif
