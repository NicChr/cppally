#ifndef CPPALLY_R_VEC_OPS_H
#define CPPALLY_R_VEC_OPS_H

#include <cppally/r_utils.h>
#include <cppally/r_vec.h>
#include <cppally/r_pmap.h>

// Vectorised binary operators: +,-,*,/,&,|,+=,-=,*=,/=,==,<=,<,>=,>
// Vectorised unary operators: !,-,
// All operators are only for atomic vectors
// Arithmetic operators are constrained on vectors of element RMathType

// ----- Important notes -----
//
// Most (if not all) operators will implicitly mutate in-place when certain conditions (detailed below) are satisfied
//
// Forwarding references are used to achieve this
//
// To be able to IMPLICITLY mutate vectors in-place, a few things need to be checked
// - Must be owning rvalue
// - Must satisfy x.is_exclusive, this ensures it is the only object that references the underlying SEXP
// - The resulting vector type must align with the input vector type
//
// Crucially though, the first (owning rvalue) check must be done at compile time (via e.g. if constexpr block)
// So that the mutating branch is not checked by the compiler when it doesn't need to be

namespace cppally {


#define CPPALLY_BINARY_OP_IN_PLACE(OP)                                                                \
r_size_t lhs_size = lhs.length();                                                                     \
if constexpr (RAtomicVector<U>){                                                                      \
  r_size_t rhs_size = rhs.length();                                                                   \
  if (rhs_size == 1){                                                                                 \
    auto val = rhs.view(0);                                                                           \
    int n_threads = internal::calc_threads(lhs_size);                                                 \
    if constexpr (RObject<typename std::remove_cvref_t<decltype(lhs)>::data_type>){                   \
      for (r_size_t i = 0; i < lhs_size; ++i){ lhs.set(i, lhs.view(i) OP val); }                      \
    } else if (n_threads > 1){                                                                        \
      OMP_PARALLEL_FOR_SIMD(n_threads)                                                                \
      for (r_size_t i = 0; i < lhs_size; ++i){ lhs.set(i, lhs.view(i) OP val); }                      \
    } else {                                                                                          \
      OMP_SIMD                                                                                        \
      for (r_size_t i = 0; i < lhs_size; ++i){ lhs.set(i, lhs.view(i) OP val); }                      \
    }                                                                                                 \
  } else if (lhs_size == rhs_size){                                                                   \
      int n_threads = internal::calc_threads(lhs_size);                                               \
      if constexpr (RObject<typename std::remove_cvref_t<decltype(lhs)>::data_type>){                 \
        for (r_size_t i = 0; i < lhs_size; ++i){                                                      \
          lhs.set(i, lhs.get(i) OP rhs.get(i));                                                       \
        }                                                                                             \
      } else if (n_threads > 1){                                                                      \
        OMP_PARALLEL_FOR_SIMD(n_threads)                                                              \
        for (r_size_t i = 0; i < lhs_size; ++i){                                                      \
          lhs.set(i, lhs.get(i) OP rhs.get(i));                                                       \
        }                                                                                             \
      } else {                                                                                        \
        OMP_SIMD                                                                                      \
        for (r_size_t i = 0; i < lhs_size; ++i){                                                      \
          lhs.set(i, lhs.get(i) OP rhs.get(i));                                                       \
        }                                                                                             \
      }                                                                                               \
  } else {                                                                                            \
    r_size_t n = lhs_size;                                                                            \
    for (r_size_t i = 0, rhsi = 0; i < n;                                                             \
    recycle_index(rhsi, rhs_size),                                                                    \
    ++i){                                                                                             \
      lhs.set(i, lhs.get(i) OP rhs.get(rhsi));                                                        \
    }                                                                                                 \
  }                                                                                                   \
} else {                                                                                              \
  r_size_t n = lhs_size;                                                                              \
  int n_threads = internal::calc_threads(n);                                                          \
  if constexpr (RObject<typename std::remove_cvref_t<decltype(lhs)>::data_type>){                     \
    for (r_size_t i = 0; i < n; ++i){                                                                 \
      lhs.set(i, lhs.get(i) OP rhs);                                                                  \
    }                                                                                                 \
  } else if (n_threads > 1){                                                                          \
    OMP_PARALLEL_FOR_SIMD(n_threads)                                                                  \
    for (r_size_t i = 0; i < n; ++i){                                                                 \
      lhs.set(i, lhs.get(i) OP rhs);                                                                  \
    }                                                                                                 \
  } else {                                                                                            \
    OMP_SIMD                                                                                          \
    for (r_size_t i = 0; i < n; ++i){                                                                 \
      lhs.set(i, lhs.get(i) OP rhs);                                                                  \
    }                                                                                                 \
  }                                                                                                   \
}

#define CPPALLY_BINARY_OP(lhs, rhs, OP, res_t)                                                                         \
using lhs_t = decltype(lhs);                                                                                           \
using rhs_t = decltype(rhs);                                                                                           \
if constexpr (RAtomicVector<lhs_t> && RAtomicVector<rhs_t>){                                                           \
  r_size_t n1 = lhs.length();                                                                                          \
  r_size_t n2 = rhs.length();                                                                                          \
  r_size_t n = std::max(n1, n2);                                                                                       \
  if (n1 == 0 || n2 == 0) return res_t();                                                                              \
  int n_threads = internal::calc_threads(n);                                                                           \
  if (n2 == 1){                                                                                                        \
    res_t out(n);                                                                                                      \
    auto val = rhs.view(0);                                                                                            \
    if constexpr (RObject<typename std::remove_cvref_t<decltype(out)>::data_type>){                                    \
      for (r_size_t i = 0; i < n; ++i){                                                                                \
        out.set(i, lhs.view(i) OP val);                                                                                \
      }                                                                                                                \
    } else if (n_threads > 1){                                                                                         \
      OMP_PARALLEL_FOR_SIMD(n_threads)                                                                                 \
      for (r_size_t i = 0; i < n; ++i){                                                                                \
        out.set(i, lhs.view(i) OP val);                                                                                \
      }                                                                                                                \
    } else {                                                                                                           \
      OMP_SIMD                                                                                                         \
      for (r_size_t i = 0; i < n; ++i){                                                                                \
        out.set(i, lhs.view(i) OP val);                                                                                \
      }                                                                                                                \
    }                                                                                                                  \
    return out;                                                                                                        \
  } else if (n1 == 1){                                                                                                 \
    res_t out(n);                                                                                                      \
    auto val = lhs.view(0);                                                                                            \
    if constexpr (RObject<typename std::remove_cvref_t<decltype(out)>::data_type>){                                    \
      for (r_size_t i = 0; i < n; ++i){                                                                                \
        out.set(i, val OP rhs.view(i));                                                                                \
      }                                                                                                                \
    } else if (n_threads > 1){                                                                                         \
      OMP_PARALLEL_FOR_SIMD(n_threads)                                                                                 \
      for (r_size_t i = 0; i < n; ++i){                                                                                \
        out.set(i, val OP rhs.view(i));                                                                                \
      }                                                                                                                \
    } else {                                                                                                           \
      OMP_SIMD                                                                                                         \
      for (r_size_t i = 0; i < n; ++i){                                                                                \
        out.set(i, val OP rhs.view(i));                                                                                \
      }                                                                                                                \
    }                                                                                                                  \
    return out;                                                                                                        \
  } else {                                                                                                             \
    return pmap_parallel_simd([](auto a, auto b) { return a OP b; }, lhs, rhs);                                        \
  }                                                                                                                    \
  /*Cases where one is a scalar*/                                                                                      \
} else if constexpr (RAtomicVector<lhs_t>) {                                                                           \
  r_size_t n = lhs.length();                                                                                           \
  res_t out(n);                                                                                                        \
  int n_threads = internal::calc_threads(n);                                                                           \
  if constexpr (RObject<typename std::remove_cvref_t<decltype(out)>::data_type>){                                      \
    for (r_size_t i = 0; i < n; ++i){                                                                                  \
      out.set(i, lhs.view(i) OP rhs);                                                                                  \
    }                                                                                                                  \
  } else if (n_threads > 1){                                                                                           \
    OMP_PARALLEL_FOR_SIMD(n_threads)                                                                                   \
    for (r_size_t i = 0; i < n; ++i){                                                                                  \
      out.set(i, lhs.view(i) OP rhs);                                                                                  \
    }                                                                                                                  \
  } else {                                                                                                             \
    OMP_SIMD                                                                                                           \
    for (r_size_t i = 0; i < n; ++i){                                                                                  \
      out.set(i, lhs.view(i) OP rhs);                                                                                  \
    }                                                                                                                  \
  }                                                                                                                    \
  return out;                                                                                                          \
} else {                                                                                                               \
  r_size_t n = rhs.length();                                                                                           \
  res_t out(n);                                                                                                        \
  int n_threads = internal::calc_threads(n);                                                                           \
  if constexpr (RObject<typename std::remove_cvref_t<decltype(out)>::data_type>){                                      \
    for (r_size_t i = 0; i < n; ++i){                                                                                  \
      out.set(i, lhs OP rhs.view(i));                                                                                  \
    }                                                                                                                  \
  } else if (n_threads > 1){                                                                                           \
    OMP_PARALLEL_FOR_SIMD(n_threads)                                                                                   \
    for (r_size_t i = 0; i < n; ++i){                                                                                  \
      out.set(i, lhs OP rhs.view(i));                                                                                  \
    }                                                                                                                  \
  } else {                                                                                                             \
    OMP_SIMD                                                                                                           \
    for (r_size_t i = 0; i < n; ++i){                                                                                  \
      out.set(i, lhs OP rhs.view(i));                                                                                  \
    }                                                                                                                  \
  }                                                                                                                    \
  return out;                                                                                                          \
}

template<RAtomicVector T, typename U>
inline T& operator+=(T& lhs, const U& rhs) {
    CPPALLY_BINARY_OP_IN_PLACE(+)
    return lhs;
}

template<RAtomicVector T, typename U>
inline T& operator-=(T& lhs, const U& rhs) {
    CPPALLY_BINARY_OP_IN_PLACE(-)
    return lhs;
}

template<RAtomicVector T, typename U>
inline T& operator*=(T& lhs, const U& rhs) {
    CPPALLY_BINARY_OP_IN_PLACE(*)
    return lhs;
}

template<RAtomicVector T, typename U>
inline T& operator/=(T& lhs, const U& rhs) {
    CPPALLY_BINARY_OP_IN_PLACE(/)
    return lhs;
}

template<RAtomicVector T, typename U>
inline T& operator%=(T& lhs, const U& rhs) {
    CPPALLY_BINARY_OP_IN_PLACE(%)
    return lhs;
}

namespace internal {

template <typename T>
concept RMathVector = RVector<T> && RMathType<typename std::remove_cvref_t<T>::data_type>;

template <RAtomicVector T, typename U>
bool use_in_place_ops(const T& lhs, const U& rhs) noexcept {
    if (!lhs.is_exclusive()){
        return false;
   }
   if constexpr (RAtomicVector<U>){
    return (lhs.length() == rhs.length()) || (lhs.length() > rhs.length() && rhs.length() != 0);
   }
   return true;
}
}

template <typename T, typename U>
requires (
    (internal::RMathVector<T> && internal::RMathVector<U>) ||
    (internal::RMathVector<T> && MathType<U>) ||
    (MathType<T> && internal::RMathVector<U>)
)
using common_math_vec_t = std::remove_cvref_t<r_vec<common_math_t<typename as_r_vector_t<T>::data_type, typename as_r_vector_t<U>::data_type>>>;

template<typename T, typename U>
requires (
    (internal::RMathVector<T> && internal::RMathVector<U>) ||
    (internal::RMathVector<T> && MathType<U>) ||
    (MathType<T> && internal::RMathVector<U>)
)
inline common_math_vec_t<T, U> operator+(T&& lhs, const U& rhs) {
    using out_t = common_math_vec_t<T, U>;
    if constexpr (std::is_same_v<T, out_t>){
        if (internal::use_in_place_ops(lhs, rhs)){
            lhs += rhs;
            return std::move(lhs);
        }
    }
    CPPALLY_BINARY_OP(lhs, rhs, +, out_t)
}

template<typename T, typename U>
requires (
    (internal::RMathVector<T> && internal::RMathVector<U>) ||
    (internal::RMathVector<T> && MathType<U>) ||
    (MathType<T> && internal::RMathVector<U>)
)
inline common_math_vec_t<T, U> operator-(T&& lhs, const U& rhs) {
    using out_t = common_math_vec_t<T, U>;
    if constexpr (std::is_same_v<T, out_t>){
        if (internal::use_in_place_ops(lhs, rhs)){
            lhs -= rhs;
            return std::move(lhs);
        }
    }
    CPPALLY_BINARY_OP(lhs, rhs, -, out_t)
}
template<typename T, typename U>
requires (
    (internal::RMathVector<T> && internal::RMathVector<U>) ||
    (internal::RMathVector<T> && MathType<U>) ||
    (MathType<T> && internal::RMathVector<U>)
)
inline common_math_vec_t<T, U> operator*(T&& lhs, const U& rhs) {
    using out_t = common_math_vec_t<T, U>;
    if constexpr (std::is_same_v<T, out_t>){
        if (internal::use_in_place_ops(lhs, rhs)){
            lhs *= rhs;
            return std::move(lhs);
        }
    }
    CPPALLY_BINARY_OP(lhs, rhs, *, out_t)
}
template<typename T, typename U>
requires (
    (internal::RMathVector<T> && internal::RMathVector<U>) ||
    (internal::RMathVector<T> && MathType<U>) ||
    (MathType<T> && internal::RMathVector<U>)
)
inline r_vec<r_dbl> operator/(T&& lhs, const U& rhs) {
    if constexpr (std::is_same_v<T, r_vec<r_dbl>>){
        if (internal::use_in_place_ops(lhs, rhs)){
            lhs /= rhs;
            return std::move(lhs);
        }
    }
    CPPALLY_BINARY_OP(lhs, rhs, /, r_vec<r_dbl>)
}
template<typename T, typename U>
requires (
    (internal::RMathVector<T> && internal::RMathVector<U>) ||
    (internal::RMathVector<T> && MathType<U>) ||
    (MathType<T> && internal::RMathVector<U>)
)
inline common_math_vec_t<T, U> operator%(T&& lhs, const U& rhs) {
    using out_t = common_math_vec_t<T, U>;
    if constexpr (std::is_same_v<T, out_t>){
        if (internal::use_in_place_ops(lhs, rhs)){
            lhs %= rhs;
            return std::move(lhs);
        }
    }
    CPPALLY_BINARY_OP(lhs, rhs, %, out_t)
}

template<typename T, typename U>
requires (
    (RAtomicVector<T> && RAtomicVector<U>) ||
    (RAtomicVector<T> && Scalar<U>) ||
    (Scalar<T> && RAtomicVector<U>)
)
inline r_vec<r_lgl> operator==(const T& lhs, const U& rhs) {
    CPPALLY_BINARY_OP(lhs, rhs, ==, r_vec<r_lgl>)
}
template<typename T, typename U>
requires (
    (RAtomicVector<T> && RAtomicVector<U>) ||
    (RAtomicVector<T> && Scalar<U>) ||
    (Scalar<T> && RAtomicVector<U>)
)
inline r_vec<r_lgl> operator!=(T&& lhs, const U& rhs) {
    if constexpr (std::is_same_v<T, r_vec<r_lgl>>){
        if (internal::use_in_place_ops(lhs, rhs)){
            CPPALLY_BINARY_OP_IN_PLACE(!=)
            return std::move(lhs);
        }
    }
    CPPALLY_BINARY_OP(lhs, rhs, !=, r_vec<r_lgl>)
}
template<typename T, typename U>
requires (
    (RAtomicVector<T> && RAtomicVector<U>) ||
    (RAtomicVector<T> && Scalar<U>) ||
    (Scalar<T> && RAtomicVector<U>)
)
inline r_vec<r_lgl> operator<=(T&& lhs, const U& rhs) {
    if constexpr (std::is_same_v<T, r_vec<r_lgl>>){
        if (internal::use_in_place_ops(lhs, rhs)){
            CPPALLY_BINARY_OP_IN_PLACE(<=)
            return std::move(lhs);
        }
    }
    CPPALLY_BINARY_OP(lhs, rhs, <=, r_vec<r_lgl>)
}
template<typename T, typename U>
requires (
    (RAtomicVector<T> && RAtomicVector<U>) ||
    (RAtomicVector<T> && Scalar<U>) ||
    (Scalar<T> && RAtomicVector<U>)
)
inline r_vec<r_lgl> operator<(T&& lhs, const U& rhs) {
    if constexpr (std::is_same_v<T, r_vec<r_lgl>>){
        if (internal::use_in_place_ops(lhs, rhs)){
            CPPALLY_BINARY_OP_IN_PLACE(<)
            return std::move(lhs);
        }
    }
    CPPALLY_BINARY_OP(lhs, rhs, <, r_vec<r_lgl>)
}
template<typename T, typename U>
requires (
    (RAtomicVector<T> && RAtomicVector<U>) ||
    (RAtomicVector<T> && Scalar<U>) ||
    (Scalar<T> && RAtomicVector<U>)
)
inline r_vec<r_lgl> operator>=(T&& lhs, const U& rhs) {
    if constexpr (std::is_same_v<T, r_vec<r_lgl>>){
        if (internal::use_in_place_ops(lhs, rhs)){
            CPPALLY_BINARY_OP_IN_PLACE(>=)
            return std::move(lhs);
        }
    }
    CPPALLY_BINARY_OP(lhs, rhs, >=, r_vec<r_lgl>)
}
template<typename T, typename U>
requires (
    (RAtomicVector<T> && RAtomicVector<U>) ||
    (RAtomicVector<T> && Scalar<U>) ||
    (Scalar<T> && RAtomicVector<U>)
)
inline r_vec<r_lgl> operator>(T&& lhs, const U& rhs) {
    if constexpr (std::is_same_v<T, r_vec<r_lgl>>){
        if (internal::use_in_place_ops(lhs, rhs)){
            CPPALLY_BINARY_OP_IN_PLACE(>)
            return std::move(lhs);
        }
    }
    CPPALLY_BINARY_OP(lhs, rhs, >, r_vec<r_lgl>)
}

template <typename T, typename U>
requires (
    (is<T, r_vec<r_lgl>> && is<U, r_vec<r_lgl>>) ||
    (is<T, r_vec<r_lgl>> && is<U, r_lgl>) ||
    (is<T, r_lgl> && is<U, r_vec<r_lgl>>)
)
inline r_vec<r_lgl> operator|(T&& lhs, const U& rhs) {
    if constexpr (std::is_same_v<T, r_vec<r_lgl>>){
        if (internal::use_in_place_ops(lhs, rhs)){
            CPPALLY_BINARY_OP_IN_PLACE(||)
            return std::move(lhs);
        }
    }
    CPPALLY_BINARY_OP(lhs, rhs, ||, r_vec<r_lgl>)
}
template <typename T, typename U>
requires (
    (is<T, r_vec<r_lgl>> && is<U, r_vec<r_lgl>>) ||
    (is<T, r_vec<r_lgl>> && is<U, r_lgl>) ||
    (is<T, r_lgl> && is<U, r_vec<r_lgl>>)
)
inline r_vec<r_lgl> operator&(T&& lhs, const U& rhs) {
    if constexpr (std::is_same_v<T, r_vec<r_lgl>>){
        if (internal::use_in_place_ops(lhs, rhs)){
            CPPALLY_BINARY_OP_IN_PLACE(&&)
            return std::move(lhs);
        }
    }
    CPPALLY_BINARY_OP(lhs, rhs, &&, r_vec<r_lgl>)
}

template <RVector T>
requires (is<T, r_vec<r_lgl>>)
inline r_vec<r_lgl> operator!(T&& x){
  if constexpr (std::is_same_v<T, r_vec<r_lgl>>){
    if (x.is_exclusive()){
        x.apply(
          [](r_lgl v){ return !v; },
          /*simd = */ true, 
          /*n_threads = */ internal::calc_threads(x.length())
        );
        return std::move(x);
    }
  }
  return x.map(
    [](r_lgl v){ return !v; },
    /*simd = */ true, 
    /*n_threads = */ internal::calc_threads(x.length())
  );
}

template <internal::RMathVector T>
inline std::remove_cvref_t<T> operator-(T&& x){
  using data_t = typename std::remove_cvref_t<T>::data_type;

  if constexpr (std::is_same_v<T, std::remove_cvref_t<T>>){
    if (x.is_exclusive()){
      x.apply(
      /*fn = */ [](auto v){ return -v; }, 
      /*simd = */ true, 
      /*n_threads = */ internal::calc_threads(x.length())
      );
      return std::move(x);
    }
  }
  return x.map(
    /*fn = */ [](data_t v){ return -v; }, 
    /*simd = */ true, 
    /*n_threads = */ internal::calc_threads(x.length())
  );
}

namespace internal {
// Helper to negate result of `==` in-place
template <typename T, typename U>
inline r_vec<r_lgl> not_equal(const T& lhs, const U& rhs){
    return !(lhs == rhs);
}
}


#undef CPPALLY_BINARY_OP
#undef CPPALLY_BINARY_OP_IN_PLACE

}

#endif
