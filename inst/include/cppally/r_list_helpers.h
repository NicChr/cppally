#ifndef CPPALLY_R_LIST_HELPERS_H
#define CPPALLY_R_LIST_HELPERS_H

#include <cppally/r_vec.h>
#include <cppally/sugar/r_sexp_methods.h>
#include <cppally/sugar/r_rep.h>

namespace cppally {

// Recycle helper functions

namespace internal {

inline r_size_t recycle_size(const r_vec<r_sexp>& x){
    r_size_t n = x.length();
    r_size_t out = 0;

    for (r_size_t i = 0; i < n; ++i){
        r_size_t size = length(x.view(i));
        if (size == 0) return 0;
        out = std::max(out, size);
    }
    return out;
}

inline void recycle_impl(r_vec<r_sexp>& x, r_size_t common_size) {
    r_size_t n = x.length();
    for (r_size_t i = 0; i < n; ++i){
        x.set(i, rep_len(x.view(i), common_size));
    }
}

}

inline r_size_t unlisted_length(const r_vec<r_sexp>& x){
    safe[R_CheckStack](); // Check C Stack size isn't close to the limit
    r_size_t n = x.length();
    r_size_t out = 0;
    for (r_size_t i = 0; i < n; ++i){
        out += view_sexp(x.view(i), [&]<typename T>(const T& elem) -> r_size_t {
            if constexpr (is<T, r_vec<r_sexp>>){
                return unlisted_length(elem);
            } else {
                return length(elem);
            }
        });
    }
    return out;
  }

}

#endif
