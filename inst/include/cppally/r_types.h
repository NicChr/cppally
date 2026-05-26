#ifndef CPPALLY_R_TYPES_H
#define CPPALLY_R_TYPES_H

#include <cppally/r_concepts.h>
#include <cppally/r_sexp.h>
#include <cppally/r_lgl.h>
#include <cppally/r_int.h>
#include <cppally/r_int64.h>
#include <cppally/r_dbl.h>
#include <cppally/r_str.h>
#include <cppally/r_cplx.h>
#include <cppally/r_raw.h>
#include <cppally/r_date.h>
#include <cppally/r_psxct.h>
#include <cppally/r_sym.h>

// R-based C++ types that closely align with their R equivalents
// Further methods (e.g. operators) are defined in r_scalar_methods.h
// constructing R types via e.g. r_dbl() r_int() does not account for NAs
// For any and all `NA` safe conversions, use the `as<>` template defined in r_coerce.h
// For example - to construct an `r_int` from an integer `x`, simply write `r_int(x)`. 
// To convert an integer `x` to an `r_dbl`, we can write `as<r_dbl>(x)`
// The latter case is able to handle `NA` conversions between different types.
// `as<>` is the de-facto tool for conversions between all types in cppally

namespace cppally {

template <typename T>
inline constexpr unwrap_t<T> unwrap(const T& x) noexcept {
  // T must be convertible (implicitly or explicitly) to unwrap_t<T>
  // unwrap_t<> is a well-defined trait which handles the mapping
  // It also asserts that it is a non-throwable construction
  static_assert(std::is_nothrow_constructible_v<unwrap_t<T>, const T&>);
  return static_cast<unwrap_t<T>>(x);
}

}

#endif
