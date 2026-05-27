#ifndef CPPALLY_R_NAS_H
#define CPPALLY_R_NAS_H

#include <cppally/r_setup.h>
#include <cppally/r_types.h>
#include <limits>
#include <bit>

namespace cppally {

// NAs

namespace internal {

// Constructs R's NA_REAL: A Quiet NaN with the payload 1954 (0x7a2).
// Hex: 0x7ff00000 (Exp/Quiet) << 32 | 0x7a2 (Payload).
// Uses std::bit_cast for portable interpretation of the 64-bit pattern
// no manual endianness swapping required
inline consteval double make_na_real() noexcept {
  return std::bit_cast<double>(0x7ff00000000007a2ULL);
}

inline consteval uint64_t na_real_bits() noexcept {
  return std::bit_cast<uint64_t>(make_na_real());
}

inline constexpr bool is_na_real(double x) noexcept {
  return std::bit_cast<uint64_t>(x) == na_real_bits();
}

// Different NaN have different bit representations, so use with care
// Mainly used to normalise NaN which may have different bit representations (excluding NA_real_)
inline consteval uint64_t nan_bits() noexcept {
  return std::bit_cast<uint64_t>(std::numeric_limits<double>::quiet_NaN());
}

}

namespace internal {

template <RVal T>
inline constexpr T na_value_impl() noexcept;

template<>
inline constexpr r_lgl na_value_impl<r_lgl>() noexcept {
  return r_na;
}

template<>
inline constexpr r_int na_value_impl<r_int>() noexcept {
  return r_int(std::numeric_limits<int>::min());
}

template<>
inline constexpr r_dbl na_value_impl<r_dbl>() noexcept {
  return r_dbl(make_na_real());
}

template <RTimeType T>
inline constexpr T na_value_impl() noexcept {
  return T(na_value_impl<inherited_type_t<T>>());
}

template<>
inline constexpr r_int64 na_value_impl<r_int64>() noexcept {
  return r_int64(std::numeric_limits<int64_t>::min());
}

template<>
inline constexpr r_cplx na_value_impl<r_cplx>() noexcept {
  return r_cplx(std::complex<double>{make_na_real(), make_na_real()});
}

template<>
inline constexpr r_raw na_value_impl<r_raw>() noexcept {
  return r_raw{0};
}

template<>
inline r_str_view na_value_impl<r_str_view>() noexcept {
  return na_str;
}

template<>
inline r_str na_value_impl<r_str>() noexcept {
  return r_str(unwrap(na_str), internal::view_tag{});
}

template<>
inline r_sexp na_value_impl<r_sexp>() noexcept {
  return r_null;
}

}

template<typename T>
inline constexpr T na() noexcept {
  return internal::na_value_impl<std::remove_cvref_t<T>>();
}

template<typename T>
inline constexpr bool is_na(T const& x) noexcept {
  if constexpr (RScalar<T>){
    return unwrap(x) == unwrap(na<T>());
  } else if constexpr (CastableToRScalar<T>){
    return is_na(as_r_scalar_t<T>(x));
  } else {
    return false;
  }
}

template<>
inline constexpr bool is_na(r_dbl const& x) noexcept {
  return unwrap(x) != unwrap(x);
}

template<>
inline bool is_na(r_str_view const& x) noexcept {
  return unwrap(x) == unwrap(na_str);
}

template<>
inline bool is_na(r_str const& x) noexcept {
  return unwrap(x) == unwrap(na_str);
}

template<>
inline constexpr bool is_na(r_cplx const& x) noexcept {
  return is_na(x.re()) || is_na(x.im());
}

template<>
inline constexpr bool is_na(r_raw const& x) noexcept {
  return false;
}

template <typename T>
inline constexpr bool is_nan(T const& x) noexcept {
  return false;
}
// NaN but not NA_REAL
template <>
inline constexpr bool is_nan(r_dbl const& x) noexcept {
  return is_na(x) && !internal::is_na_real(unwrap(x));
}

namespace internal {
template <typename T, typename U>
constexpr bool either_na(const T& x, const U& y) noexcept {
  return is_na(x) || is_na(y);
} 
inline constexpr bool either_na(r_dbl x, r_dbl y) noexcept {
  return is_na(r_dbl(unwrap(x) + unwrap(y)));
}
inline constexpr bool either_na(r_lgl x, r_lgl y) noexcept {
  return std::min(unwrap(x), unwrap(y)) == unwrap(na<r_lgl>());
}
inline constexpr bool either_na(r_int x, r_int y) noexcept {
  return std::min(unwrap(x), unwrap(y)) == unwrap(na<r_int>());
}
inline constexpr bool either_na(r_int64 x, r_int64 y) noexcept {
  return std::min(unwrap(x), unwrap(y)) == unwrap(na<r_int64>());
}

}


}

#endif
