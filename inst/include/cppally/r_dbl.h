#ifndef CPPALLY_R_DBL_H
#define CPPALLY_R_DBL_H

#include <cppally/r_concepts.h>
#include <bit>

namespace cppally {

// R double
struct r_dbl {
  double value;
  using value_type = double;
  constexpr r_dbl() noexcept : value{0.0} {}
  template <CppMathType T>
  explicit constexpr r_dbl(T x) noexcept : value{static_cast<double>(x)} {}
  template <typename U> requires (is<U, double>)
  constexpr operator U() const noexcept { return value; }


  // Constructs R's NA_REAL: a signaling NaN with payload 1954 (0x7a2).
  // Bit 51 (quiet bit) = 0, so technically signaling NaN — a pattern R chose deliberately
  // Hex: 0x7ff00000 (Exp, bit51=0) << 32 | 0x7a2 (Payload).
  static constexpr r_dbl na() noexcept {
    return r_dbl(std::bit_cast<double>(0x7ff00000000007a2ULL));
  }

  constexpr bool is_na() const noexcept {
    return value != value;
  }

};

namespace internal {
inline constexpr r_dbl na_real = r_dbl::na();
}

}

#endif
