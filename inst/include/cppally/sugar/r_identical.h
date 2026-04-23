#ifndef CPPALLY_R_IDENTICAL_H
#define CPPALLY_R_IDENTICAL_H

#include <cppally/r_vec.h>
#include <cppally/r_visit.h>
#include <cppally/r_attrs.h>

namespace cppally {

namespace internal {

// identical checks that a and b are exactly the same
// Always returns true if they are both the same NA
// needed for hash equality

template <typename T>
inline bool identical_impl(const T& a, const T& b) {
    if constexpr (RVal<T>){
        return unwrap(a) == unwrap(b);
    } else if constexpr (CastableToRScalar<T>){
        using r_t = as_r_scalar_t<T>;
        return identical_impl<r_t>(r_t(a), r_t(b));
    } else {
        return a == b;
    }
}

template<>
inline bool identical_impl<r_sym>(const r_sym& a, const r_sym& b) {
    return unwrap(a) == unwrap(b);
}

template<>
inline bool identical_impl<r_dbl>(const r_dbl& a, const r_dbl& b) {
    const double x = unwrap(a);
    const double y = unwrap(b);

    bool eq = x == y;

    if (eq) return true;

    // If both (NA or NaN)
    if (is_na(a) && is_na(b)){
        return is_na_real(x) == is_na_real(y);
    } else {
        return eq;
    }
}

template<>
inline bool identical_impl<r_cplx>(const r_cplx& a, const r_cplx& b) {
    return identical_impl(a.re(), b.re()) && identical_impl(a.im(), b.im());
}

template <RVector T>
inline bool identical_impl(const T& a, const T& b) {
    SEXP x = unwrap(a);
    SEXP y = unwrap(b);
    if (x == y) return true; // same pointer
    if (a.length() != b.length()) return false;
    if (TYPEOF(a) != TYPEOF(b)) return false;
    
    bool x_has_attrs = attr::has_attrs(a);
    bool y_has_attrs = attr::has_attrs(b);
    if (x_has_attrs != y_has_attrs) return false;
    
    if (x_has_attrs && y_has_attrs){
        r_vec<r_sexp> a_attrs = attr::get_attrs(a);
        r_vec<r_sexp> b_attrs = attr::get_attrs(b);

        if (a_attrs.length() != b_attrs.length()) return false;
        if (!identical_impl(a_attrs.names(), b_attrs.names())) return false;

        // Only do the rest of the attr checks if pointers do not match
        if (unwrap(a_attrs) != unwrap(b_attrs)){
            r_vec<r_str_view> names1 = a_attrs.names();
            r_vec<r_str_view> names2 = b_attrs.names();
            if (!identical_impl(names1, names2)) return false;

            for (r_size_t i = 0; i < a_attrs.length(); ++i){
                if (!identical_impl(a_attrs.view(i), b_attrs.view(i))) return false;
            }
        }
    }

    r_size_t n = a.length();
    for (r_size_t i = 0; i < n; ++i){
        if (!identical_impl(a.view(i), b.view(i))){
            return false;
        }
    } 
    return true;
}

template<>
inline bool identical_impl<r_factors>(const r_factors& a, const r_factors& b) {
    return identical_impl(a.value, b.value);
}

template <>
inline bool identical_impl<r_sexp>(const r_sexp& a, const r_sexp& b) {
    SEXP x = unwrap(a);
    SEXP y = unwrap(b);
    if (x == y) return true; // same pointer
    
    // Visit both SEXP
    return view_sexp(a, [&b](const auto& vec1) -> bool {
        using vec1_t = decltype(vec1);

        if constexpr (is<vec1_t, r_sexp>){
            return R_compute_identical(vec1, b, 16);
        } else {
            return view_sexp(b, [&vec1](const auto& vec2) -> bool {
                using vec2_t = decltype(vec2);

                if constexpr (!is<vec1_t, vec2_t>){
                    return false;
                } else {
                    return identical_impl(vec1, vec2);
                }
            });
        }
        });
}

template<>
inline bool identical_impl<SEXP>(const SEXP& a, const SEXP& b) {
    return identical_impl<r_sexp>(r_sexp(a, view_tag{}), r_sexp(b, view_tag{}));
}

}

// Identical takes type into account
template <typename T, typename U>
inline constexpr bool identical(const T& a, const U& b) {
    if constexpr (is<T, U>){
        return internal::identical_impl<T>(a, b);
    } else {
        return false;
    }
}

}

#endif
