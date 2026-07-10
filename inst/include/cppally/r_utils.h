#ifndef CPPALLY_R_UTILS_H
#define CPPALLY_R_UTILS_H

#include <cppally/r_setup.h>
#include <cppally/r_concepts.h>
#include <utility>
#include <limits>

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
template <typename T>
inline constexpr void recycle_index(T& v, T size) {
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

template <typename T, typename U>
inline constexpr bool between_impl(const T x, const U lo, const U hi) {
  return x >= lo && x <= hi;
}

inline int calc_threads(r_size_t data_size){
    return data_size >= CPPALLY_OMP_THRESHOLD ? get_threads() : 1;
  }

}

}

#endif
