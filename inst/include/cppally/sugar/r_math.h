#ifndef CPPALLY_R_MATH_H
#define CPPALLY_R_MATH_H

#include <cppally/r_scalar_ops.h>
#include <cppally/r_limits.h>
#include <cppally/r_coerce.h>
#include <algorithm>

// R math functions that propagate NA values in the way R expects

namespace cppally {

namespace internal {

inline constexpr r_dbl round_to_even(r_dbl x){
  return x - r_dbl{std::remainder(unwrap(x), 1.0)};
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
  return is_na(x) ? x : T{std::abs(unwrap(x))};
}
template <>
inline constexpr r_dbl abs(r_dbl x) noexcept {
  return r_dbl(std::abs(unwrap(x)));
}
inline constexpr r_int abs(r_lgl x) noexcept {
  return abs(r_int(unwrap(x)));
}

template <RNumber T>
constexpr T floor(T x) noexcept {
  if constexpr (RIntegerNumber<T>){
    return x;
  } else {
    return is_na(x) ? x : T{std::floor(unwrap(x))};
  }
}
inline constexpr r_int floor(r_lgl x) noexcept {
  return r_int(unwrap(x));
}

template <RNumber T>
constexpr T ceiling(T x) noexcept {
  if constexpr (RIntegerNumber<T>){
    return x;
  } else {
    return is_na(x) ? x : T{std::ceil(unwrap(x))};
  }
}
inline constexpr r_int ceiling(r_lgl x) noexcept {
  return r_int(unwrap(x));
}

template <RNumber T>
constexpr T trunc(T x) noexcept {
  if constexpr (RIntegerNumber<T>){
    return x;
  } else {
    return is_na(x) ? x : T{std::trunc(unwrap(x))};
  }
}
inline constexpr r_int trunc(r_lgl x) noexcept {
  return r_int(unwrap(x));
}

template <RMathType T>
constexpr r_int sign(T x) {
  return is_na(x) ? na<r_int>() : r_int( (unwrap(x) > 0) - (unwrap(x) < 0) );
}

template <RMathType T>
r_dbl sqrt(T x){
  return r_dbl(std::sqrt(unwrap(as<r_dbl>(x))));
}

template <MathType T, MathType U>
  requires (RMathType<T> || RMathType<U>)
r_dbl pow(T x, U y){
  if ((y == r_dbl(0.0)).is_true()){
     return r_dbl(1.0);
  }
  if ((x == r_dbl(1.0)).is_true()){
    return r_dbl(1.0);
  }
  if ((y == r_dbl(2.0)).is_true()){
    r_dbl left = as<r_dbl>(x);
    return left * left;
  }
  return r_dbl(std::pow(unwrap(as<r_dbl>(x)), unwrap(as<r_dbl>(y))));
}

template <RMathType T>
r_dbl log10(T x){
  return r_dbl(std::log10(unwrap(as<r_dbl>(x))));
}

template <RMathType T>
r_dbl exp(T x){
  return r_dbl(std::exp(as<r_dbl>(x).value));
}

template <MathType T, MathType U>
requires (RMathType<T> || RMathType<U>)
r_dbl log(T x, U base){
  return r_dbl(std::log(as<r_dbl>(x)) / std::log(as<r_dbl>(base)));
}
template <RMathType T>
r_dbl log(T x){
  return r_dbl(std::log(as<r_dbl>(x).value));
}

template <MathType T, MathType U>
requires (RMathType<T> || RMathType<U>)
r_dbl round(T x, U digits){
  if (is_na(x)){
    return as<r_dbl>(x);
  } else if (is_na(digits)){
    return na<r_dbl>();
  } else if (identical(x, pos_inf) || identical(digits, pos_inf)){
    return as<r_dbl>(x);
  } else if (identical(digits, neg_inf)){
    return r_dbl(0.0);
  } else {
    r_dbl scale = r_dbl(std::pow(10.0, as<r_dbl>(digits)));
    return internal::round_to_even(as<r_dbl>(x) * scale) / scale;
  }
}

template <RNumber T>
T round(T x){
  if constexpr (RIntegerNumber<T>){
    return x;
  } else {
    if (is_na(x)){
      return x;
    } else if (identical(abs(x), pos_inf)){ 
      return x;
    } else {
      return as<T>(internal::round_to_even(as<r_dbl>(x)));
    }
  }

}

inline constexpr r_int round(r_lgl x){
  return r_int(unwrap(x));
}

template <MathType T, MathType U>
requires (RMathType<T> || RMathType<U>)
r_dbl signif(T x, U digits){
  as_r_scalar_t<U> new_digits = max(as_r_scalar_t<U>(1), as<as_r_scalar_t<U>>(digits));
  if (is_na(x)){
    return as<r_dbl>(x);
  } else if (is_na(new_digits)){
    return na<r_dbl>();
  } else if (identical(new_digits, pos_inf)){
    return as<r_dbl>(x);
  } else {
    new_digits -= ceiling(log10(abs(x)));
    r_dbl scale = pow(10, new_digits);
    return internal::round_to_even(scale * x) / scale;
  }
}

inline r_lgl is_whole_number(r_dbl x, r_dbl tolerance = sqrt(r_limits<r_dbl>::epsilon())){
  return abs(x - round(x)) <= tolerance;
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

    if (identical(ax, T(1)) || identical(ay, T(1))){
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
