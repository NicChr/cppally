#ifndef CPPALLY_ROLL_H
#define CPPALLY_ROLL_H

#include <cstdint>

namespace cppally {

// Rollover policy for dates & date-times that either don't exist (30 Feb) or are ambiguous (DST transitions).
enum class roll : uint8_t {
    none = 0,       // impossible and ambiguous results are returned as NA
    backward = 1,   // month arithmetic - last day of the current month, ambiguous date-times - the earlier of the two, non-existent date-times - shifted back by the size of the gap
    forward = 2,    // month arithmetic - first day of the next month, ambiguous date-times - the later of the two, non-existent date-times - shifted forward by the size of the gap
    away = 3,       // rolls forward when adding and backward when subtracting
    nearest = 4,    // rolls backward when adding and forward when subtracting
    boundary = 5    // rolls (DST) transitions to the boundary
};

namespace internal {

constexpr roll resolve_roll(roll r, bool forward) noexcept {
    switch (r) {
        case roll::away: {
            return forward ? roll::forward : roll::backward;
        }
        case roll::nearest: {
            return forward ? roll::backward : roll::forward;
        }
        default: {
            return r;
        }
    }
}

}

}

#endif
