#ifndef CPPALLY_R_DBL_H
#define CPPALLY_R_DBL_H

#include <cppally/r_concepts.h>

namespace cppally {

// R double
struct r_dbl {
  double value;
  using value_type = double;
  constexpr r_dbl() noexcept : value{0.0} {}
  template <CppMathType T>
  explicit constexpr r_dbl(T x) noexcept : value{static_cast<double>(x)} {}
  constexpr operator double() const noexcept { return value; }
};

}

#endif
