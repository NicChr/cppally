#ifndef CPP20_R_COPY_H
#define CPP20_R_COPY_H

#include <cpp20/r_vec.h>
#include <cpp20/r_visit.h>
#include <cpp20/r_attrs.h>

namespace cpp20 {

inline r_sexp shallow_copy(const r_sexp& x){
    return r_sexp(Rf_shallow_duplicate(x)); 
}

template <RVal T>
inline r_vec<T> deep_copy(const r_vec<T>& x){
    
    r_vec<T> out(r_null);
    r_size_t n = x.length();

    if (!x.is_null()){
        out = r_vec<T>(n);

        // If list, copy list elements
        if constexpr (is<T, r_sexp>){
        for (r_size_t i = 0; i < n; ++i){
            out.set(i, deep_copy(x.view(i)));
        }
        } else {
            r_copy_n(out, x, 0, n);
        }    
        auto attrs = attr::get_attrs(x);
        int n_attrs = attrs.length();
        for (int i = 0; i < n_attrs; ++i){
        attrs.set(i, deep_copy(attrs.view(i)));
        }
        attr::set_attrs(out, attrs);
    }
    return out;
}

inline r_factors deep_copy(const r_factors& x){
    r_vec<r_int> out = deep_copy(x.value);
    return r_factors(unwrap(out), false);
}

inline r_sexp deep_copy(const r_sexp& x){
    return visit_sexp(x, [&](auto vec) -> r_sexp {
        if constexpr (!is<decltype(vec), r_sexp>){
            return r_sexp(static_cast<SEXP>(deep_copy(vec)));
        } else {
            return r_sexp(Rf_duplicate(x));
        }
    });
}

}
#endif
