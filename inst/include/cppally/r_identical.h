#ifndef CPPALLY_R_IDENTICAL_H
#define CPPALLY_R_IDENTICAL_H

#include <cppally/r_vec.h>
#include <cppally/r_attrs.h>

namespace cppally {

namespace internal {

// identical checks that a and b are exactly the same
// Always returns true if they are both the same NA
// needed for hash equality

template <RScalar T>
inline bool identical_impl(const T& a, const T& b) noexcept {
    if constexpr (RScalar<typename T::value_type>){
        return identical_impl(a.value, b.value);
      } else {
        return unwrap(a) == unwrap(b);
      }
}

template <CastableToRScalar T>
requires (CppType<T>)
inline bool identical_impl(const T& a, const T& b) noexcept {
    using r_t = as_r_scalar_t<T>;
    return identical_impl(r_t(a), r_t(b));
}

template<>
inline bool identical_impl<r_dbl>(const r_dbl& a, const r_dbl& b) noexcept {
    const double x = unwrap(a);
    const double y = unwrap(b);

    bool eq = x == y;

    if (eq) return true;

    // If both (NA or NaN)
    if (is_na(a) && is_na(b)){
        return has_na_real_payload(x) == has_na_real_payload(y);
    } else {
        return eq;
    }
}

template<>
inline bool identical_impl<r_cplx>(const r_cplx& a, const r_cplx& b) noexcept {
    return identical_impl(a.re(), b.re()) && identical_impl(a.im(), b.im());
}

inline bool identical_impl(const r_sym& a, const r_sym& b) noexcept {
    return unwrap(a) == unwrap(b);
}

inline bool identical_impl(const r_function& a, const r_function& b) noexcept {
    return unwrap(a) == unwrap(b);
}

inline bool identical_impl(const r_sexp& a, const r_sexp& b);

template <RComposite T>
inline bool identical_impl(const T& a, const T& b);

template <RVector T>
inline bool identical_impl(const T& a, const T& b);

template<>
inline bool identical_impl<r_factors>(const r_factors& a, const r_factors& b);

template<>
inline bool identical_impl<r_df>(const r_df& a, const r_df& b);

}

// Identical takes type into account
template <typename T, typename U>
inline constexpr bool identical(const T& a, const U& b) noexcept(RScalar<T>) {
    if constexpr (is<T, U>){
        return internal::identical_impl(a, b);
    } else {
        return false;
    }
}

}

#endif
