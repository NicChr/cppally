#ifndef CPPALLY_R_NAS_H
#define CPPALLY_R_NAS_H

#include <cppally/r_setup.h>
#include <cppally/scalar/scalars.h>
#include <cppally/r_sym.h>
#include <limits>
#include <bit>

namespace cppally {

// NAs

namespace internal {

template <RVal T>
inline constexpr T na_value_impl() noexcept {
  return T::na();
}

template<>
inline r_sexp na_value_impl<r_sexp>() noexcept {
  return r_null;
}

}

template <typename T>
inline constexpr T na() noexcept {
  return internal::na_value_impl<std::remove_cvref_t<T>>();
}

template<typename T>
inline constexpr bool is_na(const T& x) noexcept {
  if constexpr (RScalar<T>){
    if constexpr (RScalar<typename T::value_type>){
      return x.value.is_na();
    } else {
      return x.is_na();
    }
  } else if constexpr (CastableToRScalar<T>){
    return as_r_scalar_t<T>(x).is_na();
  } else {
    return false;
  }
}

template <typename T>
inline constexpr bool is_nan(const T& x) noexcept {
  return false;
}
// NaN but not NA_REAL
template <>
inline constexpr bool is_nan(const r_dbl& x) noexcept {
  return x.is_nan();
}

// Inspired by SQL COALESCE: returns x, or y if x is NA.
// NOT intended for R's NULL (r_null in cppally).
template<typename T>
requires requires (const T& v) { is_na(v); }
inline constexpr T coalesce(const T& x, const T& y) noexcept {
  return is_na(x) ? y : x;
}

namespace internal {
template <typename T, typename U>
constexpr bool either_na(const T& x, const U& y) noexcept {
  return is_na(x) || is_na(y);
}
template <RIntegerType T>
constexpr bool either_na(const T& x, const T& y) noexcept {
  static_assert(std::numeric_limits<unwrap_t<T>>::min() == unwrap(na<T>()), "`std::numeric_limits<unwrap_t<T>>::min() == unwrap(na<T>())` must hold for all cppally integer types `T`");
  return std::min(unwrap(x), unwrap(y)) == unwrap(na<T>());
}
inline constexpr bool either_na(r_dbl x, r_dbl y) noexcept {
  return is_na(std::abs(unwrap(x)) + std::abs(unwrap(y)));
}

}


}

#endif
