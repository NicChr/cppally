// Portable log

#ifndef CPPALLY_PORTABLE_LOG_H
#define CPPALLY_PORTABLE_LOG_H

#include <cppally/scalar/r_dbl.h>
#include <cppally/scalar/r_limits.h>
#include <cppally/math/fma.h>
#include <bit>
#include <cmath>
#include <cstdint>

namespace cppally {

namespace internal {

inline constexpr double ln2_hi = 0x1.62e42p-1;
inline constexpr double ln2_lo = 0x1.fdf473de6af28p-22;

// Always inlined so portable_log_hw below compiles it with hardware fma
[[gnu::always_inline]] inline double portable_log_kernel(double x) noexcept {

  if (x == 0.0) {
    return -unwrap(r_limits<r_dbl>::max());
  }
  
  if (r_dbl(x).is_na()){
    return x;
  }

  if (x < 0.0 || r_dbl(x).is_infinite()) {
    return x > 0.0 ? x : unwrap(r_dbl::nan());
  }

  // Subnormals are scaled into the normal range first
  int k = 0;
  if (x < 0x1.0p-1022) [[unlikely]] {
    x *= 0x1.0p54;
    k = -54;
  }

  // x = m * 2^k with m in [sqrt(0.5), sqrt(2)): offsetting the bits by sqrt(0.5) makes the exponent tick over there
  constexpr double sqrt_half = 0x1.6a09e667f3bcdp-1;
  uint64_t ix = std::bit_cast<uint64_t>(x) + (std::bit_cast<uint64_t>(1.0) - std::bit_cast<uint64_t>(sqrt_half));
  k += static_cast<int>(ix >> 52) - 1023;
  double m = std::bit_cast<double>((ix & 0x000fffffffffffffULL) + std::bit_cast<uint64_t>(sqrt_half));

  double f = m - 1.0;
  double half_f = 0.5 * f;
  double s = f / (2.0 + f);
  double z = s * s;
  double p = 1.0 / 19.0;
  p = builtin_fma(p, z, 1.0 / 17.0);
  p = builtin_fma(p, z, 1.0 / 15.0);
  p = builtin_fma(p, z, 1.0 / 13.0);
  p = builtin_fma(p, z, 1.0 / 11.0);
  p = builtin_fma(p, z, 1.0 / 9.0);
  p = builtin_fma(p, z, 1.0 / 7.0);
  p = builtin_fma(p, z, 1.0 / 5.0);
  p = builtin_fma(p, z, 1.0 / 3.0);
  double R = 2.0 * z * p;

  double kd = static_cast<double>(k);
  double small = builtin_fma(-half_f, f, builtin_fma(s, builtin_fma(half_f, f, R), kd * ln2_lo));

  double a = kd * ln2_hi;
  double hi = a + f;
  double b = hi - a;
  double err = (a - (hi - b)) + (f - b);
  return hi + (err + small);
}

// The kernel compiled with hardware fma - only call when cpu_has_fma
CPPALLY_TARGET_FMA inline double portable_log_hw(double x) noexcept {
  return portable_log_kernel(x);
}

// Same kernel, compiled with and without FMA instructions
inline double portable_log(double x) noexcept {
  return cpu_has_fma ? portable_log_hw(x) : portable_log_kernel(x);
}

}

}

#endif
