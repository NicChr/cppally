#ifndef CPPALLY_R_VEC_METHODS_H
#define CPPALLY_R_VEC_METHODS_H

#include <cppally/r_utils.h>
#include <cppally/r_vec.h>

// Vectorised binary operators: +,-,*,/,&,|,+=,-=,*=,/=,==,<=,<,>=,>
// Vectorised unary operators: !,-,
// All operators are only for R vectors containing elements of Scalar

namespace cppally {

#define CPPALLY_UNARY_OP_IN_PLACE(OP, X)  \
r_size_t n = X.length();                  \
int n_threads = internal::calc_threads(n);\
if constexpr (RObject<typename std::remove_cvref_t<decltype(X)>::data_type>){ \
  for (r_size_t i = 0; i < n; ++i){       \
    X.set(i, OP X.view(i));               \
  }                                       \
} else if (n_threads > 1){                \
  OMP_PARALLEL_FOR_SIMD(n_threads)        \
  for (r_size_t i = 0; i < n; ++i){       \
    X.set(i, OP X.view(i));               \
  }                                       \
} else {                                  \
  OMP_SIMD                                \
  for (r_size_t i = 0; i < n; ++i){       \
    X.set(i, OP X.view(i));               \
  }                                       \
}

#define CPPALLY_UNARY_OP(OP, X)                \
r_size_t n = X.length();                       \
using out_t = std::remove_cvref_t<decltype(X)>;\
out_t out(n);                                  \
int n_threads = internal::calc_threads(n);     \
if constexpr (RObject<typename std::remove_cvref_t<decltype(out)>::data_type>){ \
  for (r_size_t i = 0; i < n; ++i){            \
    out.set(i, OP X.view(i));                  \
  }                                            \
} else if (n_threads > 1){                     \
  OMP_PARALLEL_FOR_SIMD(n_threads)             \
  for (r_size_t i = 0; i < n; ++i){            \
    out.set(i, OP X.view(i));                  \
  }                                            \
} else {                                       \
  OMP_SIMD                                     \
  for (r_size_t i = 0; i < n; ++i){            \
    out.set(i, OP X.view(i));                  \
  }                                            \
}                                              \
return out;

#define CPPALLY_BINARY_OP_IN_PLACE(OP)                                            \
r_size_t lhs_size = lhs.length();                                                 \
if constexpr (RAtomicVector<U>){                                                        \
  r_size_t rhs_size = rhs.length();                                               \
  if (rhs_size == 1){                                                             \
      auto val = rhs.view(0);                                                     \
      int n_threads = internal::calc_threads(lhs_size);                           \
      if constexpr (RObject<typename std::remove_cvref_t<decltype(lhs)>::data_type>){                 \
        for (r_size_t i = 0; i < lhs_size; ++i){ lhs.set(i, lhs.view(i) OP val); }\
      } else if (n_threads > 1){                                                  \
        OMP_PARALLEL_FOR_SIMD(n_threads)                                          \
        for (r_size_t i = 0; i < lhs_size; ++i){ lhs.set(i, lhs.view(i) OP val); }\
      } else {                                                                    \
        OMP_SIMD                                                                  \
        for (r_size_t i = 0; i < lhs_size; ++i){ lhs.set(i, lhs.view(i) OP val); }\
      }                                                                           \
  } else if (lhs_size == rhs_size){                                               \
    {                                                                             \
      int n_threads = internal::calc_threads(lhs_size);                           \
      if constexpr (RObject<typename std::remove_cvref_t<decltype(lhs)>::data_type>){                 \
        for (r_size_t i = 0; i < lhs_size; ++i){                                  \
          lhs.set(i, lhs.get(i) OP rhs.get(i));                                   \
        }                                                                         \
      } else if (n_threads > 1){                                                  \
        OMP_PARALLEL_FOR_SIMD(n_threads)                                          \
        for (r_size_t i = 0; i < lhs_size; ++i){                                  \
          lhs.set(i, lhs.get(i) OP rhs.get(i));                                   \
        }                                                                         \
      } else {                                                                    \
        OMP_SIMD                                                                  \
        for (r_size_t i = 0; i < lhs_size; ++i){                                  \
          lhs.set(i, lhs.get(i) OP rhs.get(i));                                   \
        }                                                                         \
      }                                                                           \
    }                                                                             \
  } else {                                                                        \
    r_size_t n = lhs_size;                                                        \
    for (r_size_t i = 0, rhsi = 0; i < n;                                         \
    recycle_index(rhsi, rhs_size),                                                \
    ++i){                                                                         \
      lhs.set(i, lhs.get(i) OP rhs.get(rhsi));                                    \
    }                                                                             \
  }                                                                               \
} else {                                                                          \
  r_size_t n = lhs_size;                                                          \
  int n_threads = internal::calc_threads(n);                                      \
  if constexpr (RObject<typename std::remove_cvref_t<decltype(lhs)>::data_type>){                     \
    for (r_size_t i = 0; i < n; ++i){                                             \
      lhs.set(i, lhs.get(i) OP rhs);                                              \
    }                                                                             \
  } else if (n_threads > 1){                                                      \
    OMP_PARALLEL_FOR_SIMD(n_threads)                                              \
    for (r_size_t i = 0; i < n; ++i){                                             \
      lhs.set(i, lhs.get(i) OP rhs);                                              \
    }                                                                             \
  } else {                                                                        \
    OMP_SIMD                                                                      \
    for (r_size_t i = 0; i < n; ++i){                                             \
      lhs.set(i, lhs.get(i) OP rhs);                                              \
    }                                                                             \
  }                                                                               \
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

#define CPPALLY_BINARY_OP(lhs, rhs, OP, res_t)                                                       \
using lhs_t = decltype(lhs);                                                                       \
using rhs_t = decltype(rhs);                                                                       \
if constexpr (RAtomicVector<lhs_t> && RAtomicVector<rhs_t>){                                                   \
  r_size_t n1 = lhs.length();                                                                      \
  r_size_t n2 = rhs.length();                                                                      \
  r_size_t n = std::max(n1, n2);                                                                   \
  if (n1 == 0 || n2 == 0) return res_t();                                                          \
  int n_threads = internal::calc_threads(n);                                                       \
  if (n2 == 1){                                                                                    \
    res_t out(n);                                                                                  \
    auto val = rhs.view(0);                                                                        \
    if constexpr (RObject<typename std::remove_cvref_t<decltype(out)>::data_type>){                                    \
      for (r_size_t i = 0; i < n; ++i){                                                            \
        out.set(i, lhs.view(i) OP val);                                                            \
      }                                                                                            \
    } else if (n_threads > 1){                                                                     \
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
    if constexpr (RObject<typename std::remove_cvref_t<decltype(out)>::data_type>){                                    \
      for (r_size_t i = 0; i < n; ++i){                                                            \
        out.set(i, val OP rhs.view(i));                                                            \
      }                                                                                            \
    } else if (n_threads > 1){                                                                     \
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
    if constexpr (RObject<typename std::remove_cvref_t<decltype(out)>::data_type>){                                    \
      for (r_size_t i = 0; i < n; ++i){                                                            \
        out.set(i, lhs.view(i) OP rhs.view(i));                                                    \
      }                                                                                            \
    } else if (n_threads > 1){                                                                     \
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
} else if constexpr (RAtomicVector<lhs_t>) {                                                             \
  r_size_t n = lhs.length();                                                                       \
  res_t out(n);                                                                                    \
  int n_threads = internal::calc_threads(n);                                                       \
  if constexpr (RObject<typename std::remove_cvref_t<decltype(out)>::data_type>){                                      \
    for (r_size_t i = 0; i < n; ++i){                                                              \
      out.set(i, lhs.view(i) OP rhs);                                                              \
    }                                                                                              \
  } else if (n_threads > 1){                                                                       \
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
  if constexpr (RObject<typename std::remove_cvref_t<decltype(out)>::data_type>){                                      \
    for (r_size_t i = 0; i < n; ++i){                                                              \
      out.set(i, lhs OP rhs.view(i));                                                              \
    }                                                                                              \
  } else if (n_threads > 1){                                                                       \
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

template <RAtomicVector T, typename U>
bool use_in_place_ops(const T& lhs, const U& rhs) noexcept {
    if (!lhs.is_exclusive()){
        return false;
   }
    if constexpr (RAtomicVector<U>){
        return lhs.length() >= rhs.length();
    }
    return true;
}
}

template<typename T, typename U>
requires (
    (RAtomicVector<T> && RAtomicVector<U>) ||
    (RAtomicVector<T> && Scalar<U>) ||
    (Scalar<T> && RAtomicVector<U>)
)
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
requires (
    (RAtomicVector<T> && RAtomicVector<U>) ||
    (RAtomicVector<T> && Scalar<U>) ||
    (Scalar<T> && RAtomicVector<U>)
)
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
requires (
    (RAtomicVector<T> && RAtomicVector<U>) ||
    (RAtomicVector<T> && Scalar<U>) ||
    (Scalar<T> && RAtomicVector<U>)
)
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
requires (
    (RAtomicVector<T> && RAtomicVector<U>) ||
    (RAtomicVector<T> && Scalar<U>) ||
    (Scalar<T> && RAtomicVector<U>)
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
    (RAtomicVector<T> && RAtomicVector<U>) ||
    (RAtomicVector<T> && Scalar<U>) ||
    (Scalar<T> && RAtomicVector<U>)
)
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

inline r_vec<r_lgl> operator!(r_vec<r_lgl>&& x){
    if (x.is_exclusive()){
        CPPALLY_UNARY_OP_IN_PLACE(!, x)
        return std::move(x);
    } else {
        CPPALLY_UNARY_OP(!, x)
    }
}

template <RMathType T>
inline r_vec<T> operator-(r_vec<T>&& x){
    if (x.is_exclusive()){
        CPPALLY_UNARY_OP_IN_PLACE(-, x)
        return std::move(x);
    } else {
        CPPALLY_UNARY_OP(-, x)
    }
}

namespace internal {

// Helper to negative result of `==` in-place
template <typename T, typename U>
inline r_vec<r_lgl> not_equal(const T& lhs, const U& rhs){
    r_vec<r_lgl> eq = lhs == rhs;
    CPPALLY_UNARY_OP_IN_PLACE(!, eq)
    return eq;
}

}


#undef CPPALLY_BINARY_OP
#undef CPPALLY_BINARY_OP_IN_PLACE

}

#endif
