#ifndef CPPALLY_R_LIST_HELPERS_H
#define CPPALLY_R_LIST_HELPERS_H

#include <cppally/r_vec.h>
#include <cppally/r_visit.h>

namespace cppally {

// Recycle helper functions

namespace internal {

inline r_size_t recycle_size(const r_vec<r_sexp>& x){
    r_size_t n = x.length();
    r_size_t out = 0;

    for (r_size_t i = 0; i < n; ++i){
        r_size_t size = x.view(i).length();
        if (size == 0) return 0;
        out = std::max(out, size);
    }
    return out;
}

inline void recycle_impl(r_vec<r_sexp>& x, r_size_t common_size) {
    r_size_t n = x.length();

    for (r_size_t i = 0; i < n; ++i){
            x.set(
                i, 
            view_sexp(x.view(i), [&](const auto& vec) -> r_sexp {
                if constexpr (!RVector<decltype(vec)> && !RMetaVector<decltype(vec)>){
                abort("Don't know how to visit this r_sexp!");
                } else {
                return r_sexp(vec.rep_len(common_size), internal::view_tag{});
                }
        })
        );
    }
}

}

}

#endif
