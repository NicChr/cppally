#ifndef CPPALLY_R_LIST_HELPERS_H
#define CPPALLY_R_LIST_HELPERS_H

#include <cppally/r_vec.h>
#include <cppally/sugar/r_sexp_methods.h>
#include <cppally/sugar/r_rep.h>
#include <vector>

namespace cppally {

inline r_vec<r_int> lengths(const r_vec<r_sexp>& x) {
    r_size_t n = x.length();
    r_vec<r_int> out(n);
    out.set_names(x.names());

    for (r_size_t i = 0; i < n; ++i){
        r_size_t len = length(x.view(i));
        if (len > unwrap(r_limits<r_int>::max())) [[unlikely]] {
            abort("`lengths()` does not currently support long-vectors");
        }
        out.set(i, r_int(static_cast<int>(len)));
    }
    return out;
}

// Stack-overflow safe version of `length(unlist(x))`
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
