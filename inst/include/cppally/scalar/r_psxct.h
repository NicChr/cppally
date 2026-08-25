#ifndef CPPALLY_R_PSXCT_H
#define CPPALLY_R_PSXCT_H

#include <cppally/r_concepts.h>
#include <cppally/r_sexp/protect.h>
#include <cppally/scalar/r_int.h>
#include <cppally/scalar/r_dbl.h>
#include <cppally/scalar/r_str.h>
#include <cppally/scalar/r_date.h> // For rolling impossible dates
#include <cstdint>
#include <chrono> // For r_date/r_psxt

namespace cppally {

// R date-time that captures the number of seconds since epoch (1st Jan 1970) and hence implicitly UTC.
// Fractional seconds are supported.
struct r_psxct {

    r_dbl value;
    using value_type = r_dbl;

    constexpr r_psxct() noexcept : value{0.0} {}
    constexpr explicit operator r_dbl() const noexcept { return value; }
    constexpr operator double() const noexcept { return static_cast<double>(value); }

    private:

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
        return !value.is_finite() ? r_int::na() : r_int(static_cast<int>(chrono_ymd().year()));
    }

    constexpr r_int month() const noexcept {
        return !value.is_finite() ? r_int::na() : r_int(static_cast<int>(static_cast<unsigned int>(chrono_ymd().month())));
    }

    constexpr r_int day() const noexcept {
        return !value.is_finite() ? r_int::na() : r_int(static_cast<int>(static_cast<unsigned int>(chrono_ymd().day())));
    }

    constexpr r_int hour() const noexcept {
        return !value.is_finite() ? r_int::na() : r_int(static_cast<int>(chrono_hms().hours().count()));
    }

    constexpr r_int minute() const noexcept {
        return !value.is_finite() ? r_int::na() : r_int(static_cast<int>(chrono_hms().minutes().count()));
    }

    constexpr r_dbl second() const noexcept {
        return !value.is_finite() ? r_dbl::na() : r_dbl(static_cast<double>(chrono_hms().seconds().count()) + chrono_frac());
    }

    constexpr r_psxct add_seconds(double n) const noexcept {
        return is_na() || r_dbl(n).is_na() ? na() : r_psxct(unwrap(*this) + n);
    }

    // We can do an exact calculation using seconds because we're UTC and hence no DST
    constexpr r_psxct add_days(int n) const noexcept {
        return is_na() || r_int(n).is_na() ? na() : r_psxct(unwrap(*this) + (86400.0 * n));
    }

    // Impossible dates are handled via `roll` option, e.g. `roll::away` rolls 
    // to the start of the next month when `n >= 0` and to the last day of the current month when 
    // `n < 0`
    constexpr r_psxct add_months(int n, roll on_impossible_date = roll::none) const noexcept {

        using namespace std::chrono;

        if (r_int(n).is_na()){
            return na();
        }

        if (!value.is_finite()){
            return *this;
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

    r_str datetime_str() const {
        
        if (is_na()){
            return r_str::na();
        }

        if (value.is_infinite()){
            return r_str(unwrap(value) > 0 ? "Inf" : "-Inf");
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

};

}

#endif
