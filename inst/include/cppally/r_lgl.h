#ifndef CPPALLY_R_LGL_H
#define CPPALLY_R_LGL_H

#include <cppally/r_setup.h>
#include <cppally/r_concepts.h>
#include <cppally/r_protect.h>
#include <limits>

namespace cppally {

// R bool type with 3 states, similar to Rboolean.
// Can only implicitly convert to bool in if statements.
// If during implicit conversion to bool, an NA is detected, an error is thrown.
// It can implicitly coerce to int.
struct r_lgl {
  int value;
  using value_type = int;
  constexpr r_lgl() noexcept : value{0} {}
  explicit constexpr r_lgl(int x) noexcept : value{(static_cast<unsigned int>(x) * 2u) != 0u ? 1 : x} {}
  explicit constexpr r_lgl(bool x) noexcept : value{static_cast<int>(x)} {}
  template <typename U> requires (is<U, int>)
  constexpr operator U() const noexcept { return value; }

  static constexpr r_lgl na() noexcept {
    return r_lgl(std::numeric_limits<int>::min());
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
inline constexpr r_lgl na_lgl = r_lgl::na();
}

}

#endif
