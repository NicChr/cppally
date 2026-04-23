#ifndef CPPALLY_R_DBL_H
#define CPPALLY_R_DBL_H

#include <cppally/r_concepts.h>

namespace cppally {

// R double
struct r_dbl {
  double value;
  using value_type = double;
  r_dbl() : value{0} {}
  template <CppMathType T>
  explicit constexpr r_dbl(T x) : value{static_cast<double>(x)} {}
  constexpr operator double() const { return value; }
};

}

#endif
