#ifndef CPPALLY_R_UTILS_H
#define CPPALLY_R_UTILS_H

#include <cppally/r_setup.h>
#include <cppally/r_concepts.h>
#include <utility>
#include <limits>
#include <cmath>
#include <version>
#include <type_traits>

namespace cppally {

// Maximum available threads
inline int max_threads() noexcept {
  return OMP_MAX_THREADS;
}
// get the number of OMP threads currently set for use
inline int get_threads() noexcept {
  auto n_threads = internal::CPPALLY_N_THREADS > max_threads() ? max_threads() : internal::CPPALLY_N_THREADS;
  return n_threads > 1 ? n_threads : 1;
}
// Set number threads to be used throughout the program
inline void set_threads(int n) noexcept {
  internal::CPPALLY_N_THREADS = n < max_threads() ? n : max_threads();
}

// Recycle loop indices
// `v` is an index to be recycled
// `size` is the size of the vector that we are indexing with `v`
template <CppNumber T>
inline constexpr void recycle_index(T& v, T size) noexcept {
  v = (++v == size) ? T(0) : v;
}

// inline bool xor_(bool a, bool b) {
//   return (a + b) == 1;
// }

namespace internal {

template <CppFloatType F>
consteval F exp2(int n) noexcept {
  F out = F(1);
  while (n-- > 0){
    out *= F(2);
  }
  return out;
}

// Complete loss means the value can't survive the cast in any recognisable
// form: integer overflow, float overflow, Inf/NaN into an integer.
// Precision loss (fraction truncation, mantissa rounding) is tolerated.
template <CppMathType To, CppMathType From>
constexpr bool numeric_can_be_cast_without_complete_loss(From x) noexcept {
  if constexpr (lossless_numeric_cast<From, To>()){
    return true;
  } else if constexpr (CppIntegerType<From> && CppIntegerType<To>){
    return std::cmp_greater_equal(+x, +std::numeric_limits<To>::min())
        && std::cmp_less_equal(+x, +std::numeric_limits<To>::max());
  } else if constexpr (CppIntegerType<From> && CppFloatType<To>){
    // int -> float: magnitude always fits; only precision is lost (tolerated)
    return true;
  } else if constexpr (CppIntegerType<To>){
    // Float -> integer: fractions truncate toward zero, out-of-range is complete loss
    // Open upper bound: 2^digits is exact in From whereas To's max may round up
    constexpr From hi = exp2<From>(std::numeric_limits<To>::digits);
    constexpr From lo = std::is_signed_v<To> ? -hi : From(0);
    return x >= lo && x < hi; // also rejects Inf/NaN
  } else {
    // Narrowing float -> float, e.g. double -> float: overflow to Inf is complete loss
    constexpr From to_max = static_cast<From>(std::numeric_limits<To>::max());
    return (x >= -to_max && x <= to_max)
      || x == std::numeric_limits<From>::infinity()
      || x == -std::numeric_limits<From>::infinity();
  }
}

// Exact whole-number test
// inline bool is_exact_whole(double x) noexcept {
//     return (std::trunc(x) == x) && !std::isinf(x);
// }

// Safely check at runtime that numeric cast is lossless
template <CppMathType To, CppMathType From>
constexpr bool numeric_cast_is_lossless(From x) noexcept {
    return numeric_can_be_cast_without_complete_loss<To>(x) && numeric_can_be_cast_without_complete_loss<From>(static_cast<To>(x)) && static_cast<From>(static_cast<To>(x)) == x;
}

// constexpr abs() since std::abs isn't constexpr until C++23
// Only defined for arithmetic types.
// This may retain negative zeroes and negative-signed NaN but
// that's okay since we never want to distinguish those in outputs the user cares about.
template <CppNumber T>
constexpr T abs2(T x) noexcept {
  return std::is_constant_evaluated() ? (x < 0 ? -x : x) + T{0} : std::abs(x);
}

// constexpr floor
template <CppFloatType T>
constexpr T floor2(T x) noexcept {

  #if defined(__cpp_lib_constexpr_cmath) && __cpp_lib_constexpr_cmath >= 202202L
    return std::floor(x);
  #else

    if (!std::is_constant_evaluated()){
      return std::floor(x);
    }
    
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
  #endif
}

template <CppFloatType T>
constexpr T ceiling2(T x) noexcept {

  #if defined(__cpp_lib_constexpr_cmath) && __cpp_lib_constexpr_cmath >= 202202L
    return std::ceil(x);
  #else

    if (!std::is_constant_evaluated()){
      return std::ceil(x);
    }

    // If x is very large then it won't have a fractional part anyway
    if (!numeric_can_be_cast_without_complete_loss<int64_t>(x)){
      return x;
    }

    // If the round-trip from double -> int64_t -> double is lossless (i.e identity preserving), then it needs no flooring since it's a whole number
    if (numeric_cast_is_lossless<int64_t>(x)){
      return x;
    }
    int64_t int_res = x > 0 ? static_cast<int64_t>(x) + 1 : static_cast<int64_t>(x);
    return static_cast<T>(int_res);
  #endif
}

inline int calc_threads(r_size_t data_size){
    if (OMP_IN_PARALLEL){
      return 1;
    }
    return data_size >= CPPALLY_OMP_THRESHOLD ? get_threads() : 1;
  }

}

}

#endif
