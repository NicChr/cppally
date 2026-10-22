// Portable fused-multiply-add

#ifndef CPPALLY_FMA_H
#define CPPALLY_FMA_H

#include <cmath>

namespace cppally {

namespace internal {

#if defined(__x86_64__) && !defined(__FMA__) && (defined(__GNUC__) || defined(__clang__))

[[gnu::target("fma")]] inline double fma_hw(double x, double y, double z) noexcept {
  return __builtin_fma(x, y, z);
}

inline const bool cpu_has_fma = (__builtin_cpu_init(), __builtin_cpu_supports("fma"));

// Correctly rounded if CPU has FMA, othewise libm fma
inline double fma(double x, double y, double z) noexcept {
  return cpu_has_fma ? fma_hw(x, y, z) : std::fma(x, y, z);
}

#else

inline double fma(double x, double y, double z) noexcept {
  return std::fma(x, y, z);
}

#endif

}

}

#endif
