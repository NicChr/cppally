// Portable exp

#ifndef CPPALLY_PORTABLE_EXP_H
#define CPPALLY_PORTABLE_EXP_H

#include <cppally/math/portable_log.h>
#include <bit>

namespace cppally {

namespace internal {

// Always inlined so portable_exp_hw below compiles it with hardware fma
[[gnu::always_inline]] inline double portable_exp_kernel(double y) noexcept {
  if (r_dbl(y).is_na()) {
    return y;
  }
  if (y > 0x1.62e42fefa39efp+9) { // log(DBL_MAX)
    return unwrap(r_limits<r_dbl>::max());
  }
  if (y < -745.2) {
    return 0.0;
  }

  // Adding 1.5 * 2^52 rounds y / log(2) to the nearest integer (ties to even)
  double k = builtin_fma(y, 0x1.71547652b82fep+0, 0x1.8p52) - 0x1.8p52;
  double t = builtin_fma(-k, ln2_hi, y);
  double r = builtin_fma(-k, ln2_lo, t);
  double r_lo = builtin_fma(-k, ln2_lo, t - r);

  double q = 1.0 / 6227020800.0;
  q = builtin_fma(q, r, 1.0 / 479001600.0);
  q = builtin_fma(q, r, 1.0 / 39916800.0);
  q = builtin_fma(q, r, 1.0 / 3628800.0);
  q = builtin_fma(q, r, 1.0 / 362880.0);
  q = builtin_fma(q, r, 1.0 / 40320.0);
  q = builtin_fma(q, r, 1.0 / 5040.0);
  q = builtin_fma(q, r, 1.0 / 720.0);
  q = builtin_fma(q, r, 1.0 / 120.0);
  q = builtin_fma(q, r, 1.0 / 24.0);
  q = builtin_fma(q, r, 1.0 / 6.0);
  q = builtin_fma(q, r, 0.5);

  double small = builtin_fma(r * r, q, builtin_fma(r_lo, r, r_lo));

  double hi = 1.0 + r;
  double lo = (1.0 - hi) + r;
  double v = hi + (lo + small);

  // v is in [0.7, 1.42], so below k = -1021 the result can be subnormal and needs ldexp's rounding
  int64_t ki = static_cast<int64_t>(k);
  if (ki < -1021) [[unlikely]] {
    return std::ldexp(v, static_cast<int>(ki));
  }
  return std::bit_cast<double>(std::bit_cast<uint64_t>(v) + (static_cast<uint64_t>(ki) << 52));
}

// The kernel compiled with hardware fma - only call when cpu_has_fma
CPPALLY_TARGET_FMA inline double portable_exp_hw(double y) noexcept {
  return portable_exp_kernel(y);
}

// Same kernel, compiled with and without FMA instructions
inline double portable_exp(double y) noexcept {
  return cpu_has_fma ? portable_exp_hw(y) : portable_exp_kernel(y);
}

}

}

#endif
