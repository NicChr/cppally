#ifndef CPPALLY_R_SCALAR_METHODS_H
#define CPPALLY_R_SCALAR_METHODS_H

#include <cppally/r_setup.h>
#include <cppally/r_concepts.h>
#include <cppally/r_types.h>
#include <cppally/r_nas.h>
#include <cstring> // For strcmp

namespace cppally {

// Methods for custom R types

// operators for r_lgl
inline constexpr r_lgl operator!(r_lgl x) {
  return is_na(x) ? r_na : r_lgl(x.value == 0);
}

// r_true = 1, r_false = 0, r_na = INT_MIN

// ---------------------------------------------------------
// OPTIMIZED OR (||) for r_lgl
// If LSB is set (1), return 1
// Otherwise return (a|b).
// ---------------------------------------------------------
inline constexpr r_lgl operator||(r_lgl lhs, r_lgl rhs) {
    int val = lhs.value | rhs.value;
    return (val & 1) ? r_true : r_lgl(val);
}

// ---------------------------------------------------------
// OPTIMIZED AND (&&) for r_lgl
// If either is 0, return 0.
// if either is NA (negative), return NA.
// otherwise return 1.
// ---------------------------------------------------------
inline constexpr r_lgl operator&&(r_lgl lhs, r_lgl rhs) {
    if (lhs.value == 0 || rhs.value == 0) {
        return r_false;
    }
    return (lhs.value | rhs.value) < 0 ? r_na : r_true;
}

// Operators for r_str_view

inline r_lgl operator<(r_str_view lhs, r_str_view rhs) {
  if (is_na(lhs) || is_na(rhs)){
    return r_na;
  } else if (unwrap(lhs) == unwrap(rhs)){
    return r_false;
  } else {
    return r_lgl{std::strcmp(lhs.c_str(), rhs.c_str()) < 0};
  }
}
inline r_lgl operator<=(r_str_view lhs, r_str_view rhs) {
  if (is_na(lhs) || is_na(rhs)){
    return r_na;
  } else if (unwrap(lhs) == unwrap(rhs)){
    return r_true;
  } else {
    return r_lgl{std::strcmp(lhs.c_str(), rhs.c_str()) < 0};
  }
}
inline r_lgl operator>(r_str_view lhs, r_str_view rhs) {
  if (is_na(lhs) || is_na(rhs)){
    return r_na;
  } else if (unwrap(lhs) == unwrap(rhs)){
    return r_false;
  } else {
    return r_lgl{std::strcmp(lhs.c_str(), rhs.c_str()) > 0};
  }
}
inline r_lgl operator>=(r_str_view lhs, r_str_view rhs) {
  if (is_na(lhs) || is_na(rhs)){
    return r_na;
  } else if (unwrap(lhs) == unwrap(rhs)){
    return r_true;
  } else {
    return r_lgl{std::strcmp(lhs.c_str(), rhs.c_str()) > 0};
  }
}

inline r_lgl operator<(const r_str& lhs, const r_str& rhs) {
  return static_cast<r_str_view>(lhs) < static_cast<r_str_view>(rhs);
}
inline r_lgl operator<=(const r_str& lhs, const r_str& rhs) {
  return static_cast<r_str_view>(lhs) <= static_cast<r_str_view>(rhs);
}
inline r_lgl operator>(const r_str& lhs, const r_str& rhs) {
  return static_cast<r_str_view>(lhs) > static_cast<r_str_view>(rhs);
}
inline r_lgl operator>=(const r_str& lhs, const r_str& rhs) {
  return static_cast<r_str_view>(lhs) >= static_cast<r_str_view>(rhs);
}

inline r_lgl operator==(const r_sexp& lhs, const r_sexp& rhs) {
  return r_lgl{unwrap(lhs) == unwrap(rhs)};
}
// inline r_lgl operator!=(const r_sexp& lhs, const r_sexp& rhs) {
//   return r_lgl{unwrap(lhs) != unwrap(rhs)};
// }

inline r_lgl operator==(r_sym lhs, r_sym rhs) {
  return r_lgl{unwrap(lhs) == unwrap(rhs)};
}
inline r_lgl operator!=(r_sym lhs, r_sym rhs) {
  return r_lgl{unwrap(lhs) != unwrap(rhs)};
}

template <RScalar T, RScalar U>
inline constexpr r_lgl operator==(const T &lhs, const U &rhs) {
  return (is_na(lhs) || is_na(rhs)) ? r_na : r_lgl{unwrap(lhs) == unwrap(rhs)};
}

template <RScalar T, CppScalar U>
inline constexpr r_lgl operator==(const T &lhs, const U &rhs) {
  return is_na(lhs) ? r_na : r_lgl{unwrap(lhs) == rhs};
}

template <CppScalar T, RScalar U>
inline constexpr r_lgl operator==(const T &lhs, const U &rhs) {
  return is_na(rhs) ? r_na : r_lgl{lhs == unwrap(rhs)};
}

// Need to have 3 overloads otherwise compiler complains about lhs != rhs

template <RScalar T, RScalar U>
inline constexpr r_lgl operator!=(const T &lhs, const U &rhs) {
  return (is_na(lhs) || is_na(rhs)) ? r_na : r_lgl{unwrap(lhs) != unwrap(rhs)};
}

template <RScalar T, CppScalar U>
inline constexpr r_lgl operator!=(const T &lhs, const U &rhs) {
  return is_na(lhs) ? r_na : r_lgl{unwrap(lhs) != rhs};
}

template <CppScalar T, RScalar U>
inline constexpr r_lgl operator!=(const T &lhs, const U &rhs) {
  return is_na(rhs) ? r_na : r_lgl{lhs != unwrap(rhs)};
}

template <MathType T, MathType U>
requires (RMathType<T> || RMathType<U>)
inline constexpr r_lgl operator<(T lhs, U rhs) {
  return (is_na(lhs) || is_na(rhs)) ? r_na : r_lgl{unwrap(lhs) < unwrap(rhs)};
}
template <MathType T, MathType U>
requires (RMathType<T> || RMathType<U>)
inline constexpr r_lgl operator<=(T lhs, U rhs) {
  return (is_na(lhs) || is_na(rhs)) ? r_na : r_lgl{unwrap(lhs) <= unwrap(rhs)};
}
template <MathType T, MathType U>
requires (RMathType<T> || RMathType<U>)
inline constexpr r_lgl operator>(T lhs, U rhs) {
  return (is_na(lhs) || is_na(rhs)) ? r_na : r_lgl{unwrap(lhs) > unwrap(rhs)};
}
template <MathType T, MathType U>
requires (RMathType<T> || RMathType<U>)
inline constexpr r_lgl operator>=(T lhs, U rhs) {
  return (is_na(lhs) || is_na(rhs)) ? r_na : r_lgl{unwrap(lhs) >= unwrap(rhs)};
}

template<NumericType T, NumericType U>
  requires (RNumericType<T> || RNumericType<U>)
inline constexpr auto operator+(T lhs, U rhs) {

  using common_t = common_math_t<T, U>;

  if constexpr (is<common_t, r_dbl>){
    return r_dbl(static_cast<double>(unwrap(lhs)) + static_cast<double>(unwrap(rhs)));
  } else {
    return ( is_na(lhs) || is_na(rhs) ) ? 
    na<common_t>() : 
    common_t(static_cast<unwrap_t<common_t>>(unwrap(lhs)) + static_cast<unwrap_t<common_t>>(unwrap(rhs)));
  }
}

template<NumericType T, NumericType U>
  requires (RNumericType<T> || RNumericType<U>)
inline constexpr auto operator-(T lhs, U rhs) {

  using common_t = common_math_t<T, U>;

  if constexpr (is<common_t, r_dbl>){
    return r_dbl(static_cast<double>(unwrap(lhs)) - static_cast<double>(unwrap(rhs)));
  } else {
    return ( is_na(lhs) || is_na(rhs) ) ? 
    na<common_t>() : 
    common_t(static_cast<unwrap_t<common_t>>(unwrap(lhs)) - static_cast<unwrap_t<common_t>>(unwrap(rhs)));
  }
}

template<NumericType T, NumericType U>
  requires (RNumericType<T> || RNumericType<U>)
inline constexpr auto operator*(T lhs, U rhs) {

  using common_t = common_math_t<T, U>;

  if constexpr (is<common_t, r_dbl>){
    return r_dbl(static_cast<double>(unwrap(lhs)) * static_cast<double>(unwrap(rhs)));
  } else {
    return ( is_na(lhs) || is_na(rhs) ) ? 
    na<common_t>() : 
    common_t(static_cast<unwrap_t<common_t>>(unwrap(lhs)) * static_cast<unwrap_t<common_t>>(unwrap(rhs)));
  }
}

template<NumericType T, NumericType U>
  requires (RNumericType<T> || RNumericType<U>)
inline constexpr r_dbl operator/(T lhs, U rhs) {
  return ( is_na(lhs) || is_na(rhs) ) ? na<r_dbl>() : r_dbl(static_cast<double>(unwrap(lhs)) / static_cast<double>(unwrap(rhs)));
}

template<MathType T, MathType U>
  requires (RFloatType<T> || RFloatType<U>)
inline constexpr r_dbl operator%(T lhs, U rhs) {
  if (unwrap(rhs) == 0){
    return r_dbl(R_NaN);
  } else if (is_na(lhs) || is_na(rhs)){
    return na<r_dbl>();
  } else {
    // Donald Knuth floor division
    double a = static_cast<double>(unwrap(lhs));
    double b = static_cast<double>(unwrap(rhs));
    double q = std::floor(a / b);
    return r_dbl(a - (b * q));
  }
}

template<IntegerType T, IntegerType U>
  requires (RIntegerType<T> || RIntegerType<U>)
inline constexpr auto operator%(T lhs, U rhs) {
  using out_t = common_math_t<T, U>;
  using unwrapped_t = unwrap_t<out_t>;

  if ( unwrap(rhs) == 0 || is_na(lhs) || is_na(rhs) ){
    return na<out_t>();
  } else {
    unwrapped_t a = static_cast<unwrapped_t>(unwrap(lhs));
    unwrapped_t b = static_cast<unwrapped_t>(unwrap(rhs));
    unwrapped_t out = a % b;
    if (out != 0 && ((a > 0) != (b > 0))) {
      out += b;  // Adjust to match R's sign convention
    }
    return out_t(out);
  }
}

template <RNumericType T, NumericType U>
inline constexpr T& operator+=(T &lhs, U rhs) {
  if (is_na(lhs) || is_na(rhs)) {
    lhs = na<T>();
  } else {
    lhs.value += unwrap(rhs);
  }
  return lhs;
}

// Fast specialisation for r_dbl
template<>
inline constexpr r_dbl& operator+=(r_dbl &lhs, r_dbl rhs) {
  lhs.value += rhs.value;
  return lhs;
}

template <RNumericType T, NumericType U>
inline constexpr T& operator-=(T &lhs, U rhs) {
  if (is_na(lhs) || is_na(rhs)) {
    lhs = na<T>();
  } else {
    lhs.value -= unwrap(rhs);
  }
  return lhs;
}

template<>
inline constexpr r_dbl& operator-=(r_dbl &lhs, r_dbl rhs) {
  lhs.value -= rhs.value;
  return lhs;
}

template <RNumericType T, NumericType U>
inline constexpr T& operator*=(T &lhs, U rhs) {
  if (is_na(lhs) || is_na(rhs)) {
    lhs = na<T>();
  } else {
    lhs.value *= unwrap(rhs);
  }
  return lhs;
}
template<>
inline constexpr r_dbl& operator*=(r_dbl &lhs, r_dbl rhs) { 
  lhs.value *= rhs.value;
  return lhs;
}

template <RNumericType T, NumericType U>
inline constexpr T& operator/=(T &lhs, U rhs) {
  if (is_na(lhs) || is_na(rhs)) {
    lhs = na<T>();
  } else {
    lhs.value /= unwrap(rhs);
  }
  return lhs;
}

template<>
inline constexpr r_dbl& operator/=(r_dbl &lhs, r_dbl rhs) {
  lhs.value /= rhs.value;
  return lhs;
}

template <MathType T, MathType U>
requires (is<unwrap_t<T>, unwrap_t<U>>)
inline constexpr T& operator%=(T &lhs, U rhs) {
  auto res = lhs % rhs;
  if constexpr (RMathType<T>){
    lhs.value = static_cast<unwrap_t<T>>(unwrap(res));
  } else {
    lhs = static_cast<unwrap_t<T>>(unwrap(res));
  }
  return lhs;
}

template <RMathType T>
inline constexpr T operator-(T x) {
  return is_na(x) ? x : T{-unwrap(x)};
}
template<>
inline constexpr r_dbl operator-(r_dbl x) {
  return r_dbl{-unwrap(x)};
}

}

#endif
