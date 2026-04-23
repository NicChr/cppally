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
inline consteval double make_na_real() {
  return std::bit_cast<double>(0x7ff00000000007a2ULL);
}

inline consteval uint64_t na_real_bits(){
  return std::bit_cast<uint64_t>(make_na_real());
}

inline constexpr bool is_na_real(double x){
  return std::bit_cast<uint64_t>(x) == na_real_bits();
}

// Different NaN have different bit representations, so use with care
inline consteval uint64_t nan_bits(){
  return std::bit_cast<uint64_t>(std::numeric_limits<double>::quiet_NaN());
}

}

namespace internal {

template <RVal T>
inline constexpr T na_value_impl();

template<>
inline constexpr r_lgl na_value_impl<r_lgl>(){
  return r_na;
}

template<>
inline constexpr r_int na_value_impl<r_int>(){
  return r_int(std::numeric_limits<int>::min());
}

template<>
inline constexpr r_dbl na_value_impl<r_dbl>(){
  return r_dbl(make_na_real());
}

template <RTimeType T>
inline constexpr T na_value_impl(){
  return T(na_value_impl<inherited_type_t<T>>());
}

template<>
inline constexpr r_int64 na_value_impl<r_int64>(){
  return r_int64(std::numeric_limits<int64_t>::min());
}

template<>
inline constexpr r_cplx na_value_impl<r_cplx>(){
  return r_cplx(std::complex<double>{make_na_real(), make_na_real()});
}

template<>
inline constexpr r_raw na_value_impl<r_raw>(){
  return r_raw{0};
}

template<>
inline r_str_view na_value_impl<r_str_view>(){
  return na_str;
}

template<>
inline r_str na_value_impl<r_str>(){
  return r_str(unwrap(na_str), internal::view_tag{});
}

template<>
inline r_sexp na_value_impl<r_sexp>(){
  return r_null;
}

}

template<typename T>
inline constexpr auto na(){
  return internal::na_value_impl<std::remove_cvref_t<T>>();
}

template<typename T>
inline constexpr bool is_na(T const& x) {
  if constexpr (RScalar<T>){
    return unwrap(x) == unwrap(na<T>());
  } else if constexpr (CastableToRScalar<T>){
    return is_na(as_r_scalar(x));
  } else {
    return false;
  }
}

template<>
inline constexpr bool is_na(r_dbl const& x) {
  return unwrap(x) != unwrap(x);
}

template<>
inline bool is_na(r_str_view const& x) {
  return unwrap(x) == unwrap(na_str);
}

template<>
inline bool is_na(r_str const& x) {
  return unwrap(x) == unwrap(na_str);
}

template<>
inline constexpr bool is_na(r_cplx const& x){
  return is_na(x.re()) || is_na(x.im());
}

template<>
inline constexpr bool is_na(r_raw const& x){
  return false;
}

template <typename T>
inline constexpr bool is_nan(T const& x){
  return false;
}
// NaN but not NA_REAL
template <>
inline constexpr bool is_nan(r_dbl const& x){
  return is_na(x) && !internal::is_na_real(unwrap(x));
}

}

#endif
