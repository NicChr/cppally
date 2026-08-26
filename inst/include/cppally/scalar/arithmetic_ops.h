#ifndef CPPALLY_R_ARITHMETIC_OPS_H
#define CPPALLY_R_ARITHMETIC_OPS_H

// ------- Custom arithmetic operators for cppally scalars -------
// NA handling mirrors R's NA handling.
// Integer overflow is never undefined behaviour (UB) - NA is always returned when overflow is detected.
// This header is intentionally kept as light as possible to allow other scalar classes to use these methods.
// License: MIT License
// Author: Nick Christofides

// Arithmetic operators: +,-,*,/,%,+=,-=,*=,/=,%=,-,++,--

#include <cppally/r_setup.h>
#include <cppally/r_concepts.h>
#include <cppally/utils.h>
#include <cppally/scalar/r_lgl.h>
#include <cppally/scalar/r_int.h>
#include <cppally/scalar/r_int64.h>
#include <cppally/scalar/r_dbl.h>
#include <algorithm> // For std::min
#include <cmath> // For std::floor

namespace cppally {

namespace internal {

#if defined(__has_builtin)
#  if __has_builtin(__builtin_mul_overflow)
#    define CPPALLY_HAS_BUILTIN_MUL_OVERFLOW 1
#  endif
#endif

// Portable signed multiply with overflow detection.
// On overflow returns true and out is unspecified, otherwise false
template <CppIntegerType I>
inline constexpr bool mul_overflow(I a, I b, I& out) noexcept {
    if constexpr (sizeof(I) < 8){
    int_fast64_t p = static_cast<int_fast64_t>(a) * static_cast<int_fast64_t>(b);
    out = static_cast<I>(p);
    return p != static_cast<int_fast64_t>(out);
    } 
    // else if constexpr (sizeof(I) == 8 && int128_available){
    //   int128_otherwise_64_t p = static_cast<int128_otherwise_64_t>(a) * static_cast<int128_otherwise_64_t>(b);
    //   out = static_cast<I>(p);
    //   return p != static_cast<int128_otherwise_64_t>(out);
    // } 
    else {
    #ifdef CPPALLY_HAS_BUILTIN_MUL_OVERFLOW
    return __builtin_mul_overflow(a, b, &out);
    #else
    using UI = std::make_unsigned_t<I>;
    out = static_cast<I>(static_cast<UI>(a) * static_cast<UI>(b));
    if (a == 0 || b == 0){
        return false;
    }
    if (b == -1){
        return a == std::numeric_limits<I>::min(); // avoid MIN / -1 trap below
    }
    // If the product wrapped, it is off by a multiple of 2^64 and division cannot recover a
    return (out / b) != a;
    #endif
    }
}

// constexpr abs() since std::abs isn't constexpr until C++23
// Only defined for arithmetic types.
// This may retain -0.0 and negative-signed NaN but 
// that's okay since we never want to distinguish those in outputs the user cares about. 
// For more info: cppally has 2 NaN types, R's NA_REAL and all other NaN. 
// Negative zeroes are explicitly converted into positive ones where it matters (e.g. hashing), and this function
// has no effect on those anyway. 
template <CppMathType T>
constexpr T abs2(T x) noexcept {
  return x < 0 ? -x : x;
}

// constexpr floor
template <CppFloatType T>
constexpr T floor2(T x) noexcept {

  // If x is very large then it won't have a fractional part anyway
  if (!numeric_can_be_cast_without_complete_loss<int64_t>(x)){
    return x;
  }

  // If the round-trip from double -> int64_t -> double is lossless (i.e identity preserving), then it needs no flooring since it's a whole number
  if (numeric_cast_is_lossless<int64_t>(x)){
    return x;
  }
  int64_t int_res = x < 0 ? static_cast<int64_t>(x) - 1 : static_cast<int64_t>(x);
  return static_cast<T>(int_res);
}

// Floored quotient, matching R's %/%
template <CppIntegerType I>
inline constexpr I floor_div(I a, I b) noexcept {
    I q = a / b;
    if ((a % b) != 0 && ((a > 0) != (b > 0))){
    --q;
    }
    return q;
}

template <CppFloatType F>
inline constexpr F floor_div(F a, F b) noexcept {
  return floor2(a / b);
}

// Floored remainder, matching R's %%
template <CppIntegerType I>
inline constexpr I floor_mod(I a, I b) noexcept {
    I r = a % b;
    if (r != 0 && ((a > 0) != (b > 0))){
    r += b;
    }
    return r;
}

template <CppFloatType F>
inline constexpr F floor_mod(F a, F b) noexcept {
    return a - (b * floor_div(a, b));
}

template <RMathType T, RMathType U>
constexpr bool any_arithmetic_na(T x, U y) noexcept {
  return x.is_na() || y.is_na(); // This is mostly only reached for when !is<T, U>
}
template <RIntegerType T>
constexpr bool any_arithmetic_na(T x, T y) noexcept {
  using x_t = std::remove_cvref_t<T>;
  static_assert(std::numeric_limits<unwrap_t<T>>::min() == unwrap(x_t::na()), "`std::numeric_limits<unwrap_t<T>>::min() == unwrap(na<T>())` must hold for all cppally integer types `T`");
  return std::min(unwrap(x), unwrap(y)) == unwrap(x_t::na());
}

inline constexpr bool any_arithmetic_na(r_dbl x, r_dbl y) noexcept {
  return r_dbl(abs2(unwrap(x)) + abs2(unwrap(y))).is_na();
}

template <RMathType T, CppMathType U>
constexpr bool any_arithmetic_na(T x, U y) noexcept {
  return any_arithmetic_na(x, as_r_scalar_t<U>(y));
}
template <CppMathType T, RMathType U>
constexpr bool any_arithmetic_na(T x, U y) noexcept {
  return any_arithmetic_na(as_r_scalar_t<T>(x), y);
}

#undef CPPALLY_HAS_BUILTIN_MUL_OVERFLOW

}

// Arithmetic operators

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
    bool bad = (((a ^ s) & (b ^ s)) < 0) | internal::any_arithmetic_na(lhs, rhs);
    return bad ? common_t::na() : common_t(s);
  } else if constexpr (is<T, r_dbl> && is<U, r_dbl>){
    return r_dbl(static_cast<double>(unwrap(lhs)) + static_cast<double>(unwrap(rhs)));
  } else {
    return internal::any_arithmetic_na(lhs, rhs) ? 
    common_t::na() : 
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
    bool bad = (((a ^ b) & (a ^ s)) < 0) | internal::any_arithmetic_na(lhs, rhs);
    return bad ? common_t::na() : common_t(s);
  } else if constexpr (is<T, r_dbl> && is<U, r_dbl>){
    return r_dbl(static_cast<double>(unwrap(lhs)) - static_cast<double>(unwrap(rhs)));
  } else {
    return internal::any_arithmetic_na(lhs, rhs) ?
    common_t::na() :
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
    bool bad = internal::any_arithmetic_na(lhs, rhs) || internal::mul_overflow(a, b, p);
    return bad ? common_t::na() : common_t(p);
  } else if constexpr (is<T, r_dbl> && is<U, r_dbl>){
    return r_dbl(static_cast<double>(unwrap(lhs)) * static_cast<double>(unwrap(rhs)));
  } else {
    return internal::any_arithmetic_na(lhs, rhs) ?
    common_t::na() :
    common_t(static_cast<unwrap_t<common_t>>(unwrap(lhs)) * static_cast<unwrap_t<common_t>>(unwrap(rhs)));
  }
}

