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
#include <cstdio> // For snprintf

namespace cppally {

// R date-time that captures the number of seconds since epoch (1st Jan 1970) and hence implicitly UTC.
// Fractional seconds are supported.
struct r_psxct {

    r_dbl value;
    using value_type = r_dbl;

    constexpr r_psxct() noexcept : value{0.0} {}
    constexpr explicit operator r_dbl() const noexcept { return value; }
    constexpr operator double() const noexcept { return static_cast<double>(value); }

    constexpr r_dbl seconds_since_epoch() const noexcept {
        return static_cast<r_dbl>(*this);
    }

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
        return seconds_since_epoch().is_finite();
    }

    // chrono_* members assume is_chrono_safe() is true, so make sure to call that before calling them

    constexpr std::chrono::sys_seconds chrono_tp() const noexcept {
        namespace chrono = std::chrono;
        double s = unwrap(*this);
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
        return unwrap(*this) - static_cast<double>(chrono_tp().time_since_epoch().count());
    }

    public:

    explicit constexpr r_psxct(double seconds_since_epoch) noexcept : value {
        (seconds_since_epoch >= chrono_min_seconds() && seconds_since_epoch < chrono_max_seconds()) || r_dbl(seconds_since_epoch).is_infinite() ? seconds_since_epoch : r_dbl::na()
    } {}

    static constexpr r_psxct na() noexcept {
        return r_psxct(r_dbl::na());
    }

