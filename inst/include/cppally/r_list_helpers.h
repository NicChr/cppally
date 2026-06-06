#ifndef CPPALLY_R_LIST_HELPERS_H
#define CPPALLY_R_LIST_HELPERS_H

#include <cppally/r_vec.h>
#include <cppally/sugar/r_sexp_methods.h>
#include <cppally/sugar/r_rep.h>
#include <vector>

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
    r_size_t out = 0;
    std::vector<r_vec<r_sexp>> work;
    work.push_back(x);

    while (!work.empty()){
        r_vec<r_sexp> current = std::move(work.back());
        work.pop_back();
        r_size_t n = current.length();
        for (r_size_t i = 0; i < n; ++i){
            internal::view_sexp(current.view(i), [&]<typename T>(const T& elem){
                if constexpr (is<T, r_vec<r_sexp>>){
                    work.push_back(elem);
                } else {
                    out += length(elem);
                }
            });
        }
    }
    return out;
}

}

#endif
