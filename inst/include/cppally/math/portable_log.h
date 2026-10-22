// Portable log

#ifndef CPPALLY_PORTABLE_LOG_H
#define CPPALLY_PORTABLE_LOG_H

#include <cppally/scalar/r_dbl.h>
#include <cppally/scalar/r_limits.h>
#include <cppally/math/fma.h>
#include <cmath>
#include <cstdint>

namespace cppally {

namespace internal {

inline constexpr double ln2_hi = 0x1.62e42p-1;
inline constexpr double ln2_lo = 0x1.fdf473de6af28p-22;

inline double portable_log(double x) noexcept {
  if (x == 0.0) {
    return -unwrap(r_limits<r_dbl>::max());
  }
  if (!(x > 0.0) || r_dbl(x).is_infinite()) {
    return x > 0.0 ? x : unwrap(r_dbl::nan());
  }

  int k;
  double m = std::frexp(x, &k);
  if (m < 0x1.6a09e667f3bcdp-1) {
    m *= 2.0;
    --k;
  }

  double f = m - 1.0;
  double half_f = 0.5 * f;
  double s = f / (2.0 + f);
  double z = s * s;
  double p = 1.0 / 19.0;
  p = internal::fma(p, z, 1.0 / 17.0);
  p = internal::fma(p, z, 1.0 / 15.0);
  p = internal::fma(p, z, 1.0 / 13.0);
  p = internal::fma(p, z, 1.0 / 11.0);
  p = internal::fma(p, z, 1.0 / 9.0);
  p = internal::fma(p, z, 1.0 / 7.0);
  p = internal::fma(p, z, 1.0 / 5.0);
  p = internal::fma(p, z, 1.0 / 3.0);
  double R = 2.0 * z * p;

  double kd = static_cast<double>(k);
  double small = internal::fma(-half_f, f, internal::fma(s, internal::fma(half_f, f, R), kd * ln2_lo));

  double a = kd * ln2_hi;
  double hi = a + f;
  double b = hi - a;
  double err = (a - (hi - b)) + (f - b);
  return hi + (err + small);
}

}

}

#endif