    constexpr bool is_na() const noexcept {
        return seconds_since_epoch().is_na();
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

    private: 

    constexpr r_psxct add_seconds(r_dbl n) const noexcept {
        return r_psxct(seconds_since_epoch() + n);
    }

    // Impossible dates are handled via `roll` option, e.g. `roll::away` rolls 
    // to the start of the next month when `n >= 0` and to the last day of the current month when 
    // `n < 0`
    constexpr r_psxct add_months(r_int n, roll on_impossible_date = roll::none) const noexcept {

        using namespace std::chrono;

        if (n.is_na()){
            return na();
        }
        
        if (!is_chrono_safe()){
            return *this;
        }

        sys_days dp = chrono_days();

        year_month_day ymd = year_month_day(dp);
        ymd += months{unwrap(n)};

        if (!ymd.ok()) {
            bool forward = false;
            switch (on_impossible_date) {
                case roll::forward:  { forward = true;      break; }
                case roll::backward: { forward = false;     break; }
                case roll::away:     { forward = (unwrap(n) >= 0);  break; }
                case roll::nearest:  { forward = (unwrap(n) < 0);   break; }
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

    constexpr r_psxct add_months(r_dbl n, roll on_impossible_date = roll::none) const noexcept {
        
        using namespace std::chrono;

        if (!is_chrono_safe() || !n.is_finite()){
            return r_psxct(seconds_since_epoch() + n);
        }

        r_int whole_months = internal::coerce_number<r_int>(r_dbl(internal::floor2(n)));

        if (whole_months.is_na()){
            return na();
        }

        r_psxct out = add_months(whole_months, on_impossible_date);

        if (unwrap(n) == unwrap(whole_months)){
            return out;
        }

        r_dbl fraction = n - whole_months;
        r_psxct next_month = add_months(whole_months + r_dbl(1.0), on_impossible_date);
        
        // Number of seconds between result and next month
        r_dbl n_seconds = next_month.seconds_since_epoch() - out.seconds_since_epoch();

        // add (fraction * n_seconds) seconds to result
        return out.add_seconds(fraction * n_seconds);
    }

    public:

    template <string_literal Unit, Number N> 
    constexpr r_psxct add(N n, roll on_impossible_date = roll::none) const noexcept {

        constexpr std::string_view unit = internal::normalised_unit<Unit>.view();

        if constexpr (unit == "seconds") {
            
            return add_seconds(internal::coerce_number<r_dbl>(n));
            
        } else if constexpr (unit == "minutes") {

            return add_seconds(n * r_dbl(60.0));

        } else if constexpr (unit == "hours") {

            return add_seconds(n * r_dbl(3600.0));

        } else if constexpr (unit == "days") {

            return add_seconds(n * r_dbl(86400.0));

        } else if constexpr (unit == "weeks") {

            return add_seconds(n * r_dbl(604800.0));

        } else if constexpr (unit == "months") {

            return add_months(internal::coerce_number<r_dbl>(n), on_impossible_date);

        } else {

            return add_months(n * r_dbl(12.0), on_impossible_date);

        }
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

    r_str datetime_str() const {
    
        if (seconds_since_epoch().is_infinite()){
            return unwrap(*this) > 0 ? cached_str<"Inf">() : cached_str<"-Inf">();
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
        // return is_chrono_safe() ? r_date(static_cast<double>(chrono_days().time_since_epoch().count())) : r_date(seconds_since_epoch());
        return r_date(r_dbl(internal::floor2(seconds_since_epoch() / r_dbl(86400.0))));
    }

    // today's time based on unix time in fractional seconds (to microsecond level)
    static r_psxct now() noexcept {
        return r_psxct(
            std::chrono::floor<std::chrono::microseconds>(std::chrono::system_clock::now()).time_since_epoch().count() / (1000.0 * 1000.0)
        );
    }

    // Rounding

    template <string_literal Unit>
    constexpr r_psxct floor(int week_start = 7) const noexcept {

        constexpr std::string_view unit = internal::normalised_unit<Unit>.view();

        if (!seconds_since_epoch().is_finite()){
            return *this;
        }

        if constexpr (unit == "years") {

            return r_psxct(year(), 1, 1, 0, 0, 0.0);

        } else if constexpr (unit == "months") {
            
            return r_psxct(year(), month(), 1, 0, 0, 0.0);

        } else if constexpr (unit == "weeks") {

            r_psxct start_of_week = add<"days">(internal::coerce_number<r_dbl>(-(wday(week_start) - 1)));
            return r_psxct(start_of_week.year(), start_of_week.month(), start_of_week.day(), 0, 0, 0);

        } else if constexpr (unit == "days"){

            return r_psxct(year(), month(), day(), 0, 0, 0);

        } else if constexpr (unit == "hours"){

            return r_psxct(year(), month(), day(), hour(), 0, 0);

        } else if constexpr (unit == "minutes"){

            return r_psxct(year(), month(), day(), hour(), minute(), 0);

        } else { // Seconds

            return r_psxct(year(), month(), day(), hour(), minute(), r_dbl(internal::floor2(second())));

        }
    }

    template <string_literal Unit>
    constexpr r_psxct ceiling(int week_start = 7) const noexcept {

        if (!seconds_since_epoch().is_finite()){
            return *this;
        }

        r_psxct lower = floor<Unit>(week_start);
        r_psxct upper = add<Unit>(1);

        if (lower.is_na()){
            return na();
        }

        if ( unwrap(lower.seconds_since_epoch() - seconds_since_epoch()) == 0 ){
            return lower;
        }

        return upper.floor<Unit>(week_start);
    }

    template <string_literal Unit>
    constexpr r_psxct round(int week_start = 7) const noexcept {

        if (!seconds_since_epoch().is_finite()){
            return *this;
        }

        r_psxct lower = floor<Unit>(week_start);
        r_psxct upper = ceiling<Unit>(week_start);
        r_dbl lower_diff = seconds_since_epoch() - lower.seconds_since_epoch();
        r_dbl upper_diff = upper.seconds_since_epoch() - seconds_since_epoch();

        // If we're at exactly the halfway point, floor the date to the nearest time unit
        // So for example, if we're rounding to the nearest day and the date is at noon, we floor to midnight on the same day.
        if (unwrap(lower_diff) <= unwrap(upper_diff)){
            return lower;
        } else {
            return upper;
        }

    }

};

inline constexpr r_psxct r_date::as_datetime() const noexcept {
    return r_psxct(days_since_epoch() * r_dbl(86400.0));
}

namespace internal {

inline constexpr r_dbl diff_seconds(r_psxct x, r_psxct y) noexcept {
    return y.seconds_since_epoch() - x.seconds_since_epoch();
}

inline constexpr r_dbl diff_seconds(r_psxct x, r_psxct y, r_dbl n) noexcept {
    return diff_seconds(x, y) / n;
}

inline constexpr r_dbl diff_months(r_psxct x, r_psxct y, bool fractional = true, roll on_impossible_date = roll::none) noexcept {

    r_dbl out = diff_months(x.as_date(), y.as_date(), false, on_impossible_date);

    if (out.is_na()){
        return out;
    }

    bool l2r = unwrap(y) >= unwrap(x);

    r_psxct small_int_start = x.add<"months">(out, on_impossible_date);

    // If we have overshot, adjust by 1 month
    if (l2r ? unwrap(small_int_start) > unwrap(y) : unwrap(small_int_start) < unwrap(y)){
        out = l2r ? out - r_dbl(1.0) : out + r_dbl(1.0);
        small_int_start = x.add<"months">(out, on_impossible_date);
    }

    if (!fractional || static_cast<double>(y) == static_cast<double>(small_int_start)){
        return out;
    }

    r_psxct big_int_end = x.add<"months">(out + (l2r ? r_dbl(1.0) : r_dbl(-1.0)), on_impossible_date);
    r_dbl ratio = diff_seconds(small_int_start, y) / diff_seconds(small_int_start, big_int_end);

    return l2r ? out + ratio : out - ratio;
}

}

template <string_literal Unit, Number N = double>
inline constexpr r_dbl time_diff(r_psxct x, r_psxct y, N n = 1.0, roll on_impossible_date = roll::none) noexcept {

    constexpr std::string_view unit = internal::normalised_unit<Unit>.view();

    r_dbl n_blocks = internal::coerce_number<r_dbl>(n);

    if (n_blocks.is_na() || unwrap(n_blocks) == 0.0){
        return r_dbl::na();
    }

    if constexpr (unit == "years") {

        return internal::diff_months(x, y, true, on_impossible_date) / (n_blocks * 12.0);

    } else if constexpr (unit == "months") {

        return internal::diff_months(x, y, true, on_impossible_date) / n_blocks;

    } else if constexpr (unit == "weeks"){

        return internal::diff_seconds(x, y, n_blocks * 604800.0);

    } else if constexpr (unit == "days"){

        return internal::diff_seconds(x, y, n_blocks * 86400.0);

    } else if constexpr (unit == "hours"){

        return internal::diff_seconds(x, y, n_blocks * 3600.0);

    } else if constexpr (unit == "minutes"){

        return internal::diff_seconds(x, y, n_blocks * 60.0);

    } else {

        return internal::diff_seconds(x, y, n_blocks);

    }
}

}

#endif
