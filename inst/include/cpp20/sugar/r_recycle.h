#ifndef CPP20_R_RECYCLE_H
#define CPP20_R_RECYCLE_H

#include <cpp20/r_visit.h>
#include <cpp20/sugar/r_make_vec.h>

namespace cpp20 {

// Variadic recycle function


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

inline r_vec<r_sexp> recycle_impl(const r_vec<r_sexp>& x) {
    r_size_t n = x.length();
    r_vec<r_sexp> out(n);

    r_size_t common_size = recycle_size(x);

    for (r_size_t i = 0; i < n; ++i){
            out.set(
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
    return out;
}

}

template <typename... Args>
inline r_vec<r_sexp> recycle(Args&&... args) {
  r_vec<r_sexp> r_args = make_vec<r_sexp>(std::forward<Args>(args)...);
  return internal::recycle_impl(r_args);
}

}

#endif
