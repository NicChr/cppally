#ifndef CPPALLY_R_MATH_H
#define CPPALLY_R_MATH_H

#include <cppally/scalar/arithmetic_ops.h>
#include <cppally/scalar/relational_ops.h>
#include <cppally/scalar/r_limits.h>
#include <cppally/na.h>
#include <algorithm>

// R math functions that propagate NA values in the way R expects

namespace cppally {

namespace internal {

inline constexpr r_dbl round_to_even(r_dbl x){
  return x - r_dbl{std::remainder(unwrap(x), 1.0)};
}

inline constexpr bool is_pos_inf(r_dbl x) noexcept {
  return x.is_infinite() && unwrap(x) > 0;
}

inline constexpr bool is_neg_inf(r_dbl x) noexcept {
  return x.is_infinite() && unwrap(x) < 0;
}

}

template <NumericType T, NumericType U>
requires (RNumericType<T> || RNumericType<U>)
inline constexpr common_r_t<as_r_scalar_t<T>, as_r_scalar_t<U>> min(T x, U y) noexcept {
  
  using common_t = common_r_t<as_r_scalar_t<T>, as_r_scalar_t<U>>;

  return internal::either_na(x, y) ? na<common_t>() : 
  common_t(std::min(
    static_cast<unwrap_t<common_t>>(unwrap(x)), 
    static_cast<unwrap_t<common_t>>(unwrap(y))
  ));
}

template <NumericType T, NumericType U>
requires (RNumericType<T> || RNumericType<U>)
inline constexpr common_r_t<as_r_scalar_t<T>, as_r_scalar_t<U>> max(T x, U y) noexcept {
  
  using common_t = common_r_t<as_r_scalar_t<T>, as_r_scalar_t<U>>;

  return internal::either_na(x, y) ? na<common_t>() : 
  common_t(std::max(
    static_cast<unwrap_t<common_t>>(unwrap(x)), 
    static_cast<unwrap_t<common_t>>(unwrap(y))
  ));
}

inline r_str_view min(r_str_view x, r_str_view y){
  r_lgl res = x < y;
  if (is_na(res)){
    return na<r_str_view>();
  } else {
    return res ? x : y;
  }
}
inline r_str_view max(r_str_view x, r_str_view y){
  r_lgl res = x < y;
  if (is_na(res)){
    return na<r_str_view>();
  } else {
    return res ? y : x;
  }
}

inline r_str min(const r_str& x, const r_str& y){
  r_lgl res = x < y;
  if (is_na(res)){
    return na<r_str>();
  } else {
    return res ? x : y;
  }
}
inline r_str max(const r_str& x, const r_str& y){
  r_lgl res = x < y;
  if (is_na(res)){
    return na<r_str>();
  } else {
    return res ? y : x;
  }
}

template <RNumber T>
constexpr T abs(T x) noexcept {
  return is_na(x) ? x : T{internal::abs2(unwrap(x))};
}
template <>
inline constexpr r_dbl abs(r_dbl x) noexcept {
  return r_dbl(internal::abs2(unwrap(x)));
}
inline constexpr r_int abs(r_lgl x) noexcept {
  return abs(r_int(unwrap(x)));
}

template <RNumber T>
constexpr T floor(T x) noexcept {
  return is_na(x) ? x : T{internal::floor2(unwrap(x))};
}
template <RIntegerType T>
constexpr auto floor(T x) noexcept { 
  return +x;
}

template <RNumber T>
constexpr T ceiling(T x) noexcept {
  return is_na(x) ? x : T{std::ceil(unwrap(x))};
}
template <RIntegerType T>
constexpr auto ceiling(T x) noexcept { 
  return +x;
}

template <RNumber T>
constexpr T trunc(T x) noexcept {
  return is_na(x) ? x : T{std::trunc(unwrap(x))};
}
template <RIntegerType T>
constexpr auto trunc(T x) noexcept { 
  return +x;
}

template <RMathType T>
constexpr r_int sign(T x) noexcept {
  return is_na(x) ? na<r_int>() : r_int( (unwrap(x) > 0) - (unwrap(x) < 0) );
}

template <RMathType T>
r_dbl sqrt(T x) {
  return r_dbl(std::sqrt(unwrap(internal::coerce_number<r_dbl>(x))));
}

template <MathType T, MathType U>
  requires (RMathType<T> || RMathType<U>)
r_dbl pow(T x, U y){

  r_dbl x_ = internal::coerce_number<r_dbl>(x);
  r_dbl y_ = internal::coerce_number<r_dbl>(y);

  if (unwrap(y_) == 0.0){
     return r_dbl(1.0);
  }
  if (unwrap(x_) == 1.0){
    return r_dbl(1.0);
  }
  if (unwrap(y_) == 2.0){
    return x_ * x_;
  }

  return r_dbl(std::pow(x_, y_));
}

template <RMathType T>
r_dbl log10(T x){
  return r_dbl(std::log10(unwrap(internal::coerce_number<r_dbl>(x))));
}

template <RMathType T>
r_dbl exp(T x){
  return r_dbl(std::exp(internal::coerce_number<r_dbl>(x)));
}

template <MathType T, MathType U>
requires (RMathType<T> || RMathType<U>)
r_dbl log(T x, U base){
  return r_dbl(std::log(internal::coerce_number<r_dbl>(x)) / std::log(internal::coerce_number<r_dbl>(base)));
}
template <RMathType T>
r_dbl log(T x){
  return r_dbl(std::log(internal::coerce_number<r_dbl>(x)));
}

template <MathType T, MathType U>
requires (RMathType<T> || RMathType<U>)
r_dbl round(T x, U digits){

  r_dbl x_ = internal::coerce_number<r_dbl>(x);
  r_dbl digits_ = internal::coerce_number<r_dbl>(digits);

  if (is_na(x_)){
    return x_;
  } else if (is_na(digits_)){
    return na<r_dbl>();
  } else if (x_.is_infinite() || internal::is_pos_inf(digits_)){
    return x_;
  } else if (internal::is_neg_inf(digits_)){
    return r_dbl(0.0);
  } else {
    double scale = std::pow(10.0, digits_);
    return internal::round_to_even(x_ * scale) / scale;
  }
}

template <RNumber T>
T round(T x){
  if (is_na(x)){
    return x;
  } else if (internal::coerce_number<r_dbl>(x).is_infinite()){
    return x;
  } else {
    return internal::coerce_number<T>(internal::round_to_even(x));
  }
}

template <RIntegerType T>
auto round(T x){
  return +x;
}

template <MathType T, MathType U>
requires (RMathType<T> || RMathType<U>)
r_dbl signif(T x, U digits){

  r_dbl x_ = internal::coerce_number<r_dbl>(x);
  r_dbl digits_ = internal::coerce_number<r_dbl>(digits);
  r_dbl new_digits = max(1, digits_);

  if (is_na(x_)){
    return x_;
  } else if (is_na(new_digits)){
    return na<r_dbl>();
  } else if (new_digits.is_infinite()){
    return x_;
  } else {
    new_digits -= ceiling(log10(abs(x_)));
    r_dbl scale = pow(10, new_digits);
    return internal::round_to_even(scale * x_) / scale;
  }
}

// Greatest common divisor
template <RNumber T>
T gcd(T x, T y, T tol = r_limits<T>::tolerance()) noexcept {

  using unwrapped_t = unwrap_t<T>;

  T ax = abs(x);
  T ay = abs(y);

  unwrapped_t ax_ = unwrap(ax);
  unwrapped_t ay_ = unwrap(ay);
  unwrapped_t tol_ = unwrap(tol);

  if constexpr (RIntegerNumber<T>){

    if (unwrap(ax) == 1 || unwrap(ay) == 1){
      return T(1);
    }

    if (internal::either_na(x, y)){
      return na<T>();
    }

    // Taken from number theory lecture notes

    // GCD(0,0)=0
    if (ax_ == 0 && ay_ == 0){
      return T(0);
    }
    // GCD(a,0)=a
    if (ax_ == 0){
      return ay;
    }
    // GCD(a,0)=a
    if (ay_ == 0){
      return ax;
    }

    unwrapped_t r;

    while(ay_ != 0){
      r = ax_ % ay_;
      ax_ = ay_;
      ay_ = r;
    }
    return T(ax_);
  } else {

    if (internal::either_na(x, y)){
      return na<T>();
    }

    // GCD(0,0)=0
    if (ax_ <= tol_ && ay_ <= tol_){
      return T(0.0);
    }
    // GCD(a,0)=a
    if (ax_ <= tol_){
      return ay;
    }
    // GCD(a,0)=a
    if (ay_ <= tol_){
      return ax;
    }

    unwrapped_t r;
    while(ay_ > tol_){
      r = std::fmod(ax_, ay_);
      ax_ = ay_;
      ay_ = r;
    }
    return T(ax_);
  }
}

inline r_int gcd(r_lgl x, r_lgl y, r_lgl tol = r_limits<r_lgl>::tolerance()) noexcept {
  return gcd(r_int(unwrap(x)), r_int(unwrap(y)), r_int(unwrap(tol)));
}


// Lowest common multiple
// LCM(x, y) = (|x| / GCD(x, y)) * |y|
template <RNumber T>
T lcm(T x, T y, T tol = r_limits<T>::tolerance()) noexcept {

  T ax = abs(x);
  T ay = abs(y);

  if ( (ax <= tol || ay <= tol).is_true() ){
    return T(0);
  }

  if (internal::either_na(x, y)){
    return na<T>();
  }

  T out = ax;
  out /= gcd(ax, ay, tol);
  return out * ay;
}

inline r_int lcm(r_lgl x, r_lgl y, r_lgl tol = r_limits<r_lgl>::tolerance()) noexcept {
  return lcm(r_int(unwrap(x)), r_int(unwrap(y)), r_int(unwrap(tol)));
}


}

#endif

// Example of how to use structs to lessen the need for lambdas
// struct max_fn {

//   template <NumericType T, NumericType U>
//   requires (RNumericType<T> || RNumericType<U>)
//   constexpr common_r_t<T, U> operator()(T a, U b) const noexcept {
//     using common_t = common_r_t<T, U>;

//     return internal::either_na(a, b) ? na<common_t>() : 
//     common_t(std::max(
//       static_cast<unwrap_t<common_t>>(unwrap(a)), 
//       static_cast<unwrap_t<common_t>>(unwrap(b))
//     ));
//   }

//   r_str_view operator()(r_str_view x, r_str_view y) const {
//     r_lgl res = x < y;
//     if (is_na(res)){
//       return na<r_str_view>();
//     } else {
//       return res ? y : x;
//     }
//   }

//   r_str_view operator()(const r_str& x, const r_str& y) const {
//       r_lgl res = x < y;
//       if (is_na(res)){
//         return na<r_str>();
//       } else {
//         return res ? y : x;
//       }
//   }
// };

// inline constexpr max_fn max{};

// We can then write easily write a vectorised max function like so
// x.reduce(max);
