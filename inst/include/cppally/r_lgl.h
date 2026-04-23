#ifndef CPPALLY_R_LGL_H
#define CPPALLY_R_LGL_H

#include <cppally/r_setup.h>
#include <cppally/r_concepts.h>
#include <cppally/r_protect.h>

namespace cppally {

// bool type, similar to Rboolean
// Can only implicitly convert to bool in if statements
// If during implicit conversion, an NA is detected, an error is thrown
// Detect NA manually via the `is_na` member function
struct r_lgl {
  int value;
  using value_type = int;
  r_lgl() : value{0} {}
  explicit constexpr r_lgl(int x) : value{x} {}
  explicit constexpr r_lgl(bool x) : value{x} {}  
  explicit constexpr operator int() const { return value; }

  explicit operator bool() const;
  constexpr bool is_true() const;
  constexpr bool is_false() const;
};

// The 3 possible values of r_lgl
inline constexpr r_lgl r_true{1};
inline constexpr r_lgl r_false{0};
inline constexpr r_lgl r_na{std::numeric_limits<int>::min()};

inline constexpr bool r_lgl::is_true() const {
    return value == 1;
}

inline constexpr bool r_lgl::is_false() const {
    return value == 0;
}

inline r_lgl::operator bool() const {
    if (value == std::numeric_limits<int>::min()) [[unlikely]] {
        abort("Cannot implicitly convert r_lgl NA to bool, please check");
    }
    return static_cast<bool>(value);
}

}

#endif
