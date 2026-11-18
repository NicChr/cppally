#ifndef CPPALLY_ROLL_H
#define CPPALLY_ROLL_H

#include <cstdint>

namespace cppally {

// Rollover policy for dates & date-times that either don't exist (30 Feb) or are ambiguous (DST transitions).
enum class roll : uint8_t {
    none = 0,       // impossible and ambiguous results are returned as NA
    backward = 1,   // last day of the current month, the DST transition, or the earlier of two ambiguous times
    forward = 2,    // first day of the next month, the DST transition, or the later of two ambiguous times
    away = 3,       // rolls forward when adding and backward when subtracting (my favourite)
    nearest = 4     // rolls backward when adding and forward when subtracting
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
