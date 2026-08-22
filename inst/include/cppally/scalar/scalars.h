#ifndef CPPALLY_R_SCALARS_H
#define CPPALLY_R_SCALARS_H

// Convenience header to include all scalar type headers

#include <cppally/r_concepts.h>
#include <cppally/scalar/r_lgl.h>
#include <cppally/scalar/r_int.h>
#include <cppally/scalar/r_int64.h>
#include <cppally/scalar/r_dbl.h>
#include <cppally/scalar/r_str.h>
#include <cppally/scalar/r_cplx.h>
#include <cppally/scalar/r_raw.h>
#include <cppally/scalar/r_date.h>
#include <cppally/scalar/r_psxct.h>

// R-based C++ types that closely align with their R equivalents
// Further methods (e.g. operators) are defined in scalar/arithmetic_ops.h and scalar/relational_ops.h
// constructing R types via e.g. r_dbl() r_int() does not account for NAs
// For any and all `NA` safe conversions, use the `as<>` template defined in coerce.h
// For example - to construct an `r_int` from an integer `x`, simply write `r_int(x)`. 
// To convert an integer `x` to an `r_dbl`, we can write `as<r_dbl>(x)`
// The latter case is able to handle `NA` conversions between different types.
// `as<>` is the de-facto tool for conversions between all types in cppally

#endif
