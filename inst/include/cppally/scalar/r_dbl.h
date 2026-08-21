#ifndef CPPALLY_R_DBL_H
#define CPPALLY_R_DBL_H

#include <cppally/r_concepts.h>
#include <limits>
#include <bit> // for std::bit_cast

namespace cppally {

namespace internal {

// Important: assumes x is already NA (via x != x)
// Matches R's R_IsNA: a NaN carrying NA_real_'s payload (1954) in its low word.
// Checks the payload rather than the full bit pattern so NA survives FP operations
// (add/sub/mul, negation) that quiet the signaling NaN or flip its sign bit.
constexpr bool has_na_real_payload(double x) noexcept {
  return (std::bit_cast<uint64_t>(x) & 0xFFFFFFFFULL) == 1954ULL;
}

}

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

  static constexpr r_dbl nan() noexcept {
    constexpr double q = std::numeric_limits<double>::quiet_NaN();
    // If this NaN happens to contain 1954, convert it to 1955
    if constexpr (internal::has_na_real_payload(q)) {
      return r_dbl(std::bit_cast<double>(std::bit_cast<uint64_t>(q) ^ 1ULL));
    } else {
      return r_dbl(q);
    }
  }

  static constexpr r_dbl inf() noexcept {
    return r_dbl(std::numeric_limits<double>::infinity());
  }

  constexpr bool is_na() const noexcept {
    return value != value;
  }

  constexpr bool is_infinite() const noexcept {
    return constexpr_fabs(value) == static_cast<double>(inf());
  }

  constexpr bool is_finite() const noexcept {
    return constexpr_fabs(value) < static_cast<double>(inf());
  }

  constexpr bool is_nan() const noexcept {
    return is_na() && !internal::has_na_real_payload(value);
  }

  private: 

  static constexpr double constexpr_fabs(double x) noexcept {
    return x > 0 ? x : -x;
  }

};

}

#endif
