#ifndef CPPALLY_R_LENGTH_H
#define CPPALLY_R_LENGTH_H

#include <cppally/r_vec.h>
#include <cppally/r_visit.h>

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

inline r_size_t length(const r_sexp& x) {
    return CPPALLY_VIEW_AND_APPLY(x, /*return_type = */ r_size_t, /*fn = */ length);
}

// template <RVal T>
// r_vec<r_int> r_vec<T>::lengths() const requires is<T, r_sexp> {
//     r_size_t n = length();
//     r_vec<r_int> out(n);
  
//     for (r_size_t i = 0; i < n; ++i){
//       r_size_t len = length(view(i));
//       if (len > unwrap(r_limits<r_int>::max())) [[unlikely]] {
//         abort("`lengths()` does not currently support long-vectors");
//       }
//       out.set(i, r_int(static_cast<int>(len)));
//     }
//     return out;
// }

}

  #endif

