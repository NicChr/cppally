#ifndef CPPALLY_R_DATE_H
#define CPPALLY_R_DATE_H

#include <cppally/r_concepts.h>
#include <cppally/r_sexp/protect.h>
#include <cppally/scalar/r_int.h>
#include <cppally/scalar/r_dbl.h>
#include <cppally/scalar/r_str.h>
#include <cppally/scalar/arithmetic_ops.h>
#include <cppally/scalar/relational_ops.h>
#include <cppally/utils.h>
#include <cppally/time/roll.h>
#include <cppally/time/unit.h>
#include <cppally/time/timeline.h>
#include <cstdint>
#include <chrono> // For r_date/r_psxt
#include <cstdio> // For snprintf

namespace cppally {

// R date that captures the number of days since epoch (1st Jan 1970)
// Since r_date is stored as a double to match R storage, this means fractional dates are supported but highly discouraged - use `r_psxct` instead for date-times.
struct r_date {

    r_dbl value;
    using value_type = r_dbl;

    constexpr r_date() noexcept : value{0.0} {}
    constexpr explicit operator r_dbl() const noexcept { return value; }
    constexpr operator double() const noexcept { return static_cast<double>(value); }

    constexpr r_dbl days_since_epoch() const noexcept {
        return static_cast<r_dbl>(*this);
    }

    constexpr r_dbl epoch_offset() const noexcept {
        return days_since_epoch();
    }

    private:

    static constexpr double chrono_min_days() noexcept {
        using namespace std::chrono;
        return sys_days{year::min()/January/1}.time_since_epoch().count();
    }

    static constexpr double chrono_max_days() noexcept {
        using namespace std::chrono;
        return (sys_days{year::max()/December/31} + days{1}).time_since_epoch().count();
    }

    // Is the current date finite and representable for use with chrono?
    // Assume days_since_epoch() has already been validated by r_date ctor
    constexpr bool is_chrono_safe() const noexcept {
        return days_since_epoch().is_finite(); // Also false for NA
    }

    // Please ensure is_chrono_safe() is true before calling chrono_days()
    constexpr std::chrono::sys_days chrono_days() const noexcept {
        return std::chrono::sys_days{std::chrono::days{static_cast<int32_t>(internal::floor2(unwrap(*this)))}};
    }

    // Please ensure is_chrono_safe() is true before calling chrono_ymd()
    constexpr std::chrono::year_month_day chrono_ymd() const noexcept {
        return std::chrono::year_month_day{chrono_days()};
    }

    static constexpr r_date chrono_to_date(std::chrono::sys_days d) noexcept {
        return r_date(static_cast<double>(d.time_since_epoch().count()));
    }

    public:

    explicit constexpr r_date(double days_since_epoch) noexcept : value {
        (!r_dbl(days_since_epoch).is_na() && (days_since_epoch < chrono_min_days() || days_since_epoch >= chrono_max_days()) && r_dbl(days_since_epoch).is_finite()) ? r_dbl::na() : days_since_epoch
    } {}

    static constexpr r_date na() noexcept {
        r_date out;
        out.value = r_dbl::na();
        return out;
    }

