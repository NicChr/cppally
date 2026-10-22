// Portable log-factorial

#ifndef CPPALLY_LOG_FACTORIAL_H
#define CPPALLY_LOG_FACTORIAL_H

#include <cppally/math/portable_log.h>

namespace cppally {

namespace internal {

// Hormann's fc(k) = log(k!) - Stirling's approximation, exact table for k <= 9
inline double stirling_tail(double k) noexcept {
  static constexpr double tail[10] = {
    0x1.4c071bcda0a5bp-4, 0x1.52a9b923ea649p-5, 0x1.c579a268d80b3p-6, 0x1.54a2662fd78a9p-6,
    0x1.10b4e513fcbedp-6, 0x1.c6b167bebdf36p-7, 0x1.85d4d612e4a86p-7, 0x1.552805e7b3076p-7,
    0x1.2f4871b12ab64p-7, 0x1.10f9d4c0743a7p-7
  };
  if (k <= 9.0) {
    return tail[static_cast<int>(k)];
  }
  double kp1sq = (k + 1.0) * (k + 1.0);
  return (1.0 / 12.0 - (1.0 / 360.0 - 1.0 / 1260.0 / kp1sq) / kp1sq) / (k + 1.0);
}

// log(k!) = (k + 0.5) log(k + 1) - (k + 1) + log(2 pi) / 2 + fc(k)
inline double log_factorial(double k) noexcept {
  double kp1 = k + 1.0;
  return internal::fma(k + 0.5, portable_log(kp1), 0x1.d67f1c864beb5p-1 - kp1) + stirling_tail(k);
}

}

}

#endif