template <MathType T, MathType U>
  requires (RMathType<T> || RMathType<U>)
inline constexpr r_dbl operator/(T lhs, U rhs) noexcept {
  return internal::any_arithmetic_na(lhs, rhs) ? r_dbl::na() : r_dbl(static_cast<double>(unwrap(lhs)) / static_cast<double>(unwrap(rhs)));
}

template <MathType T, MathType U>
  requires (RMathType<T> || RMathType<U>)
inline constexpr auto operator%(T lhs, U rhs) noexcept {

  using common_t = common_math_t<T, U>;

  const bool has_na = internal::any_arithmetic_na(lhs, rhs);

  if constexpr (RIntegerType<common_t>){
    using I = unwrap_t<common_t>;

    if (has_na || unwrap(rhs) == 0){
      return common_t::na();
    }
    I a = static_cast<I>(unwrap(lhs));
    I b = static_cast<I>(unwrap(rhs));
    return common_t(internal::floor_mod(a, b));
  } else {
    if (unwrap(rhs) == 0){
      return r_dbl::nan();
    } else if (has_na){
      return r_dbl::na();
    }
    double a = static_cast<double>(unwrap(lhs));
    double b = static_cast<double>(unwrap(rhs));
    return r_dbl(internal::floor_mod(a, b));
  }
}

template <RNumber T, MathType U>
inline constexpr T& operator+=(T& lhs, U rhs) noexcept {
  auto res = lhs + rhs;
  if constexpr (is<T, decltype(res)>){
    lhs = res;
  } else {
    using unwrapped_t = unwrap_t<T>;
    lhs = res.is_na() || !internal::numeric_can_be_cast_without_complete_loss<unwrapped_t>(unwrap(res)) ? std::remove_cvref_t<T>::na() : T(static_cast<unwrapped_t>(unwrap(res)));
  }
  return lhs;
}

