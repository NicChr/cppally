#ifndef CPPALLY_ROLL_H
#define CPPALLY_ROLL_H

#include <cstdint>

namespace cppally {

// Rollover policy for dates & date-times
// that either don't exist: 30 Feb, or 01:30 March 29 2026 in Europe/London
// or are ambiguous: 01:30 October 25 2026 in Europe/London
enum class roll : uint8_t {
    none = 0,       // impossible and ambiguous results are returned as NA
    backward = 1,   // month arithmetic - last day of the current month, ambiguous date-times - the earlier of the two, non-existent date-times - shifted back by the size of the gap
    forward = 2,    // month arithmetic - first day of the next month, ambiguous date-times - the later of the two, non-existent date-times - shifted forward by the size of the gap
    away = 3,       // rolls forward when adding and backward when subtracting
    nearest = 4,    // rolls backward when adding and forward when subtracting
    boundary = 5    // roll to the start of the interval after the transition (start of next month or new UTC offset)
};

namespace internal {

constexpr roll resolve_roll(roll r, bool moving_forward) noexcept {
    switch (r) {
        case roll::away: {
            return moving_forward ? roll::forward : roll::backward;
        }
        case roll::nearest: {
            return moving_forward ? roll::backward : roll::forward;
        }
        default: {
            return r;
        }
    }
}

}

}

#endif
