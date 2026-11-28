#ifndef CPPALLY_TIMELINE_H
#define CPPALLY_TIMELINE_H

#include <cppally/time/unit.h>

namespace cppally {

// Timeline choices: automatic (default), clock time, or `stopwatch` time (UNIX seconds).
// If you are unsure, use timeline::automatic which chooses `clock` time for arithmetic involving years, months, weeks, and days, and `stopwatch` time otherwise.
enum class timeline {
    automatic,  // `stopwatch` (seconds-based) for sub-day units, otherwise `clock`
    clock,      // arithmetic uses the clock time though results may be ambiguous or non-existent
    stopwatch   // arithmetic uses unix seconds, where months and years become an average number of seconds, based on the Gregorian calendar
};

namespace internal {

template <string_literal Unit> 
consteval timeline timeline_from_unit() noexcept {
    constexpr std::string_view unit = normalised_time_unit<Unit>.view();
    constexpr bool period_unit = unit == "days" || unit == "weeks" || unit == "months" || unit == "years";
    return period_unit ? timeline::clock : timeline::stopwatch;
}

}

}

#endif