template <RNumber T, MathType U>
inline constexpr T& operator-=(T& lhs, U rhs) noexcept {
  auto res = lhs - rhs;
  if constexpr (is<T, decltype(res)>){
    lhs = res;
  } else {
    using unwrapped_t = unwrap_t<T>;
    lhs = res.is_na() || !internal::numeric_can_be_cast_without_complete_loss<unwrapped_t>(unwrap(res)) ? std::remove_cvref_t<T>::na() : T(static_cast<unwrapped_t>(unwrap(res)));
  }
  return lhs;
}

template <RNumber T, MathType U>
inline constexpr T& operator*=(T& lhs, U rhs) noexcept {
  auto res = lhs * rhs;
  if constexpr (is<T, decltype(res)>){
    lhs = res;
  } else {
    using unwrapped_t = unwrap_t<T>;
    lhs = res.is_na() || !internal::numeric_can_be_cast_without_complete_loss<unwrapped_t>(unwrap(res)) ? std::remove_cvref_t<T>::na() : T(static_cast<unwrapped_t>(unwrap(res)));
  }
  return lhs;
}

template <RNumber T, MathType U>
inline constexpr T& operator%=(T& lhs, U rhs) noexcept {
  auto res = lhs % rhs;
  if constexpr (is<T, decltype(res)>){
    lhs = res;
  } else {
    using unwrapped_t = unwrap_t<T>;
    lhs = res.is_na() || !internal::numeric_can_be_cast_without_complete_loss<unwrapped_t>(unwrap(res)) ? std::remove_cvref_t<T>::na() : T(static_cast<unwrapped_t>(unwrap(res)));
  }
  return lhs;
}

// `/=` uses true division for r_dbl and floored division (R's `%/%`) for integers
template <RNumber T, MathType U>
inline constexpr T& operator/=(T& lhs, U rhs) noexcept {
  using common_t = common_math_t<T, U>;
  using unwrapped_common_t = unwrap_t<common_t>;
  using unwrapped_t = unwrap_t<T>;

  if (internal::any_arithmetic_na(lhs, rhs)){
    lhs = std::remove_cvref_t<T>::na();
    return lhs;
  }

  if constexpr (RIntegerType<common_t>){
    if (unwrap(rhs) == 0){
      lhs = std::remove_cvref_t<T>::na();
    } else {
      unwrapped_common_t a = static_cast<unwrapped_common_t>(unwrap(lhs));
      unwrapped_common_t b = static_cast<unwrapped_common_t>(unwrap(rhs));
      unwrapped_common_t q = internal::floor_div(a, b);
      lhs.value = !internal::numeric_can_be_cast_without_complete_loss<unwrapped_t>(q) ? unwrap(std::remove_cvref_t<T>::na()) : static_cast<unwrapped_t>(q);
    }
  } else {
    double res = unwrap(lhs / rhs);
    if constexpr (RIntegerType<T>){
      res = std::floor(res); // integer target matches R's %/%
    }
    lhs.value = !internal::numeric_can_be_cast_without_complete_loss<unwrapped_t>(res) ? unwrap(std::remove_cvref_t<T>::na()) : static_cast<unwrapped_t>(res);
  }
  return lhs;
}

template <RNumber T>
inline constexpr T operator-(T x) noexcept {
  return x.is_na() ? x : T{-unwrap(x)};
}
template<>
inline constexpr r_dbl operator-(r_dbl x) noexcept {
  return r_dbl{-unwrap(x)};
}
inline constexpr r_int operator-(r_lgl x) noexcept {
  return -r_int(unwrap(x));
}

template <RNumber T>
inline constexpr T operator+(T x) noexcept {
  return x;
}
template<>
inline constexpr r_dbl operator+(r_dbl x) noexcept {
  return x;
}
inline constexpr r_int operator+(r_lgl x) noexcept {
  return r_int(unwrap(x));
}

template <RNumber T>
inline constexpr T& operator++(T& lhs) noexcept {
  lhs += T(1);
  return lhs;
}
template <RNumber T>
inline constexpr T operator++(T& lhs, int) noexcept {
  T tmp = lhs;
  ++lhs; 
  return tmp;
}

template <RNumber T>
inline constexpr T& operator--(T& lhs) noexcept {
  lhs -= T(1);
  return lhs;
}
template <RNumber T>
inline constexpr T operator--(T& lhs, int) noexcept {
  T tmp = lhs;
  --lhs; 
  return tmp;
}

}

#endif
