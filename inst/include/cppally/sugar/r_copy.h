#ifndef CPPALLY_R_COPY_H
#define CPPALLY_R_COPY_H

#include <cppally/r_vec.h>
#include <cppally/r_visit.h>
#include <cppally/r_attrs.h>

namespace cppally {

// Forward declarations

template <typename T>
inline T deep_copy(const T& x) = delete;
template<>
inline r_sexp deep_copy(const r_sexp& x);

template <typename T>
inline T shallow_copy(const T& x) = delete;
template<>
inline r_sexp shallow_copy(const r_sexp& x);

template <RVector T>
inline T deep_copy(const T& x){
    
    if (x.is_null()) return T(r_null);

    r_size_t n = x.length();
    T out(n);

    // If list, copy list elements
    if constexpr (is<T, r_vec<r_sexp>>){
        for (r_size_t i = 0; i < n; ++i){
            out.set(i, deep_copy(x.view(i)));
        }
    } else {
        r_copy_n(out, x, 0, n);
    }    
    if (attr::has_attrs(x)){
        r_vec<r_sexp> attrs = attr::get_attrs(x);
        int n_attrs = attrs.length();
        for (int i = 0; i < n_attrs; ++i){
            attrs.set(i, deep_copy(attrs.view(i)));
        }
        attr::set_attrs(out, attrs);
    }
    return out;
}

template<>
inline r_factors deep_copy(const r_factors& x){
    r_vec<r_int> out = deep_copy(x.value);
    return r_factors(unwrap(out), false);
}

// Symbols can't be deep copied
template<>
inline r_sym deep_copy(const r_sym& x){
    return x;
}

template<>
inline r_sexp deep_copy(const r_sexp& x){
    return view_sexp(x, [](const auto& vec) -> r_sexp {
        if constexpr (!is<decltype(vec), r_sexp>){
            return r_sexp(static_cast<SEXP>(deep_copy(vec)));
        } else {
            return r_sexp(safe[Rf_duplicate](vec));
        }
    });
}

template <RVector T>
inline T shallow_copy(const T& x){
    
    if (x.is_null()) return T(r_null);

    r_size_t n = x.length();
    T out(n);

    // If list, shallow copy list elements
    if constexpr (is<T, r_vec<r_sexp>>){
        for (r_size_t i = 0; i < n; ++i){
            out.set(i, x.view(i));
        }
    } else {
        r_copy_n(out, x, 0, n);
    }    
    attr::set_attrs(out, attr::get_attrs(x));
    return out;
}

template<>
inline r_factors shallow_copy(const r_factors& x){
    r_vec<r_int> out = shallow_copy(x.value);
    return r_factors(unwrap(out), false);
}

// Symbols can't be copied
template<>
inline r_sym shallow_copy(const r_sym& x){
    return x;
}

template<>
inline r_sexp shallow_copy(const r_sexp& x){
    return view_sexp(x, [](const auto& vec) -> r_sexp {
        if constexpr (!is<decltype(vec), r_sexp>){
            return r_sexp(static_cast<SEXP>(shallow_copy(vec)));
        } else {
            return r_sexp(safe[Rf_shallow_duplicate](vec));
        }
    });
}

}
#endif
