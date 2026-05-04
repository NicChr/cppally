#ifndef CPPALLY_R_VEC_SUGAR_H
#define CPPALLY_R_VEC_SUGAR_H

// General free functions

#include <cppally/r_vec.h>
#include <cppally/r_attrs.h>
#include <cppally/r_visit.h>
#include <cppally/r_length.h>

namespace cppally {

// Generic SEXP functions

// Equal to `r_null`?
inline bool is_null(SEXP x) noexcept {
    return x == R_NilValue;
}
inline bool is_altrep(SEXP x) noexcept {
    return r_sexp(x, internal::view_tag{}).is_altrep();
}
// Memory address
inline r_str address(SEXP x) {
    return r_sexp(x, internal::view_tag{}).address();
}

// Vector and vector-based functions

inline bool is_long(SEXP x){
    return length(r_sexp(x, internal::view_tag{})) > static_cast<r_size_t>(std::numeric_limits<int>::max());
}

inline r_vec<r_int> lengths(const r_vec<r_sexp>& x){
    return x.lengths();
}
inline r_vec<r_int> lengths(const r_df& x){
    return x.value.lengths();
}

// Fns left to do:
// count
// find
// remove
// replace
// na_count
// any_na
// all_na

}

#endif
