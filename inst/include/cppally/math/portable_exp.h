// Portable exp

#ifndef CPPALLY_PORTABLE_EXP_H
#define CPPALLY_PORTABLE_EXP_H

#include <cppally/math/portable_log.h>

namespace cppally {

namespace internal {

inline double portable_exp(double y) noexcept {
  if (r_dbl(y).is_na()) {
    return y;
  }
  if (y > 0x1.62e42fefa39efp+9) { // log(DBL_MAX)
    return unwrap(r_limits<r_dbl>::max());
  }
  if (y < -745.2) {
    return 0.0;
  }

  double k = std::round(y * 0x1.71547652b82fep+0);
  double t = internal::fma(-k, ln2_hi, y);
  double r = internal::fma(-k, ln2_lo, t);
  double r_lo = internal::fma(-k, ln2_lo, t - r);

  double q = 1.0 / 6227020800.0;
  q = internal::fma(q, r, 1.0 / 479001600.0);
  q = internal::fma(q, r, 1.0 / 39916800.0);
  q = internal::fma(q, r, 1.0 / 3628800.0);
  q = internal::fma(q, r, 1.0 / 362880.0);
  q = internal::fma(q, r, 1.0 / 40320.0);
  q = internal::fma(q, r, 1.0 / 5040.0);
  q = internal::fma(q, r, 1.0 / 720.0);
  q = internal::fma(q, r, 1.0 / 120.0);
  q = internal::fma(q, r, 1.0 / 24.0);
  q = internal::fma(q, r, 1.0 / 6.0);
  q = internal::fma(q, r, 0.5);

  double small = internal::fma(r * r, q, internal::fma(r_lo, r, r_lo));

  double hi = 1.0 + r;
  double lo = (1.0 - hi) + r;
  return std::ldexp(hi + (lo + small), static_cast<int>(k));
}

}

}

#endif
