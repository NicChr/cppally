#ifndef CPPALLY_R_SCALAR_OPS_H
#define CPPALLY_R_SCALAR_OPS_H

#include <cppally/r_setup.h>
#include <cppally/r_concepts.h>
#include <cppally/r_types.h>
#include <cppally/r_nas.h>
#include <cstring> // For strcmp

namespace cppally {

// Methods for custom R types

// operators for r_lgl
inline constexpr r_lgl operator!(r_lgl x) noexcept {
  return is_na(x) ? r_na : r_lgl(x.value == 0);
}

// r_true = 1, r_false = 0, r_na = INT_MIN

// ---------------------------------------------------------
// OPTIMIZED OR (||) for r_lgl
// If LSB is set (1), return 1
// Otherwise return (a|b).
// ---------------------------------------------------------
inline constexpr r_lgl operator||(r_lgl lhs, r_lgl rhs) noexcept {
    int val = lhs.value | rhs.value;
    return (val & 1) ? r_true : r_lgl(val);
}

// ---------------------------------------------------------
// OPTIMIZED AND (&&) for r_lgl
// If either is 0, return 0.
// if either is NA (negative), return NA.
// otherwise return 1.
// ---------------------------------------------------------

inline constexpr r_lgl operator&&(r_lgl lhs, r_lgl rhs) noexcept {
  int a = lhs.value;
  int b = rhs.value;
  int o = a | b;
  int res = (a & b) | ((o & r_na.value) & -(o & 1));
  return r_lgl{res};
}

inline constexpr r_lgl operator|(r_lgl lhs, r_lgl rhs) noexcept {
  return lhs || rhs;
}
inline constexpr r_lgl operator&(r_lgl lhs, r_lgl rhs) noexcept {
  return lhs && rhs;
}

// Operators for r_str_view

inline r_lgl operator<(r_str_view lhs, r_str_view rhs) noexcept {
  if (internal::either_na(lhs, rhs)){
    return r_na;
  } else if (unwrap(lhs) == unwrap(rhs)){
    return r_false;
  } else {
    return r_lgl{std::strcmp(lhs.c_str(), rhs.c_str()) < 0};
  }
}
inline r_lgl operator<=(r_str_view lhs, r_str_view rhs) noexcept {
  if (internal::either_na(lhs, rhs)){
    return r_na;
  } else if (unwrap(lhs) == unwrap(rhs)){
    return r_true;
  } else {
    return r_lgl{std::strcmp(lhs.c_str(), rhs.c_str()) < 0};
  }
}
inline r_lgl operator>(r_str_view lhs, r_str_view rhs) noexcept {
  if (internal::either_na(lhs, rhs)){
    return r_na;
  } else if (unwrap(lhs) == unwrap(rhs)){
    return r_false;
  } else {
    return r_lgl{std::strcmp(lhs.c_str(), rhs.c_str()) > 0};
  }
}
inline r_lgl operator>=(r_str_view lhs, r_str_view rhs) noexcept {
  if (internal::either_na(lhs, rhs)){
    return r_na;
  } else if (unwrap(lhs) == unwrap(rhs)){
    return r_true;
  } else {
    return r_lgl{std::strcmp(lhs.c_str(), rhs.c_str()) > 0};
  }
}

inline r_lgl operator<(const r_str& lhs, const r_str& rhs) noexcept {
  return static_cast<r_str_view>(lhs) < static_cast<r_str_view>(rhs);
}
inline r_lgl operator<=(const r_str& lhs, const r_str& rhs) noexcept {
  return static_cast<r_str_view>(lhs) <= static_cast<r_str_view>(rhs);
}
inline r_lgl operator>(const r_str& lhs, const r_str& rhs) noexcept {
  return static_cast<r_str_view>(lhs) > static_cast<r_str_view>(rhs);
}
inline r_lgl operator>=(const r_str& lhs, const r_str& rhs) noexcept {
  return static_cast<r_str_view>(lhs) >= static_cast<r_str_view>(rhs);
}

inline r_lgl operator==(r_sym lhs, r_sym rhs) noexcept {
  return r_lgl{unwrap(lhs) == unwrap(rhs)};
}
inline r_lgl operator!=(r_sym lhs, r_sym rhs) noexcept {
  return r_lgl{unwrap(lhs) != unwrap(rhs)};
}

template <RScalar T, RScalar U>
requires (requires (unwrap_t<T> a, unwrap_t<U> b) { a == b; })
inline constexpr r_lgl operator==(const T& lhs, const U& rhs) noexcept {
  return (internal::either_na(lhs, rhs)) ? r_na : r_lgl{unwrap(lhs) == unwrap(rhs)};
}

template <RScalar T, CppScalar U>
requires (requires (unwrap_t<T> a, unwrap_t<U> b) { a == b; })
inline constexpr r_lgl operator==(const T& lhs, const U& rhs) noexcept {
  return (internal::either_na(lhs, rhs)) ? r_na : r_lgl{unwrap(lhs) == unwrap(rhs)};
}

template <CppScalar T, RScalar U>
requires (requires (unwrap_t<T> a, unwrap_t<U> b) { a == b; })
inline constexpr r_lgl operator==(const T& lhs, const U& rhs) noexcept {
  return (internal::either_na(lhs, rhs)) ? r_na : r_lgl{unwrap(lhs) == unwrap(rhs)};
}

// Need to have 3 overloads otherwise compiler complains about lhs != rhs

template <RScalar T, RScalar U>
requires (requires (unwrap_t<T> a, unwrap_t<U> b) { a != b; })
inline constexpr r_lgl operator!=(const T &lhs, const U &rhs) noexcept {
  return (internal::either_na(lhs, rhs)) ? r_na : r_lgl{unwrap(lhs) != unwrap(rhs)};
}

template <RScalar T, CppScalar U>
requires (requires (unwrap_t<T> a, unwrap_t<U> b) { a != b; })
inline constexpr r_lgl operator!=(const T &lhs, const U &rhs) noexcept {
  return (internal::either_na(lhs, rhs)) ? r_na : r_lgl{unwrap(lhs) != unwrap(rhs)};
}

template <CppScalar T, RScalar U>
requires (requires (unwrap_t<T> a, unwrap_t<U> b) { a != b; })
inline constexpr r_lgl operator!=(const T &lhs, const U &rhs) noexcept {
  return (internal::either_na(lhs, rhs)) ? r_na : r_lgl{unwrap(lhs) != unwrap(rhs)};
}

template <MathType T, MathType U>
requires (RMathType<T> || RMathType<U>)
inline constexpr r_lgl operator<(T lhs, U rhs) noexcept {
  return (internal::either_na(lhs, rhs)) ? r_na : r_lgl{unwrap(lhs) < unwrap(rhs)};
}
template <MathType T, MathType U>
requires (RMathType<T> || RMathType<U>)
inline constexpr r_lgl operator<=(T lhs, U rhs) noexcept {
  return (internal::either_na(lhs, rhs)) ? r_na : r_lgl{unwrap(lhs) <= unwrap(rhs)};
}
template <MathType T, MathType U>
requires (RMathType<T> || RMathType<U>)
inline constexpr r_lgl operator>(T lhs, U rhs) noexcept {
  return (internal::either_na(lhs, rhs)) ? r_na : r_lgl{unwrap(lhs) > unwrap(rhs)};
}
template <MathType T, MathType U>
requires (RMathType<T> || RMathType<U>)
inline constexpr r_lgl operator>=(T lhs, U rhs) noexcept {
  return (internal::either_na(lhs, rhs)) ? r_na : r_lgl{unwrap(lhs) >= unwrap(rhs)};
}

template <MathType T, MathType U>
  requires (RMathType<T> || RMathType<U>)
inline constexpr auto operator+(T lhs, U rhs) noexcept {

  using common_t = common_math_t<T, U>;

  if constexpr (RIntegerType<common_t>){
    using I  = unwrap_t<common_t>;
    using UI = std::make_unsigned_t<I>;

    I a = static_cast<I>(unwrap(lhs));
    I b = static_cast<I>(unwrap(rhs));

    // Wraparound sum via unsigned: defined behaviour, no CPU flags
    I s = static_cast<I>(static_cast<UI>(a) + static_cast<UI>(b));

    // Overflowed iff a and b share a sign that s does not
    bool bad = (((a ^ s) & (b ^ s)) < 0) | internal::either_na(lhs, rhs);
    return bad ? na<common_t>() : common_t(s);
  } else if constexpr (is<T, r_dbl> && is<U, r_dbl>){
    return r_dbl(static_cast<double>(unwrap(lhs)) + static_cast<double>(unwrap(rhs)));
  } else {
    return ( internal::either_na(lhs, rhs) ) ? 
    na<common_t>() : 
    common_t(static_cast<unwrap_t<common_t>>(unwrap(lhs)) + static_cast<unwrap_t<common_t>>(unwrap(rhs)));
  }
}

template <MathType T, MathType U>
  requires (RMathType<T> || RMathType<U>)
inline constexpr auto operator-(T lhs, U rhs) noexcept {

  using common_t = common_math_t<T, U>;

  if constexpr (RIntegerType<common_t>){
    using I  = unwrap_t<common_t>;
    using UI = std::make_unsigned_t<I>;

    I a = static_cast<I>(unwrap(lhs));
    I b = static_cast<I>(unwrap(rhs));

    // Wraparound difference via unsigned: defined behaviour, no CPU flags
    I s = static_cast<I>(static_cast<UI>(a) - static_cast<UI>(b));

    // Overflowed iff a and b differ in sign and s does not share a's sign
    bool bad = (((a ^ b) & (a ^ s)) < 0) | internal::either_na(lhs, rhs);
    return bad ? na<common_t>() : common_t(s);
  } else if constexpr (is<T, r_dbl> && is<U, r_dbl>){
    return r_dbl(static_cast<double>(unwrap(lhs)) - static_cast<double>(unwrap(rhs)));
  } else {
    return ( internal::either_na(lhs, rhs) ) ?
    na<common_t>() :
    common_t(static_cast<unwrap_t<common_t>>(unwrap(lhs)) - static_cast<unwrap_t<common_t>>(unwrap(rhs)));
  }
}

template <MathType T, MathType U>
  requires (RMathType<T> || RMathType<U>)
inline constexpr auto operator*(T lhs, U rhs) noexcept {

  using common_t = common_math_t<T, U>;

  if constexpr (RIntegerType<common_t>){
    using I = unwrap_t<common_t>;
    I a = static_cast<I>(unwrap(lhs));
    I b = static_cast<I>(unwrap(rhs));
    I p;
    bool bad = internal::either_na(lhs, rhs) || __builtin_mul_overflow(a, b, &p);
    return bad ? na<common_t>() : common_t(p);
  } else if constexpr (is<T, r_dbl> && is<U, r_dbl>){
    return r_dbl(static_cast<double>(unwrap(lhs)) * static_cast<double>(unwrap(rhs)));
  } else {
    return ( internal::either_na(lhs, rhs) ) ?
    na<common_t>() :
    common_t(static_cast<unwrap_t<common_t>>(unwrap(lhs)) * static_cast<unwrap_t<common_t>>(unwrap(rhs)));
  }
}

template <MathType T, MathType U>
  requires (RMathType<T> || RMathType<U>)
inline constexpr r_dbl operator/(T lhs, U rhs) noexcept {
  return ( internal::either_na(lhs, rhs) ) ? na<r_dbl>() : r_dbl(static_cast<double>(unwrap(lhs)) / static_cast<double>(unwrap(rhs)));
}

template<MathType T, MathType U>
  requires (RFloatType<T> || RFloatType<U>)
inline constexpr r_dbl operator%(T lhs, U rhs) noexcept {
  if (unwrap(rhs) == 0){
    return r_dbl(R_NaN);
  } else if (internal::either_na(lhs, rhs)){
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
inline constexpr auto operator%(T lhs, U rhs) noexcept {
  using out_t = common_math_t<T, U>;
  using unwrapped_t = unwrap_t<out_t>;

  if ( unwrap(rhs) == 0 || internal::either_na(lhs, rhs) ){
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

template <RMathType T, MathType U>
inline constexpr T& operator+=(T &lhs, U rhs) noexcept {
  auto res = lhs + rhs;
  if constexpr (is<T, decltype(res)>){
    lhs = res;
  } else {
    // Narrowing back from common_t: map NA explicitly so it survives the cast
    lhs = is_na(res) ? na<T>() : T(static_cast<unwrap_t<T>>(unwrap(res)));
  }
  return lhs;
}

template <RMathType T, MathType U>
inline constexpr T& operator-=(T &lhs, U rhs) noexcept {
  auto res = lhs - rhs;
  if constexpr (is<T, decltype(res)>){
    lhs = res;
  } else {
    // Narrowing back from common_t: map NA explicitly so it survives the cast
    lhs = is_na(res) ? na<T>() : T(static_cast<unwrap_t<T>>(unwrap(res)));
  }
  return lhs;
}

template <RMathType T, MathType U>
inline constexpr T& operator*=(T &lhs, U rhs) noexcept {
  auto res = lhs * rhs;
  if constexpr (is<T, decltype(res)>){
    lhs = res;
  } else {
    // Narrowing back from common_t: map NA explicitly so it survives the cast
    lhs = is_na(res) ? na<T>() : T(static_cast<unwrap_t<T>>(unwrap(res)));
  }
  return lhs;
}

template <RMathType T, MathType U>
inline constexpr T& operator/=(T &lhs, U rhs) {
  if (internal::either_na(lhs, rhs)) {
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
inline constexpr T operator-(T x) noexcept {
  return is_na(x) ? x : T{-unwrap(x)};
}
template<>
inline constexpr r_dbl operator-(r_dbl x) noexcept {
  return r_dbl{-unwrap(x)};
}

template <RMathType T>
inline constexpr T& operator++(T &lhs) noexcept {
  lhs += T(1);
  return lhs;
}
template <RMathType T>
inline constexpr T operator++(T& lhs, int) noexcept {
  T tmp = lhs;
  ++lhs; 
  return tmp;
}

template <RMathType T>
inline constexpr T& operator--(T &lhs) noexcept {
  lhs -= T(1);
  return lhs;
}
template <RMathType T>
inline constexpr T operator--(T& lhs, int) noexcept {
  T tmp = lhs;
  --lhs; 
  return tmp;
}

}

#endif
