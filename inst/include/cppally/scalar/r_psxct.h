#ifndef CPPALLY_R_PSXCT_H
#define CPPALLY_R_PSXCT_H

#include <cppally/r_concepts.h>
#include <cppally/r_sexp/protect.h>
#include <cppally/scalar/r_int.h>
#include <cppally/scalar/r_dbl.h>
#include <cppally/scalar/r_str.h>
#include <cppally/scalar/arithmetic_ops.h>
#include <cppally/scalar/relational_ops.h>
#include <cppally/scalar/r_date.h>
#include <cppally/time/zone.h>
#include <cppally/time/timeline.h>
#include <cstdint>
#include <chrono> // For r_date/r_psxt
#include <cstdio> // For snprintf

namespace cppally {

template <string_literal Unit, timeline Axis = timeline::automatic>
constexpr r_dbl time_diff(r_psxct x, r_psxct y, r_dbl width = r_dbl(1.0), roll on_impossible = roll::none) noexcept;

namespace internal {
constexpr r_dbl diff_months(r_psxct x, r_psxct y, r_dbl k = r_dbl(1.0), bool fractional = true, roll on_impossible = roll::none) noexcept;
}

// R date-time: seconds since epoch (1st Jan 1970 UTC).
// Fractional seconds are supported.
struct r_psxct {

    r_dbl value;
    using value_type = r_dbl;

    private:

    const internal::zone_record* m_tzone = nullptr; // nullptr is UTC

    template <string_literal Unit, timeline Axis>
    friend constexpr r_dbl time_diff(r_psxct x, r_psxct y, r_dbl width, roll on_impossible) noexcept;
    friend constexpr r_dbl internal::diff_months(r_psxct x, r_psxct y, r_dbl k, bool fractional, roll on_impossible) noexcept;

    // r_psxct from epoch + this->m_tzone
    constexpr r_psxct zoned_psxct(double seconds) const noexcept {
        return r_psxct(seconds, m_tzone, internal::no_checks_tag{});
    }

    public:

    constexpr r_psxct() noexcept : value{0.0} {}
    constexpr explicit operator r_dbl() const noexcept { return value; }
    constexpr operator double() const noexcept { return static_cast<double>(value); }

    constexpr r_dbl seconds_since_epoch() const noexcept {
        return static_cast<r_dbl>(*this);
    }

    constexpr r_dbl epoch_offset() const noexcept {
        return seconds_since_epoch();
    }

    // Is r_psxct's timezone "UTC"?
    constexpr bool is_utc() const noexcept {
        return m_tzone == nullptr;
    }

