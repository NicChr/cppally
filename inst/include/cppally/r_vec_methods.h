#ifndef CPPALLY_R_VEC_METHODS_H
#define CPPALLY_R_VEC_METHODS_H

#include <cppally/r_utils.h>
#include <cppally/r_vec.h>

// Vectorised binary operators: +,-,*,/,&,|,+=,-=,*=,/=,==,<=,<,>=,>
// Vectorised unary operators: !,-,

namespace cppally {

#define CPPALLY_BINARY_OP_IN_PLACE(OP)                        \
r_size_t lhs_size = lhs.length();                             \
if constexpr (RVector<U>){                                    \
  r_size_t rhs_size = rhs.length();                           \
  if (rhs_size == 1){                                         \
    return lhs OP##= rhs.get(0);                              \
  } else if (lhs_size == rhs_size){                           \
    {                                                         \
      int n_threads = internal::calc_threads(lhs_size);       \
      if (n_threads > 1){                                     \
        OMP_PARALLEL_FOR_SIMD(n_threads)                      \
        for (r_size_t i = 0; i < lhs_size; ++i){              \
          lhs.set(i, lhs.get(i) OP rhs.get(i));               \
        }                                                     \
      } else {                                                \
        OMP_SIMD                                              \
        for (r_size_t i = 0; i < lhs_size; ++i){              \
          lhs.set(i, lhs.get(i) OP rhs.get(i));               \
        }                                                     \
      }                                                       \
    }                                                         \
    return lhs;                                               \
  } else {                                                    \
    r_size_t n = lhs_size;                                    \
    for (r_size_t i = 0, rhsi = 0; i < n;                     \
    recycle_index(rhsi, rhs_size),                            \
    ++i){                                                     \
      lhs.set(i, lhs.get(i) OP rhs.get(rhsi));                \
    }                                                         \
    return lhs;                                               \
  }                                                           \
} else {                                                      \
  r_size_t n = lhs_size;                                      \
  int n_threads = internal::calc_threads(n);                  \
  if (n_threads > 1){                                         \
    OMP_PARALLEL_FOR_SIMD(n_threads)                          \
    for (r_size_t i = 0; i < n; ++i){                         \
      lhs.set(i, lhs.get(i) OP rhs);                          \
    }                                                         \
  } else {                                                    \
    OMP_SIMD                                                  \
    for (r_size_t i = 0; i < n; ++i){                         \
      lhs.set(i, lhs.get(i) OP rhs);                          \
    }                                                         \
  }                                                           \
  return lhs;                                                 \
}

template<RVector T, typename U>
inline T& operator+=(T& lhs, const U& rhs) {
    CPPALLY_BINARY_OP_IN_PLACE(+);
}

template<RVector T, typename U>
inline T& operator-=(T& lhs, const U& rhs) {
    CPPALLY_BINARY_OP_IN_PLACE(-);
}

template<RVector T, typename U>
inline T& operator*=(T& lhs, const U& rhs) {
    CPPALLY_BINARY_OP_IN_PLACE(*);
}

template<RVector T, typename U>
inline T& operator/=(T& lhs, const U& rhs) {
    CPPALLY_BINARY_OP_IN_PLACE(/);
}

template<RVector T, typename U>
inline T& operator%=(T& lhs, const U& rhs) {
    CPPALLY_BINARY_OP_IN_PLACE(%);
}

#define CPPALLY_BINARY_OP(lhs, rhs, OP, res_t)                                                       \
using lhs_t = decltype(lhs);                                                                       \
using rhs_t = decltype(rhs);                                                                       \
if constexpr (RVector<lhs_t> && RVector<rhs_t>){                                                   \
  r_size_t n1 = lhs.length();                                                                      \
  r_size_t n2 = rhs.length();                                                                      \
  r_size_t n = std::max(n1, n2);                                                                   \
  if (n1 == 0 || n2 == 0) return res_t();                                                          \
  int n_threads = internal::calc_threads(n);                                                       \
  if (n2 == 1){                                                                                    \
    res_t out(n);                                                                                  \
    auto val = rhs.view(0);                                                                        \
    if (n_threads > 1){                                                                            \
      OMP_PARALLEL_FOR_SIMD(n_threads)                                                             \
      for (r_size_t i = 0; i < n; ++i){                                                            \
        out.set(i, lhs.view(i) OP val);                                                            \
      }                                                                                            \
    } else {                                                                                       \
      OMP_SIMD                                                                                     \
      for (r_size_t i = 0; i < n; ++i){                                                            \
        out.set(i, lhs.view(i) OP val);                                                            \
      }                                                                                            \
    }                                                                                              \
    return out;                                                                                    \
  } else if (n1 == 1){                                                                             \
    res_t out(n);                                                                                  \
    auto val = lhs.view(0);                                                                        \
    if (n_threads > 1){                                                                            \
      OMP_PARALLEL_FOR_SIMD(n_threads)                                                             \
      for (r_size_t i = 0; i < n; ++i){                                                            \
        out.set(i, val OP rhs.view(i));                                                            \
      }                                                                                            \
    } else {                                                                                       \
      OMP_SIMD                                                                                     \
      for (r_size_t i = 0; i < n; ++i){                                                            \
        out.set(i, val OP rhs.view(i));                                                            \
      }                                                                                            \
    }                                                                                              \
    return out;                                                                                    \
  } else if (n1 == n2){                                                                            \
    res_t out(n);                                                                                  \
    if (n_threads > 1){                                                                            \
      OMP_PARALLEL_FOR_SIMD(n_threads)                                                             \
      for (r_size_t i = 0; i < n; ++i){                                                            \
        out.set(i, lhs.view(i) OP rhs.view(i));                                                    \
      }                                                                                            \
    } else {                                                                                       \
      OMP_SIMD                                                                                     \
      for (r_size_t i = 0; i < n; ++i){                                                            \
        out.set(i, lhs.view(i) OP rhs.view(i));                                                    \
      }                                                                                            \
    }                                                                                              \
    return out;                                                                                    \
  } else {                                                                                         \
    res_t out(n);                                                                                  \
    for (r_size_t i = 0, lhsi = 0, rhsi = 0; i < n;                                                \
    recycle_index(lhsi, n1),                                                                       \
    recycle_index(rhsi, n2),                                                                       \
    ++i){                                                                                          \
      out.set(i, lhs.view(lhsi) OP rhs.view(rhsi));                                                \
    }                                                                                              \
    return out;                                                                                    \
  }                                                                                                \
  /*Cases where one is a scalar*/                                                                  \
} else if constexpr (RVector<lhs_t>) {                                                             \
  r_size_t n = lhs.length();                                                                       \
  res_t out(n);                                                                                    \
  int n_threads = internal::calc_threads(n);                                                       \
  if (n_threads > 1){                                                                              \
    OMP_PARALLEL_FOR_SIMD(n_threads)                                                               \
    for (r_size_t i = 0; i < n; ++i){                                                              \
      out.set(i, lhs.view(i) OP rhs);                                                              \
    }                                                                                              \
  } else {                                                                                         \
    OMP_SIMD                                                                                       \
    for (r_size_t i = 0; i < n; ++i){                                                              \
      out.set(i, lhs.view(i) OP rhs);                                                              \
    }                                                                                              \
  }                                                                                                \
  return out;                                                                                      \
} else {                                                                                           \
  r_size_t n = rhs.length();                                                                       \
  res_t out(n);                                                                                    \
  int n_threads = internal::calc_threads(n);                                                       \
  if (n_threads > 1){                                                                              \
    OMP_PARALLEL_FOR_SIMD(n_threads)                                                               \
    for (r_size_t i = 0; i < n; ++i){                                                              \
      out.set(i, lhs OP rhs.view(i));                                                              \
    }                                                                                              \
  } else {                                                                                         \
    OMP_SIMD                                                                                       \
    for (r_size_t i = 0; i < n; ++i){                                                              \
      out.set(i, lhs OP rhs.view(i));                                                              \
    }                                                                                              \
  }                                                                                                \
  return out;                                                                                      \
}

namespace internal {
template <RVector T, typename U>
bool use_in_place_ops(const T& lhs, const U& rhs) noexcept {
    if (!lhs.value.is_exclusive()){
        return false;
   }
    if constexpr (RVector<U>){
        return lhs.length() >= rhs.length();
    }
    return true;
}
}

template<typename T, typename U>
requires (RVector<T> || RVector<U>)
inline common_r_t<as_r_composite_t<T>, as_r_composite_t<U>> operator+(T&& lhs, const U& rhs) {
    using out_t = common_r_t<as_r_composite_t<T>, as_r_composite_t<U>>;
    if constexpr (std::is_same_v<T, out_t>){
        if (internal::use_in_place_ops(lhs, rhs)){
            lhs += rhs;
            return std::move(lhs);
        }
    }
    CPPALLY_BINARY_OP(lhs, rhs, +, out_t)
}
template<typename T, typename U>
requires (RVector<T> || RVector<U>)
inline common_r_t<as_r_composite_t<T>, as_r_composite_t<U>> operator-(T&& lhs, const U& rhs) {
    using out_t = common_r_t<as_r_composite_t<T>, as_r_composite_t<U>>;
    if constexpr (std::is_same_v<T, out_t>){
        if (internal::use_in_place_ops(lhs, rhs)){
            lhs -= rhs;
            return std::move(lhs);
        }
    }
    CPPALLY_BINARY_OP(lhs, rhs, -, out_t)
}
template<typename T, typename U>
requires (RVector<T> || RVector<U>)
inline common_r_t<as_r_composite_t<T>, as_r_composite_t<U>> operator*(T&& lhs, const U& rhs) {
    using out_t = common_r_t<as_r_composite_t<T>, as_r_composite_t<U>>;
    if constexpr (std::is_same_v<T, out_t>){
        if (internal::use_in_place_ops(lhs, rhs)){
            lhs *= rhs;
            return std::move(lhs);
        }
    }
    CPPALLY_BINARY_OP(lhs, rhs, *, out_t)
}
template<typename T, typename U>
requires (RVector<T> || RVector<U>)
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
requires (RVector<T> || RVector<U>)
inline common_r_t<as_r_composite_t<T>, as_r_composite_t<U>> operator%(T&& lhs, const U& rhs) {
    using out_t = common_r_t<as_r_composite_t<T>, as_r_composite_t<U>>;
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
    (RAtomicVector<T> && RScalar<U>) ||
    (RScalar<T> && RAtomicVector<U>)
)
inline r_vec<r_lgl> operator==(const T& lhs, const U& rhs) {
    CPPALLY_BINARY_OP(lhs, rhs, ==, r_vec<r_lgl>)
}
template<typename T, typename U>
requires (
    (RAtomicVector<T> && RAtomicVector<U>) ||
    (RAtomicVector<T> && RScalar<U>) ||
    (RScalar<T> && RAtomicVector<U>)
)
inline r_vec<r_lgl> operator!=(const T& lhs, const U& rhs) {
    CPPALLY_BINARY_OP(lhs, rhs, !=, r_vec<r_lgl>)
}
template<typename T, typename U>
requires (
    (RAtomicVector<T> && RAtomicVector<U>) ||
    (RAtomicVector<T> && RScalar<U>) ||
    (RScalar<T> && RAtomicVector<U>)
)
inline r_vec<r_lgl> operator<=(const T& lhs, const U& rhs) {
    CPPALLY_BINARY_OP(lhs, rhs, <=, r_vec<r_lgl>)
}
template<typename T, typename U>
requires (
    (RAtomicVector<T> && RAtomicVector<U>) ||
    (RAtomicVector<T> && RScalar<U>) ||
    (RScalar<T> && RAtomicVector<U>)
)
inline r_vec<r_lgl> operator<(const T& lhs, const U& rhs) {
    CPPALLY_BINARY_OP(lhs, rhs, <, r_vec<r_lgl>)
}
template<typename T, typename U>
requires (
    (RAtomicVector<T> && RAtomicVector<U>) ||
    (RAtomicVector<T> && RScalar<U>) ||
    (RScalar<T> && RAtomicVector<U>)
)
inline r_vec<r_lgl> operator>=(const T& lhs, const U& rhs) {
    CPPALLY_BINARY_OP(lhs, rhs, >=, r_vec<r_lgl>)
}
template<typename T, typename U>
requires (
    (RAtomicVector<T> && RAtomicVector<U>) ||
    (RAtomicVector<T> && RScalar<U>) ||
    (RScalar<T> && RAtomicVector<U>)
)
inline r_vec<r_lgl> operator>(const T& lhs, const U& rhs) {
    CPPALLY_BINARY_OP(lhs, rhs, >, r_vec<r_lgl>)
}

template <typename T, typename U>
requires (
    (is<T, r_vec<r_lgl>> && is<U, r_vec<r_lgl>>) ||
    (is<T, r_vec<r_lgl>> && is<U, r_lgl>) ||
    (is<T, r_lgl> && is<U, r_vec<r_lgl>>)
)
inline r_vec<r_lgl> operator|(const T& lhs, const U& rhs) {
    CPPALLY_BINARY_OP(lhs, rhs, ||, r_vec<r_lgl>)
}
template <typename T, typename U>
requires (
    (is<T, r_vec<r_lgl>> && is<U, r_vec<r_lgl>>) ||
    (is<T, r_vec<r_lgl>> && is<U, r_lgl>) ||
    (is<T, r_lgl> && is<U, r_vec<r_lgl>>)
)
inline r_vec<r_lgl> operator&(const T& lhs, const U& rhs) {
    CPPALLY_BINARY_OP(lhs, rhs, &&, r_vec<r_lgl>)
}

inline r_vec<r_lgl> operator!(const r_vec<r_lgl>& x){
    r_size_t n = x.length();
    r_vec<r_lgl> out(n);
    int n_threads = internal::calc_threads(n);
    if (n_threads > 1){
        OMP_PARALLEL_FOR_SIMD(n_threads)
        for (r_size_t i = 0; i < n; ++i){
            out.set(i, !x.get(i));
        }
    } else {
        OMP_SIMD
        for (r_size_t i = 0; i < n; ++i){
            out.set(i, !x.get(i));
        }
    }
    return out;
}

template <RMathType T>
inline r_vec<T> operator-(const r_vec<T>& x){
    r_size_t n = x.length();
    r_vec<T> out(n);
    int n_threads = internal::calc_threads(n);
    if (n_threads > 1){
        OMP_PARALLEL_FOR_SIMD(n_threads)
        for (r_size_t i = 0; i < n; ++i){
            out.set(i, -x.get(i));
        }
    } else {
        OMP_SIMD
        for (r_size_t i = 0; i < n; ++i){
            out.set(i, -x.get(i));
        }
    }
    return out;
}

namespace internal {

// Helper to negative result of `==` in-place
template <typename T, typename U>
inline r_vec<r_lgl> not_equal(const T& lhs, const U& rhs){
    r_vec<r_lgl> eq = lhs == rhs;
    r_size_t n = eq.length();
    int n_threads = internal::calc_threads(n);
    if (n_threads > 1){
        OMP_PARALLEL_FOR_SIMD(n_threads)
        for (r_size_t i = 0; i < n; ++i){
            eq.set(i, !eq.get(i));
        }
    } else {
        OMP_SIMD
        for (r_size_t i = 0; i < n; ++i){
            eq.set(i, !eq.get(i));
        }
    }
    return eq;
}

}


#undef CPPALLY_BINARY_OP
#undef CPPALLY_BINARY_OP_IN_PLACE

}

#endif