    constexpr bool is_na() const noexcept {
        return days_since_epoch().is_na();
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

    private: 

    constexpr r_date add_days(r_dbl n) const noexcept {
        return r_date(days_since_epoch() + n);
    }
    
    // Impossible dates are handled via `roll` option, e.g. `roll::away` rolls
    // to the start of the next month when `n >= 0` and to the last day of the current month when 
    // `n < 0`
    constexpr r_date add_months(r_int n, roll on_impossible = roll::none) const noexcept {
        
        using namespace std::chrono;

        if (n.is_na()){
            return na();
        }

        if (!is_chrono_safe()){
            return *this;
        }
        
        year_month_day ymd = chrono_ymd();

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

        // Re-add fractional part that gets dropped by chrono
        double rem = at_boundary ? 0.0 : unwrap(*this) - internal::floor2(unwrap(*this));
        return r_date(static_cast<double>(sys_days{ymd}.time_since_epoch().count()) + rem);
    }

    // Returns the date that is partway through the interval [lhs, rhs)
    static constexpr r_date interpolate(r_date lhs, r_date rhs, r_dbl fraction) noexcept {
        r_dbl n_days = rhs.days_since_epoch() - lhs.days_since_epoch();
        return lhs.add_days(fraction * n_days);
    }

    // Callers must have checked that both `*this` and `m` are finite
    constexpr r_date add_months(r_dbl m, roll on_impossible) const noexcept {

        double floored = internal::floor2(m);
        r_int whole_months = internal::coerce_number<r_int>(r_dbl(floored));
        r_date lhs = add_months(whole_months, on_impossible);

        if (unwrap(m) == floored){
            return lhs;
        }

        r_date rhs = add_months(whole_months + r_int(1), on_impossible);
        return interpolate(lhs, rhs, /*fraction = */ m - whole_months);
    }

    constexpr r_date add_months(r_dbl n, roll on_impossible, r_dbl k) const noexcept {

        if (!is_chrono_safe() || !n.is_finite() || !k.is_finite()){
            return r_date(days_since_epoch() + (n * k));
        }

        if (unwrap(k) == 1.0){
            return add_months(n, on_impossible);
        }

        r_dbl whole_blocks = r_dbl(internal::floor2(n));
        r_date lhs = add_months(whole_blocks * k, on_impossible);

        if ((n == whole_blocks).is_true()){
            return lhs;
        }

        r_date rhs = add_months((whole_blocks + r_dbl(1.0)) * k, on_impossible);
        return interpolate(lhs, rhs, /*fraction = */ n - whole_blocks);
    }

    public:

    template <string_literal Unit, timeline Axis = timeline::automatic, Number N>
    [[nodiscard]] constexpr r_date add(N n, roll on_impossible = roll::none, r_dbl width = r_dbl(1.0)) const noexcept {

        constexpr std::string_view unit = internal::normalised_time_unit<Unit>.view();
        constexpr timeline time_axis = Axis == timeline::automatic ? internal::timeline_from_unit<Unit>() : Axis;

        r_dbl n_units = internal::coerce_number<r_dbl>(n);

        if constexpr (time_axis == timeline::stopwatch && (unit == "months" || unit == "years")) {

            constexpr r_dbl unit_seconds = r_dbl(internal::average_seconds<Unit>());
            constexpr r_dbl day_seconds = r_dbl(internal::average_seconds<"days">());
            return add_days(( (n_units * width) * unit_seconds ) / day_seconds);

        } else if constexpr (unit == "years") {

            return add_months(n_units, on_impossible, width * r_dbl(12.0));

        } else if constexpr (unit == "months") {

            return add_months(n_units, on_impossible, width);

        } else if constexpr (unit == "weeks") {

            return add_days(n_units * width * r_dbl(7.0));

        } else if constexpr (unit == "days"){

            return add_days(n_units * width);

        } else if constexpr (unit == "hours"){

            return add_days((n_units * width) / r_dbl(24.0));

        } else if constexpr (unit == "minutes"){

            return add_days((n_units * width) / r_dbl(1440.0));

        } else { // Seconds

            return add_days((n_units * width) / r_dbl(86400.0));

        }

    }

    // Year number
    constexpr r_int year() const noexcept {
        return is_chrono_safe() ? r_int(static_cast<int>(chrono_ymd().year())) : r_int::na();
    }
    
    // Month of the year
    constexpr r_int month() const noexcept {
        return is_chrono_safe() ? r_int(static_cast<int>(static_cast<unsigned int>(chrono_ymd().month()))) : r_int::na();
    }
    
    // Week of the year
    constexpr r_int week() const noexcept {
        r_int yd = yday();
        return yd.is_na() ? yd : r_int( ((unwrap(yd) - 1) / 7) + 1);
    }

    // ISO-8601 weeks.
    // A year has either 52 or 53 full ISO weeks, which has the advantage that all ISO weeks have 7 days.
    constexpr r_int iso_week() const noexcept {
        r_date thursday = add_days(r_dbl(4) - wday(/*week_start=*/ 1));
        r_int yd = thursday.yday();
        return yd.is_na() ? yd : r_int( ((unwrap(yd) - 1) / 7) + 1 );
    }

    constexpr r_int iso_year() const noexcept {
        r_date thursday = add_days(r_dbl(4) - wday(/*week_start=*/ 1));
        return thursday.year();
    }
    
    // Day of the month
    constexpr r_int day() const noexcept {
        return is_chrono_safe() ? r_int(static_cast<int>(static_cast<unsigned int>(chrono_ymd().day()))) : r_int::na();
    }

    // Day of the year
    constexpr r_int yday() const noexcept {
        if (!is_chrono_safe()) {
            return r_int::na();
        }
        using namespace std::chrono;
        sys_days this_day = chrono_days();
        sys_days jan_1 = year_month_day{this_day}.year() / January / 1;
        return r_int(static_cast<int>((this_day - jan_1).count()) + 1);
    }

    // Day of the week (1-based)
    // week_start = [1 = Monday, 7 = Sunday]
    constexpr r_int wday(int week_start = 7) const noexcept {
        if (!is_chrono_safe() || week_start < 1 || week_start > 7) {
            return r_int::na();
        }
        int iso_wday = static_cast<int>(std::chrono::weekday{chrono_days()}.iso_encoding());
        return r_int((iso_wday - week_start + 7) % 7 + 1);
    }

    constexpr r_lgl is_leap_year() const noexcept {
        return is_chrono_safe() ? r_lgl(chrono_ymd().year().is_leap()) : r_na;
    }

    constexpr r_int days_in_month() const noexcept {
        r_date first_day_of_month = r_date(year(), month(), 1);
        r_date first_day_of_next_month = first_day_of_month.add_months(r_int(1));
        r_dbl out = first_day_of_next_month.days_since_epoch() - first_day_of_month.days_since_epoch();

        return internal::coerce_number<r_int>(out);
    }

    r_str date_str() const {
        
        if (days_since_epoch().is_infinite()){
            return unwrap(*this) > 0 ? cached_str<"Inf">() : cached_str<"-Inf">();
        }

        if (!is_chrono_safe()){
            return r_str::na();
        }

        auto ymd = chrono_ymd();
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%04d-%02u-%02u", static_cast<int32_t>(ymd.year()), static_cast<uint32_t>(ymd.month()), static_cast<uint32_t>(ymd.day()));
        return r_str(static_cast<const char*>(buf));
    }

    r_str as_str() const {
        return date_str();
    }

    constexpr r_psxct as_datetime() const noexcept;

    // today's date based on unix time
    static r_date today() noexcept {
        return r_date(static_cast<double>(
            std::chrono::floor<std::chrono::days>(std::chrono::system_clock::now()).time_since_epoch().count()
        ));
    }

    // Rounding

    template <string_literal Unit>
    [[nodiscard]] constexpr r_date floor(int week_start = 7) const noexcept {

        constexpr std::string_view unit = internal::normalised_time_unit<Unit>.view();

        if (!days_since_epoch().is_finite()){
            return *this;
        }

        using namespace std::chrono;

        if constexpr (unit == "years") {

            return chrono_to_date(chrono_ymd().year() / January / 1);

        } else if constexpr (unit == "months") {

            year_month_day ymd = chrono_ymd();
            return chrono_to_date(ymd.year() / ymd.month() / 1);

        } else if constexpr (unit == "weeks") {

            r_int wd = wday(week_start);
            return wd.is_na() ? na() : chrono_to_date(chrono_days() - days{unwrap(wd) - 1});

        } else if constexpr (unit == "days"){

            return chrono_to_date(chrono_days());

        } else if constexpr (unit == "hours"){

            return r_date(r_dbl(internal::floor2(days_since_epoch() * 24.0)) / 24.0);

        } else if constexpr (unit == "minutes"){

            return r_date(r_dbl(internal::floor2(days_since_epoch() * 1440.0)) / 1440.0);

        } else { // Seconds

            return r_date(r_dbl(internal::floor2(days_since_epoch() * 86400.0)) / 86400.0);

        }
    }

    template <string_literal Unit>
    [[nodiscard]] constexpr r_date ceiling(int week_start = 7) const noexcept {

        if (!days_since_epoch().is_finite()){
            return *this;
        }

        r_date lower = floor<Unit>(week_start);

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
    [[nodiscard]] constexpr r_date round(int week_start = 7) const noexcept {

        if (!days_since_epoch().is_finite()){
            return *this;
        }

        r_date lower = floor<Unit>(week_start);
        r_date upper = ceiling<Unit>(week_start);
        r_dbl lower_diff = days_since_epoch() - lower.days_since_epoch();
        r_dbl upper_diff = upper.days_since_epoch() - days_since_epoch();

        // If we're at exactly the halfway point, floor the date to the nearest time unit
        // So for example, if we're rounding to the nearest day and the date is at noon, we floor to midnight on the same day.
        if ((lower_diff <= upper_diff).is_true()){
            return lower;
        } else {
            return upper;
        }

    }

};

namespace internal {

// Difference in days between two dates
inline constexpr r_dbl diff_days(r_date x, r_date y) noexcept {
    return y.days_since_epoch() - x.days_since_epoch();
}

// Adjust the whole month estimate
inline constexpr r_dbl diff_month_blocks(r_date x, r_date y, r_dbl k, r_dbl estimate, bool fractional, roll on_impossible) noexcept {

    r_date small_int_start = x.add<"months">(estimate * k, on_impossible);
    r_date big_int_end = x.add<"months">((estimate + r_dbl(1.0)) * k, on_impossible);

    r_dbl ratio = diff_days(small_int_start, y) / diff_days(small_int_start, big_int_end);

    if (ratio.is_finite() && (unwrap(ratio) < 0.0 || unwrap(ratio) >= 1.0)){

        estimate = estimate + r_dbl(unwrap(ratio) < 0.0 ? -1.0 : 1.0);

        small_int_start = x.add<"months">(estimate * k, on_impossible);
        big_int_end = x.add<"months">((estimate + r_dbl(1.0)) * k, on_impossible);

        ratio = diff_days(small_int_start, y) / diff_days(small_int_start, big_int_end);
    }

    if (small_int_start.is_na()){
        return r_dbl::na();
    }

    if (!fractional || static_cast<double>(y) == static_cast<double>(small_int_start)){
        return estimate;
    }

    return ratio.is_finite() ? estimate + ratio : r_dbl::na();
}

inline constexpr r_dbl diff_months(r_date x, r_date y, r_dbl k = r_dbl(1.0), bool fractional = true, roll on_impossible = roll::none) noexcept {

    if (!k.is_finite() || unwrap(k) == 0.0){
        return ( y.days_since_epoch() - x.days_since_epoch() ) / k;
    }

    if (!x.days_since_epoch().is_finite() || !y.days_since_epoch().is_finite()){
        return r_dbl::na();
    }

    r_int sy = x.year();
    r_int ey = y.year();
    r_int sm = x.month();
    r_int em = y.month();
    r_int smd = x.day();
    r_int emd = y.day();

    // Approximate whole months between x and y
    r_int whole_months = r_int(12) * (ey - sy) + (em - sm) - r_int(static_cast<int>((emd < smd).is_true()));

    if (whole_months.is_na()){
        return r_dbl::na();
    }

    r_dbl months = coerce_number<r_dbl>(whole_months);

    if (unwrap(k) != 1.0){
        r_dbl exact_months = diff_month_blocks(x, y, r_dbl(1.0), months, true, on_impossible);
        if (!exact_months.is_na()){
            months = exact_months;
        }
    }

    return diff_month_blocks(x, y, k, /*estimate = */ r_dbl(floor2(months / k)), fractional, on_impossible);
}

}

template <string_literal Unit, timeline Axis = timeline::automatic>
inline constexpr r_dbl time_diff(r_date x, r_date y, r_dbl width = r_dbl(1.0), roll on_impossible = roll::none) noexcept {

    constexpr std::string_view unit = internal::normalised_time_unit<Unit>.view();
    constexpr timeline time_axis = Axis == timeline::automatic ? internal::timeline_from_unit<Unit>() : Axis;

    if constexpr (time_axis == timeline::stopwatch && (unit == "months" || unit == "years")) {

        constexpr r_dbl unit_seconds = r_dbl(internal::average_seconds<Unit>());
        constexpr r_dbl day_seconds = r_dbl(internal::average_seconds<"days">());

        return (internal::diff_days(x, y) * day_seconds) / (width * unit_seconds);

    } else if constexpr (unit == "years") {

        return internal::diff_months(x, y, width * r_dbl(12.0), true, on_impossible);

    } else if constexpr (unit == "months") {

        return internal::diff_months(x, y, width, true, on_impossible);

    } else if constexpr (unit == "weeks"){

        return internal::diff_days(x, y) / (r_dbl(7.0) * width);

    } else if constexpr (unit == "days"){

        r_dbl out = internal::diff_days(x, y);
        return unwrap(width) == 1.0 ? out : out / width;
        
    } else if constexpr (unit == "hours"){

        r_dbl out = internal::diff_days(x, y) * r_dbl(24.0);
        return unwrap(width) == 1.0 ? out : out / width;

    } else if constexpr (unit == "minutes"){

        r_dbl out = internal::diff_days(x, y) * r_dbl(1440.0);
        return unwrap(width) == 1.0 ? out : out / width;

    } else { // Seconds

        r_dbl out = internal::diff_days(x, y) * r_dbl(86400.0);
        return unwrap(width) == 1.0 ? out : out / width;

    }
}

}

#endif
