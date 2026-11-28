#ifndef CPPALLY_TZ_FWD_H
#define CPPALLY_TZ_FWD_H

#include <chrono>

// The parts of tz.h that code outside the one compiled implementation unit needs (see zone_impl.h)

namespace cppally {

namespace internal {

namespace tz {

// Time points of local wall-clock readings (date::local_t)
struct local_t {};
template <class Duration>
using local_time = std::chrono::time_point<local_t, Duration>;
using local_seconds = local_time<std::chrono::seconds>;
using local_days = local_time<std::chrono::days>;

class time_zone;

}

}

}

#endif
