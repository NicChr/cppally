#ifndef CPPALLY_R_LGL_H
#define CPPALLY_R_LGL_H

#include <cppally/r_setup.h>
#include <cppally/r_concepts.h>
#include <cppally/r_sexp/protect.h>
#include <cppally/scalar/r_int.h>

namespace cppally {

// R bool type with 3 states, similar to Rboolean.
// Can only implicitly convert to bool in if statements.
// If during implicit conversion to bool, an NA is detected, an error is thrown.
// It can implicitly coerce to int.
struct r_lgl {
  int value;
  using value_type = int;
  constexpr r_lgl() noexcept : value{0} {}
  // explicit constexpr r_lgl(int x) noexcept : value{(static_cast<unsigned int>(x) * 2u) != 0u ? 1 : x} {} // Has trouble vectorising on GCC
  explicit constexpr r_lgl(int x) noexcept : value(r_int(x).is_na() ? x : static_cast<int>(static_cast<bool>(x))){}
  explicit constexpr r_lgl(bool x) noexcept : value{static_cast<int>(x)} {}
  template <typename U> requires (is<U, int>)
  constexpr operator U() const noexcept { return value; }

  static constexpr r_lgl na() noexcept {
    constexpr int na_int = r_int::na().value;
    r_lgl out;
    out.value = na_int;
    return out;
  }

  constexpr bool is_true() const noexcept {
    return value == 1;
  }
  constexpr bool is_false() const noexcept {
    return value == 0;
  }
  constexpr bool is_na() const noexcept {
    return value == na().value;
  }

  explicit operator bool() const {
    if (is_na()) [[unlikely]] {
        abort("Cannot implicitly convert r_lgl NA to bool, please check");
    }
    return static_cast<bool>(value);
  }
};

// The 3 possible values of r_lgl
inline constexpr r_lgl r_true{1};
inline constexpr r_lgl r_false{0};
inline constexpr r_lgl r_na = r_lgl::na();

namespace internal {

// Internal fast r_lgl constructor for pre-normalised input
inline constexpr r_lgl new_r_lgl(int x) noexcept {
  r_lgl out;
  out.value = x;
  return out;
}

}

// Logical operators

inline constexpr r_lgl operator!(r_lgl x) noexcept {
  return x.is_na() ? r_lgl::na() : r_lgl(x.value == 0);
}

// r_true = 1, r_false = 0, r_na = INT_MIN

// ---------------------------------------------------------
// OPTIMIZED OR (||) for r_lgl
// If LSB is set (1), return 1
// Otherwise return (a|b).
// ---------------------------------------------------------
inline constexpr r_lgl operator||(r_lgl lhs, r_lgl rhs) noexcept {
    int val = lhs.value | rhs.value;
    return (val & 1) ? r_true : internal::new_r_lgl(val);
}

// ---------------------------------------------------------
// OPTIMIZED AND (&&) for r_lgl
// If either is 0, return 0.
// if either is NA (negative), return NA.
// otherwise return 1.
// ---------------------------------------------------------

inline constexpr r_lgl operator&&(r_lgl lhs, r_lgl rhs) noexcept {
  int a = lhs.value;
  int b = rhs.value;
  int o = a | b;
  int res = (a & b) | ((o & r_na.value) & -(o & 1));
  return internal::new_r_lgl(res);
}

inline constexpr r_lgl operator|(r_lgl lhs, r_lgl rhs) noexcept {
  return lhs || rhs;
}
inline constexpr r_lgl operator&(r_lgl lhs, r_lgl rhs) noexcept {
  return lhs && rhs;
}

}

#endif