    // Offset from UTC in seconds, e.g. 3600 for BST
    constexpr r_int utc_offset() const noexcept {
        if (!is_chrono_safe()){
            return r_int::na();
        }
        if (is_utc()){
            return r_int(0);
        }
        std::chrono::seconds offset{0};
        if (!internal::utc_offset(m_tzone, chrono_tp(), offset)){
            return r_int::na();
        }
        return r_int(static_cast<int>(offset.count()));
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

    static constexpr r_psxct chrono_to_datetime(std::chrono::sys_seconds tp) noexcept {
        return r_psxct(static_cast<double>(tp.time_since_epoch().count()));
    }

    // Sub-second fraction, always in [0, 1)
    constexpr double chrono_frac() const noexcept {
        return unwrap(*this) - static_cast<double>(chrono_tp().time_since_epoch().count());
    }

    public:

    explicit constexpr r_psxct(double seconds_since_epoch) noexcept : value {
        (!r_dbl(seconds_since_epoch).is_na() && (seconds_since_epoch < chrono_min_seconds() || seconds_since_epoch >= chrono_max_seconds()) && r_dbl(seconds_since_epoch).is_finite()) ? r_dbl::na() : seconds_since_epoch
    } {}

    explicit r_psxct(double seconds_since_epoch, const char* tz) noexcept
      : r_psxct(seconds_since_epoch, tz ? internal::zone_registry::get().find_or_add(tz) : nullptr, internal::no_checks_tag{}) {}

    // r_psxct constructor using an already known zone_record
    explicit constexpr r_psxct(double seconds_since_epoch, const internal::zone_record* zone, internal::no_checks_tag) noexcept : r_psxct(seconds_since_epoch) {
        m_tzone = zone;
        if (zone && zone->invalid) {
            value = r_dbl::na();
        }
    }

    static constexpr r_psxct na() noexcept {
        r_psxct out;
        out.value = r_dbl::na();
        return out;
    }

    constexpr bool is_na() const noexcept {
        return seconds_since_epoch().is_na();
    }

    // timezone as a const char*
    const char* tzone() const noexcept {
        return is_utc() ? "UTC" : m_tzone->label();
    }

    // Same UTC instant, different timezone
    [[nodiscard]] r_psxct with_tzone(const char* tz) const noexcept {
        return r_psxct(unwrap(*this), tz);
    }
    [[nodiscard]] r_psxct with_tzone(r_str_view tz) const noexcept {
        return tz.is_na() ? na() : with_tzone(tz.c_str());
    }

    // Same wall clock, different timezone.
    // If the UTC offset of the timezone is different, then this UTC instant will be different.
    // NA is returned for ambiguous or non-existent date-times.
    // Supply a roll strategy to `on_impossible` and/or `on_ambiguous` to handle rolling.
    [[nodiscard]] r_psxct replace_tzone(const char* tz, roll on_impossible = roll::none, roll on_ambiguous = roll::none) const noexcept {
        return r_psxct(unwrap(*this), tz).from_clock(to_clock(), on_impossible, on_ambiguous);
    }
    [[nodiscard]] r_psxct replace_tzone(r_str_view tz, roll on_impossible = roll::none, roll on_ambiguous = roll::none) const noexcept {
        return tz.is_na() ? na() : replace_tzone(tz.c_str(), on_impossible, on_ambiguous);
    }

    // Construct r_psxct from the year/month/day hour:minute:second clock time reading in `zone`.
    constexpr explicit r_psxct(
    int year, int month, int day,
    int hour = 0, int minute = 0, double second = 0.0,
    const char* zone = "UTC",
    roll on_impossible = roll::none,
    roll on_ambiguous = roll::none
    ) noexcept : value{unix_seconds(year, month, day, hour, minute, second)} {
        if (zone && std::string_view(zone) != "UTC") {
            m_tzone = internal::zone_registry::get().find_or_add(zone);
            *this = from_clock(r_psxct(unwrap(value)), on_impossible, on_ambiguous);
        }
    }

    // Apply fn(r_psxct) to the clock time reading, mapping the result back into the input timezone
    template <typename F>
    [[nodiscard]] constexpr r_psxct using_clock(F fn, roll on_impossible = roll::none, roll on_ambiguous = roll::none) const noexcept(noexcept(fn(std::declval<r_psxct>()))) {
        return is_utc() ? fn(*this) : using_clock_impl(fn, on_impossible, on_ambiguous);
    }

    private:

    static constexpr double unix_seconds(int year, int month, int day, int hour, int minute, double second) noexcept {

        namespace chrono = std::chrono;

        if (r_int(year).is_na()){
            return r_dbl::na();
        }

        if (year < static_cast<int>(chrono::year::min()) || year > static_cast<int>(chrono::year::max())) {
            return r_dbl::na();
        }

        unsigned int mo = static_cast<unsigned int>(month);
        unsigned int d = static_cast<unsigned int>(day);
        unsigned int h = static_cast<unsigned int>(hour);
        unsigned int mi = static_cast<unsigned int>(minute);

        if (mo > 12 || d > 31 || h > 23 || mi > 59 || !(second >= 0.0 && second < 60.0)) {
            return r_dbl::na();
        }

        auto ymd = chrono::year{year} / chrono::month{mo} / chrono::day{d};

        if (!ymd.ok()) {
            return r_dbl::na();
        }

        int64_t whole = static_cast<int64_t>(second);
        auto tp = chrono::sys_days{ymd} + chrono::hours{h} + chrono::minutes{mi} + chrono::seconds{whole};
        return static_cast<double>(tp.time_since_epoch().count()) + (second - static_cast<double>(whole));
    }

    // Wall-clock, e.g. "01-01-2000 00:00:00 America/New_York" is just "01-01-2000 00:00:00 UTC"
    constexpr r_psxct to_clock() const noexcept {
        if (is_utc()){
            return *this;
        }
        if (!is_chrono_safe()){
            return r_psxct(unwrap(*this));
        }
        return r_psxct(epoch_offset() + utc_offset());
    }

    // Same clock time but in this timezone (i.e. different UTC offset)
    constexpr r_psxct from_clock(r_psxct clock, roll on_impossible = roll::none, roll on_ambiguous = roll::none) const noexcept {
        if (is_utc()){
            return clock;
        }
        if (!clock.is_chrono_safe()){
            return zoned_psxct(unwrap(clock));
        }
        std::chrono::sys_seconds sys{};
        bool at_boundary = false;
        if (!internal::local_to_sys(m_tzone, internal::tz::local_seconds{clock.chrono_tp().time_since_epoch()}, sys, on_impossible, on_ambiguous, at_boundary)){
            return zoned_psxct(r_dbl::na());
        }
        // Sub-second fractions are dropped at transition boundaries
        return zoned_psxct(static_cast<double>(sys.time_since_epoch().count()) + (at_boundary ? 0.0 : clock.chrono_frac()));
    }

    template <string_literal Unit>
    constexpr r_psxct from_clock(r_psxct before, r_psxct after, roll on_impossible, roll on_ambiguous) const noexcept {
        if constexpr (internal::timeline_from_unit<Unit>() == timeline::clock){
            bool moving_forward = !(after < before).is_true();
            return from_clock(after, internal::resolve_roll(on_impossible, moving_forward), internal::resolve_roll(on_ambiguous, moving_forward));
        } else {
            return add_seconds(after.seconds_since_epoch() - before.seconds_since_epoch());
        }
    }

    template <string_literal Unit, typename F>
    CPPALLY_NOINLINE constexpr r_psxct on_clock(F fn, roll on_impossible, roll on_ambiguous) const noexcept {
        r_psxct cl = to_clock();
        return from_clock<Unit>(cl, fn(cl), on_impossible, on_ambiguous);
    }

    template <typename F>
    CPPALLY_NOINLINE constexpr r_psxct using_clock_impl(F fn, roll on_impossible, roll on_ambiguous) const noexcept(noexcept(fn(std::declval<r_psxct>()))) {
        r_psxct cl = to_clock();
        r_psxct after = fn(cl);
        bool moving_forward = !(after < cl).is_true();
        return from_clock(after, internal::resolve_roll(on_impossible, moving_forward), internal::resolve_roll(on_ambiguous, moving_forward));
    }

    constexpr r_psxct add_seconds(r_dbl n) const noexcept {
        return zoned_psxct(seconds_since_epoch() + n);
    }

    // Impossible dates are handled via `roll` option, e.g. `roll::away` rolls 
    // to the start of the next month when `n >= 0` and to the last day of the current month when 
    // `n < 0`
    constexpr r_psxct add_months(r_int n, roll on_impossible = roll::none) const noexcept {

        using namespace std::chrono;

        if (n.is_na()){
            return na();
        }

        if (!is_chrono_safe()){
            return *this;
        }

        sys_days dp = chrono_days();

        year_month_day ymd = year_month_day(dp);

        // Return NA for out-of-range years
        int64_t total_months = static_cast<int64_t>(static_cast<int>(ymd.year())) * 12 + (static_cast<unsigned int>(ymd.month()) - 1) + unwrap(n);
        int64_t target_year = (total_months >= 0 ? total_months : total_months - 11) / 12;
        if (target_year < static_cast<int>(std::chrono::year::min()) || target_year > static_cast<int>(std::chrono::year::max())) [[unlikely]] {
            return na();
        }

        ymd += months{unwrap(n)};
        bool at_boundary = false;

        if (!ymd.ok()) {
            bool forward = false;
            on_impossible = internal::resolve_roll(on_impossible, /*moving_forward = */ unwrap(n) >= 0);
            switch (on_impossible) {
                case roll::forward:  { forward = true;      break; }
                case roll::backward: { forward = false;     break; }
                case roll::boundary: { forward = true; at_boundary = true; break; }
                default:             { return na(); }
            }
            if (forward) {
                ymd = (year_month{ymd.year(), ymd.month()} + months{1}) / std::chrono::day{1};
            } else {
                ymd = ymd.year() / ymd.month() / last;
            }
        }

        // At transition boundary drop the time since midnight (including sub-seconds), otherwise keep it
        double rem = at_boundary ? 0.0 : unwrap(*this) - static_cast<double>(dp.time_since_epoch().count()) * 86400.0;
        return r_psxct(static_cast<double>(sys_days{ymd}.time_since_epoch().count()) * 86400.0 + rem);
    }

    // Returns the date-time that is partway through the interval [lhs, rhs)
    static constexpr r_psxct interpolate(r_psxct lhs, r_psxct rhs, r_dbl fraction) noexcept {
        r_dbl n_seconds = rhs.seconds_since_epoch() - lhs.seconds_since_epoch();
        return lhs.add_seconds(fraction * n_seconds);
    }

    constexpr r_psxct add_months(r_dbl m, roll on_impossible) const noexcept {

        double floored = internal::floor2(m);
        r_int whole_months = internal::coerce_number<r_int>(r_dbl(floored));
        r_psxct lhs = add_months(whole_months, on_impossible);

        if (unwrap(m) == floored){
            return lhs;
        }

        r_psxct rhs = add_months(whole_months + r_int(1), on_impossible);
        return interpolate(lhs, rhs, /*fraction = */ m - whole_months);
    }

    constexpr r_psxct add_months(r_dbl n, roll on_impossible, r_dbl k) const noexcept {

        if (!is_chrono_safe() || !n.is_finite() || !k.is_finite()){
            return r_psxct(seconds_since_epoch() + (n * k));
        }

        if (unwrap(k) == 1.0){
            return add_months(n, on_impossible);
        }

        r_dbl whole_blocks = r_dbl(internal::floor2(n));
        r_psxct lhs = add_months(whole_blocks * k, on_impossible);

        if ((n == whole_blocks).is_true()){
            return lhs;
        }

        r_psxct rhs = add_months((whole_blocks + r_dbl(1.0)) * k, on_impossible);
        return interpolate(lhs, rhs, /*fraction = */ n - whole_blocks);
    }

    public:

    template <string_literal Unit, timeline Axis = timeline::automatic, Number N>
    [[nodiscard]] constexpr r_psxct add(N n, roll on_impossible = roll::none, roll on_ambiguous = roll::none, r_dbl width = r_dbl(1.0)) const noexcept {

        constexpr std::string_view unit = internal::normalised_unit<Unit>.view();
        constexpr timeline time_axis = Axis == timeline::automatic ? internal::timeline_from_unit<Unit>() : Axis;

        if constexpr (time_axis == timeline::clock){
            if (!is_utc()){
                return using_clock_impl([&](r_psxct cl) noexcept { return cl.template add<Unit, time_axis>(n, on_impossible, roll::none, width); }, on_impossible, on_ambiguous);
            }
        }

        r_dbl n_units = internal::coerce_number<r_dbl>(n);

        if constexpr (time_axis == timeline::clock && unit == "months") {

            return add_months(n_units, on_impossible, width);

        } else if constexpr (time_axis == timeline::clock && unit == "years") {

            return add_months(n_units, on_impossible, width * r_dbl(12.0));

        } else {

            return add_seconds((n_units * width) * r_dbl(internal::average_seconds<Unit>()));

        }
    }

    // Legacy deprecated overload - Use add(N, on_impossible, on_ambiguous, width)
    template <string_literal Unit, Number N>
    [[nodiscard]] constexpr r_psxct add(N n, roll on_impossible, r_dbl width) const noexcept {
        return add<Unit>(n, on_impossible, roll::none, width);
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
        r_psxct cl = to_clock();
        return cl.is_chrono_safe() ? r_int(static_cast<int>(cl.chrono_hms().hours().count())) : r_int::na();
    }

    constexpr r_int minute() const noexcept {
        r_psxct cl = to_clock();
        return cl.is_chrono_safe() ? r_int(static_cast<int>(cl.chrono_hms().minutes().count())) : r_int::na();
    }

    constexpr r_dbl second() const noexcept {
        r_psxct cl = to_clock();
        return cl.is_chrono_safe() ? r_dbl(static_cast<double>(cl.chrono_hms().seconds().count()) + cl.chrono_frac()) : r_dbl::na();
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

        std::chrono::seconds offset{0};
        std::string abbrev;
        if (!internal::utc_offset(m_tzone, chrono_tp(), offset, &abbrev)){
            return r_str::na();
        }

        // The clock time reading, formatted as if UTC
        r_psxct cl(unwrap(*this) + static_cast<double>(offset.count()));
        if (!cl.is_chrono_safe()){
            return r_str::na();
        }

        auto ymd = cl.chrono_ymd();
        auto hms = cl.chrono_hms();

        // "0.500000" -> "0.5" -> ".5", and "0.000000" -> "0" -> "" once the leading digit is skipped
        char frac[10];
        int n = std::snprintf(frac, sizeof(frac), "%.6f", cl.chrono_frac());
        while (frac[n - 1] == '0') {
            --n;
        }
        if (frac[n - 1] == '.') {
            --n;
        }
        frac[n] = '\0';

        char buf[64];
        std::snprintf(buf, sizeof(buf),
            "%04d-%02u-%02u %02u:%02u:%02u%s %s",
            static_cast<int32_t>(ymd.year()),
            static_cast<uint32_t>(ymd.month()),
            static_cast<uint32_t>(ymd.day()),
            static_cast<uint32_t>(hms.hours().count()),
            static_cast<uint32_t>(hms.minutes().count()),
            static_cast<uint32_t>(hms.seconds().count()),
            frac + 1,
            abbrev.c_str()
        );
        return r_str(static_cast<const char*>(buf));
    }

    r_str as_str() const {
        return datetime_str();
    }

    // The date on the zone's clock time
    constexpr r_date as_date() const noexcept {
        return r_date(r_dbl(internal::floor2(to_clock().seconds_since_epoch() / r_dbl(86400.0))));
    }

    // today's time based on unix time in fractional seconds (to microsecond level)
    static r_psxct now() noexcept {
        return r_psxct(
            std::chrono::floor<std::chrono::microseconds>(std::chrono::system_clock::now()).time_since_epoch().count() / (1000.0 * 1000.0)
        );
    }

    // Rounding

    template <string_literal Unit>
    [[nodiscard]] constexpr r_psxct floor(int week_start = 7, roll on_impossible = roll::none, roll on_ambiguous = roll::none) const noexcept {

        constexpr std::string_view unit = internal::normalised_unit<Unit>.view();

        if (!seconds_since_epoch().is_finite()){
            return *this;
        }

        // In a zone, rounding happens on its clock time
        if (!is_utc()){
            return on_clock<Unit>([&](r_psxct cl){ return cl.template floor<Unit>(week_start); }, on_impossible, on_ambiguous);
        }

        using namespace std::chrono;

        if constexpr (unit == "years") {

            return chrono_to_datetime(sys_days{chrono_ymd().year() / January / 1});

        } else if constexpr (unit == "months") {

            year_month_day ymd = chrono_ymd();
            return chrono_to_datetime(sys_days{ymd.year() / ymd.month() / 1});

        } else if constexpr (unit == "weeks") {

            r_int wd = wday(week_start);
            return wd.is_na() ? na() : chrono_to_datetime(chrono_days() - days{unwrap(wd) - 1});

        } else if constexpr (unit == "days"){

            return chrono_to_datetime(chrono_days());

        } else if constexpr (unit == "hours"){

            return chrono_to_datetime(std::chrono::floor<hours>(chrono_tp()));

        } else if constexpr (unit == "minutes"){

            return chrono_to_datetime(std::chrono::floor<minutes>(chrono_tp()));

        } else { // Seconds

            return chrono_to_datetime(chrono_tp());

        }
    }

    template <string_literal Unit>
    [[nodiscard]] constexpr r_psxct ceiling(int week_start = 7, roll on_impossible = roll::none, roll on_ambiguous = roll::none) const noexcept {

        if (!seconds_since_epoch().is_finite()){
            return *this;
        }

        if (!is_utc()){
            return on_clock<Unit>([&](r_psxct cl){ return cl.template ceiling<Unit>(week_start); }, on_impossible, on_ambiguous);
        }

        r_psxct lower = floor<Unit>(week_start);

        if (lower.is_na()){
            return na();
        }

        // If on the boundary, return the boundary
        if ((lower == *this).is_true()){
            return lower;
        }

        return add<Unit>(1, roll::backward).template floor<Unit>(week_start);
    }

    template <string_literal Unit>
    [[nodiscard]] constexpr r_psxct round(int week_start = 7, roll on_impossible = roll::none, roll on_ambiguous = roll::none) const noexcept {

        if (!seconds_since_epoch().is_finite()){
            return *this;
        }

        if (!is_utc()){
            return on_clock<Unit>([&](r_psxct cl){ return cl.template round<Unit>(week_start); }, on_impossible, on_ambiguous);
        }

        r_psxct lower = floor<Unit>(week_start);
        r_psxct upper = ceiling<Unit>(week_start);
        r_dbl lower_diff = seconds_since_epoch() - lower.seconds_since_epoch();
        r_dbl upper_diff = upper.seconds_since_epoch() - seconds_since_epoch();

        // If we're at exactly the halfway point, floor the date to the nearest time unit
        // So for example, if we're rounding to the nearest day and the date is at noon, we floor to midnight on the same day.
        if ((lower_diff <= upper_diff).is_true()){
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

inline constexpr r_dbl diff_month_blocks(r_psxct x, r_psxct y, r_dbl k, r_dbl estimate, bool fractional, roll on_impossible) noexcept {

    r_psxct small_int_start = x.add<"months">(estimate * k, on_impossible);
    r_psxct big_int_end = x.add<"months">((estimate + r_dbl(1.0)) * k, on_impossible);

    r_dbl ratio = diff_seconds(small_int_start, y) / diff_seconds(small_int_start, big_int_end);

    if (ratio.is_finite() && (unwrap(ratio) < 0.0 || unwrap(ratio) >= 1.0)){

        estimate = estimate + r_dbl(unwrap(ratio) < 0.0 ? -1.0 : 1.0);

        small_int_start = x.add<"months">(estimate * k, on_impossible);
        big_int_end = x.add<"months">((estimate + r_dbl(1.0)) * k, on_impossible);

        ratio = diff_seconds(small_int_start, y) / diff_seconds(small_int_start, big_int_end);
    }

    if (small_int_start.is_na()){
        return r_dbl::na();
    }

    if (!fractional || static_cast<double>(y) == static_cast<double>(small_int_start)){
        return estimate;
    }

    return ratio.is_finite() ? estimate + ratio : r_dbl::na();
}

inline constexpr r_dbl diff_months(r_psxct x, r_psxct y, r_dbl k, bool fractional, roll on_impossible) noexcept {

    if (!k.is_finite() || unwrap(k) == 0.0){
        return ( y.seconds_since_epoch() - x.seconds_since_epoch() ) / k;
    }

    y = x.zoned_psxct(unwrap(y)).to_clock();
    x = x.to_clock();

    r_dbl out = diff_months(x.as_date(), y.as_date(), r_dbl(1.0), false, on_impossible);

    if (unwrap(k) != 1.0){
        r_dbl exact_months = out.is_na() ? out : diff_month_blocks(x, y, r_dbl(1.0), out, true, on_impossible);
        if (exact_months.is_na()){
            out = diff_months(x.as_date(), y.as_date(), k, false, on_impossible);
        } else {
            out = r_dbl(floor2(exact_months / k));
        }
    }

    if (out.is_na()){
        return out;
    }

    return diff_month_blocks(x, y, k, /*estimate = */ out, fractional, on_impossible);
}

}

template <string_literal Unit, timeline Axis>
inline constexpr r_dbl time_diff(r_psxct x, r_psxct y, r_dbl width, roll on_impossible) noexcept {

    constexpr std::string_view unit = internal::normalised_unit<Unit>.view();
    constexpr timeline time_axis = Axis == timeline::automatic ? internal::timeline_from_unit<Unit>() : Axis;

    if constexpr (time_axis == timeline::stopwatch && unit == "seconds") {

        r_dbl out = internal::diff_seconds(x, y);
        return unwrap(width) == 1.0 ? out : out / width;

    } else if constexpr (time_axis == timeline::stopwatch) {

        return internal::diff_seconds(x, y, width * r_dbl(internal::average_seconds<Unit>()));

    } else if constexpr (unit == "years") {

        return internal::diff_months(x, y, width * r_dbl(12.0), true, on_impossible);

    } else if constexpr (unit == "months") {

        return internal::diff_months(x, y, width, true, on_impossible);

    } else {

        return internal::diff_seconds(x.to_clock(), x.zoned_psxct(unwrap(y)).to_clock(), width * r_dbl(internal::average_seconds<Unit>()));

    }
}

}

#endif
