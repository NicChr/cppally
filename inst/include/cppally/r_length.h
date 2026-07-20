#ifndef CPPALLY_R_LENGTH_H
#define CPPALLY_R_LENGTH_H

#include <cppally/r_vec.h>
#include <cppally/r_factor.h>
#include <cppally/r_df.h>

namespace cppally {

template <RVector T>
inline r_size_t length(const T& x) noexcept {
    return x.length();
}

inline r_size_t length(const r_factors& x) noexcept {
    return x.value.length();
}

inline r_size_t length(const r_df& x) noexcept {
    return x.nrow();
}

inline r_size_t length(const r_sym& x) noexcept {
    return 1;
}

inline r_size_t length(const r_function& x) noexcept {
    return 1;
}

inline r_size_t length(const r_sexp& x);

inline bool is_long(SEXP x){
    return length(r_sexp(x, internal::view_tag{})) > static_cast<r_size_t>(std::numeric_limits<int>::max());
}

}

  #endif

