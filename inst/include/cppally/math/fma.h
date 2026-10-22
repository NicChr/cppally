// Portable fused-multiply-add

#ifndef CPPALLY_FMA_H
#define CPPALLY_FMA_H

#include <cmath>

namespace cppally {

namespace internal {

//  On x86-64 without -mfma the CPU may lack FMA, so FMA code lives in
// functions tagged CPPALLY_TARGET_FMA and is only called after checking cpu_has_fma.
// Everywhere else the build already fixes the fma, so the tag is empty and the flag always true
#if defined(__x86_64__) && !defined(__FMA__) && (defined(__GNUC__) || defined(__clang__))
#define CPPALLY_TARGET_FMA [[gnu::target("fma")]]
inline const bool cpu_has_fma = (__builtin_cpu_init(), __builtin_cpu_supports("fma"));
#else
#define CPPALLY_TARGET_FMA
inline constexpr bool cpu_has_fma = true;
#endif

// Whatever fma the enclosing function is compiled for: the instruction inside CPPALLY_TARGET_FMA functions, else libm's.
// Only for code reached after checking cpu_has_fma - anywhere else use internal::fma
[[gnu::always_inline]] inline double builtin_fma(double x, double y, double z) noexcept {
#if defined(__GNUC__) || defined(__clang__)
  return __builtin_fma(x, y, z);
#else
  return std::fma(x, y, z);
#endif
}

CPPALLY_TARGET_FMA inline double fma_hw(double x, double y, double z) noexcept {
  return builtin_fma(x, y, z);
}

// Safe anywhere: correctly rounded on any CPU with FMA, libm's fma otherwise
inline double fma(double x, double y, double z) noexcept {
  return cpu_has_fma ? fma_hw(x, y, z) : std::fma(x, y, z);
}

}

}

#endif
