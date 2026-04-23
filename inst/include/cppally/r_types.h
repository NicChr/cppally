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

// Recursively unwrap until we hit a primitive type
template <typename T>
inline constexpr auto unwrap(const T& x){
if constexpr (RVal<T>){
    return unwrap(x.value);
  } else if constexpr (RObject<T>){
    return static_cast<SEXP>(x);
  } else {
    return x;
  }
}

// Coerce C/C++ types to RScalar
template <typename T>
inline constexpr auto as_r_scalar(T const& x) { 
  if constexpr (RScalar<T>){
    return x;
  } else if constexpr (CastableToRScalar<T>){
    return as_r_scalar_t<T>(x);
  } else {
    static_assert(
      always_false<T>,
      "Unsupported type for `as_r_scalar`"
    );
    return r_null;
  } 
}

// Coerce to an RVal type (like as_r_scalar but it can also coerce to `r_sexp`)
// It also coerces objects like `r_vec<T>` to the RVal `r_sexp` 
template <typename T>
inline constexpr auto as_r_val(T const& x) { 
  if constexpr (RVal<T>){
    return x;
  } else if constexpr (CastableToRVal<T>){
    return static_cast<as_r_val_t<T>>(x);
  } else {
    static_assert(
      always_false<T>,
      "Unsupported type for `as_r_val`"
    );
    return r_null;
  } 
}

}

#endif
