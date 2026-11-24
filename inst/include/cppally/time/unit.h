#ifndef CPPALLY_TIME_UNIT_H
#define CPPALLY_TIME_UNIT_H

#include <cppally/string/string_literal.h>

namespace cppally {

// Is the time unit valid?
// Valid for 'years', 'months', 'weeks', 'days', 'hours', 'minutes' and 'seconds'
template <string_literal Unit>
consteval bool is_valid_time_unit() noexcept {
    std::string_view unit = Unit.view();
    return unit == "seconds" || unit == "minutes" || unit == "hours" || unit == "days" || unit == "weeks" || unit == "months" || unit == "years";
}

namespace internal {

template <string_literal Unit>
consteval void assert_valid_time_unit() noexcept {
    static_assert(is_valid_time_unit<Unit>(), "Invalid time unit, please supply 'years', 'months', 'weeks', 'days', 'hours', 'minutes', or 'seconds'");
}

// Normalise time units into plural form, e.g. "month"
template <string_literal Unit>
consteval auto normalise_time_unit() noexcept {
    if constexpr (is_valid_time_unit<Unit>()){
        return Unit;
    } else {
        constexpr auto plural = Unit.concat("s");
        if constexpr (is_valid_time_unit<plural>()){
            return plural;
        } else {
            assert_valid_time_unit<Unit>();
            return Unit;
        }
    }
}

template <string_literal Unit>
constexpr auto normalised_time_unit = normalise_time_unit<Unit>();

// Average seconds based on Gregorian calendar
template <string_literal Unit>
consteval double average_seconds() noexcept {
    
    constexpr std::string_view unit = normalised_time_unit<Unit>.view();

    // Average number of seconds per unit in a 400-year period (ignoring leap seconds)

    if (unit == "seconds") {
        return 1.0;
    } else if (unit == "minutes") {
        return 60.0;
    } else if (unit == "hours") {
        return 3600.0;
    } else if (unit == "days") {
        return 86400.0;
    } else if (unit == "weeks") {
        return 604800.0;
    } else if (unit == "months") {
        return 2629746.0;
    } else {
        return 31556952.0;
    }
}

}

}

#endif
