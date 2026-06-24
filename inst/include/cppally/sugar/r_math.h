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
inline constexpr common_r_t<T, U> min(T x, U y) noexcept {
  
  using common_t = common_r_t<T, U>;

  return internal::either_na(x, y) ? na<common_t>() : 
  common_t(std::min(
    static_cast<unwrap_t<common_t>>(unwrap(x)), 
    static_cast<unwrap_t<common_t>>(unwrap(y))
  ));
}

template <NumericType T, NumericType U>
requires (RNumericType<T> || RNumericType<U>)
inline constexpr common_r_t<T, U> max(T x, U y) noexcept {
  
  using common_t = common_r_t<T, U>;

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

template <RMathType T>
inline T abs(T x){
  return is_na(x) ? x : T{std::abs(unwrap(x))};
}

template <>
inline r_dbl abs(r_dbl x){
  return r_dbl(std::abs(unwrap(x)));
}

template<RMathType T>
inline T floor(T x){
  return is_na(x) ? x : T{std::floor(unwrap(x))};
}
template<>
inline r_dbl floor(r_dbl x){
  return r_dbl(std::floor(unwrap(x)));
}
template<RIntegerType T>
inline constexpr T floor(T x){
  return x;
}

template<RMathType T>
inline T ceiling(T x){
  return is_na(x) ? x : T{std::ceil(unwrap(x))};
}
template<>
inline r_dbl ceiling(r_dbl x){
  return r_dbl(std::ceil(unwrap(x)));
}
template<RIntegerType T>
inline constexpr T ceiling(T x){
  return x;
}

template<RMathType T>
inline T trunc(T x){
  return is_na(x) ? x : T{std::trunc(unwrap(x))};
}

template <>
inline r_dbl trunc(r_dbl x){
  return r_dbl(std::trunc(unwrap(x)));
}
template<RIntegerType T>
inline constexpr T trunc(T x){
  return x;
}

template <RMathType T>
inline constexpr r_int sign(T x) {
  return is_na(x) ? na<r_int>() : r_int( (unwrap(x) > 0) - (unwrap(x) < 0) );
}

template<RMathType T>
inline r_dbl sqrt(T x){
  return r_dbl(std::sqrt(as<r_dbl>(x).value));
}

template<MathType T, MathType U>
  requires (AtLeastOneRMathType<T, U>)
inline r_dbl pow(T x, U y){
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

template<RMathType T>
inline r_dbl log10(T x){
  return r_dbl(std::log10(unwrap(as<r_dbl>(x))));
}

template<RMathType T>
inline r_dbl exp(T x){
  return r_dbl(std::exp(as<r_dbl>(x).value));
}

template<MathType T, MathType U>
requires (AtLeastOneRMathType<T, U>)
inline r_dbl log(T x, U base){
  return r_dbl(std::log(as<r_dbl>(x)) / std::log(as<r_dbl>(base)));
}
template<RMathType T>
inline r_dbl log(T x){
  return r_dbl(std::log(as<r_dbl>(x).value));
}
inline r_cplx log(r_cplx x){
  if (is_na(x)){
    return x;
  }
  r_dbl real = r_dbl(0.5 * (log(pow(x.re(), 2.0) + pow(x.im(), 2.0))));
  r_dbl imag = r_dbl(std::atan2(as<r_dbl>(x.im()), as<r_dbl>(x.re())));
  return r_cplx{real, imag};
}


template<MathType T, MathType U>
requires (AtLeastOneRMathType<T, U>)
inline r_dbl round(T x, U digits){
  if (is_na(x)){
    return as<r_dbl>(x);
  } else if (is_na(digits)){
    return na<r_dbl>();
  } else if (identical(x, pos_inf)){
    return x;
  } else if (identical(digits, neg_inf)){
    return r_dbl(0.0);
  } else if (identical(digits, pos_inf)){
    return x;
  } else {
    r_dbl scale = r_dbl(std::pow(10.0, as<r_dbl>(digits)));
    return internal::round_to_even(as<r_dbl>(x) * scale) / scale;
  }
}

template<RMathType T>
inline T round(T x){
  if (is_na(x)){
    return x;
  } else if (identical(abs(x), pos_inf)){ 
    return x;
  } else {
    return as<T>(internal::round_to_even(as<r_dbl>(x)));
  }
}

template<RIntegerType T>
inline constexpr T round(T x){
  return x;
}

template<MathType T, MathType U>
requires (AtLeastOneRMathType<T, U>)
inline r_dbl signif(T x, U digits){
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

inline r_lgl is_whole_number(r_dbl x, r_dbl tolerance){
  return abs(x - round(x)) <= tolerance;
}


// Greatest common divisor
template <RMathType T>
inline T gcd(T x, T y, bool na_rm = false, T tol = r_limits<T>::tolerance()){

  using unwrapped_t = unwrap_t<T>;

  T ax = abs(x);
  T ay = abs(y);

  if (is_na(ax) || is_na(ay)){
    if (na_rm){ 
      if (is_na(ax)){
        return ay;
      } else {
        return ax;
      }
    } else {
      return na<T>();
    }
  }

  unwrapped_t ax_ = unwrap(ax);
  unwrapped_t ay_ = unwrap(ay);
  unwrapped_t tol_ = unwrap(tol);

  if constexpr (RIntegerType<T>){

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


// Lowest common multiple
template <RMathType T>
inline T lcm(T x, T y, bool na_rm = false, T tol = r_limits<T>::tolerance()){
  if (is_na(x) || is_na(y)){
    if (na_rm){
      if (is_na(x)){
        return y;
      } else {
        return x;
      }
    } else {
      return na<T>();
    }
  }

  
  T ax = abs(x);
  T ay = abs(y);

  if constexpr (RIntegerType<T>){
    if (ax == 0 && ay == 0){
      return T(0);
    }
    // Because `/` for RMath types returns r_dbl and the C++ version doesn't
    // we must use the C++ version
    // We should always expect res to be an integer because the x is always divisible by gcd(x, y) exactly
    T res = T(unwrap(ax) / unwrap(gcd(x, y, na_rm)));
    if (y != 0 && (res > (r_limits<T>::max() / ay))){
      return na<T>();
    }
    return res * ay;
  } else {
    if (ax <= tol && ay <= tol){
      return T(0.0);
    }
    return ( ax / gcd(x, y, na_rm, tol) ) * ay;
  }
}


}

#endif
