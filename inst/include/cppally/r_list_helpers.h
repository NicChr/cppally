#ifndef CPPALLY_R_LIST_HELPERS_H
#define CPPALLY_R_LIST_HELPERS_H

#include <cppally/r_vec.h>
#include <cppally/r_visit.h>
#include <cppally/r_length.h>
#include <cppally/r_rep.h>

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

}

#endif
