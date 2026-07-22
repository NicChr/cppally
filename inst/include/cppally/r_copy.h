#ifndef CPPALLY_R_COPY_H
#define CPPALLY_R_COPY_H

#include <cppally/r_vec.h>
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

template <RComposite T>
T shallow_copy(const T& x){
    return x.copy();
}

template <RVector T>
inline T deep_copy(const T& x){
    
    T out = x.copy();

    // If list, copy list elements
    if constexpr (is<T, r_vec<r_sexp>>){
        r_size_t n = x.length();
        for (r_size_t i = 0; i < n; ++i){
            out.set(i, deep_copy(x.view(i)));
        }
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
inline r_sexp deep_copy(const r_sexp& x) {
    return internal::view_sexp(x, []<typename vec_t>(const vec_t& vec) -> r_sexp {
        if constexpr (!is<vec_t, r_sexp>){
            return r_sexp(deep_copy(vec));
        } else {
            return r_sexp(safe[Rf_duplicate](vec));
        }
    });
}

template<>
inline r_sexp shallow_copy(const r_sexp& x) {
    return internal::view_sexp(x, []<typename vec_t>(const vec_t& vec) -> r_sexp {
        if constexpr (!is<vec_t, r_sexp>){
            return r_sexp(shallow_copy(vec));
        } else {
            return r_sexp(safe[Rf_shallow_duplicate](vec));
        }
    });
}

template<>
inline r_factors deep_copy(const r_factors& x){
    r_vec<r_int> out = deep_copy(x.value);
    return r_factors(static_cast<r_sexp>(out), internal::no_checks_tag{});
}

template<>
inline r_df deep_copy(const r_df& x){
    return r_df(deep_copy(x.value));
}

// Symbols can't be deep copied
template<>
inline r_sym deep_copy(const r_sym& x){
    return x;
}

// Symbols can't be copied
template<>
inline r_sym shallow_copy(const r_sym& x){
    return x;
}

template<>
inline r_function shallow_copy(const r_function& x){
    return x;
}
template<>
inline r_function deep_copy(const r_function& x){
    return x;
}


}
#endif
