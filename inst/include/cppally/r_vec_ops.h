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

#define CPPALLY_BINARY_OP(lhs, rhs, OP, res_t)                                                                                  \
using lhs_t = decltype(lhs);                                                                                                    \
using rhs_t = decltype(rhs);                                                                                                    \
if constexpr (RAtomicVector<lhs_t> && RAtomicVector<rhs_t>){                                                                    \
  if (rhs.length() == 1){                                                                                                       \
    auto val = rhs.view(0);                                                                                                     \
    return pmap_parallel_simd([val](auto a) noexcept { return a OP val; }, lhs);                                                \
  } else if (lhs.length() == 1){                                                                                                \
    auto val = lhs.view(0);                                                                                                     \
    return pmap_parallel_simd([val](auto b) noexcept { return val OP b; }, rhs);                                                \
  } else {                                                                                                                      \
    return pmap_parallel_simd([](auto a, auto b) noexcept { return a OP b; }, lhs, rhs);                                        \
  }                                                                                                                             \
  /*Cases where one is a scalar*/                                                                                               \
} else if constexpr (RAtomicVector<lhs_t>) {                                                                                    \
  return pmap_parallel_simd([rhs](auto a) noexcept { return a OP rhs; }, lhs);                                                  \
} else {                                                                                                                        \
  return pmap_parallel_simd([lhs](auto b) noexcept { return lhs OP b; }, rhs);                                                  \
}

#define CPPALLY_BINARY_OP_IN_PLACE(OP)                                                                                                               \
r_size_t lhs_size = lhs.length();                                                                                                                    \
if constexpr (RAtomicVector<U>){                                                                                                                     \
  r_size_t rhs_size = rhs.length();                                                                                                                  \
  if (rhs_size == 1){                                                                                                                                \
    auto val = rhs.view(0);                                                                                                                          \
    lhs.apply([val](auto a) noexcept { return a OP val; }, true, true);                                                                              \
  } else if (lhs_size == rhs_size){                                                                                                                  \
    const auto *p_rhs = rhs.data();                                                                                                                  \
    using rhs_data_t = typename std::remove_cvref_t<U>::data_type;                                                                                   \
    lhs.apply_with_index([p_rhs](r_size_t i, auto a) noexcept { return a OP rhs_data_t(p_rhs[i]); }, true, true);                                    \
  } else {                                                                                                                                           \
    r_size_t rhsi = 0;                                                                                                                               \
    lhs.apply_with_index([&rhs, &rhsi, rhs_size](r_size_t i, auto a) noexcept {                                                                      \
      auto r = rhs.view(rhsi);                                                                                                                       \
      recycle_index(rhsi, rhs_size);                                                                                                                 \
      return a OP r;                                                                                                                                 \
    });                                                                                                                                              \
  }                                                                                                                                                  \
} else {                                                                                                                                             \
  lhs.apply([rhs](auto a) noexcept { return a OP rhs; }, true, true);                                                                                \
}

template <RAtomicVector T, typename U>
requires (RNumber<typename T::data_type>)
inline T& operator+=(T& lhs, const U& rhs) {
    CPPALLY_BINARY_OP_IN_PLACE(+=)
    return lhs;
}

template <RAtomicVector T, typename U>
requires (RNumber<typename T::data_type>)
inline T& operator-=(T& lhs, const U& rhs) {
    CPPALLY_BINARY_OP_IN_PLACE(-=)
    return lhs;
}

template <RAtomicVector T, typename U>
requires (RNumber<typename T::data_type>)
inline T& operator*=(T& lhs, const U& rhs) {
    CPPALLY_BINARY_OP_IN_PLACE(*=)
    return lhs;
}

template <RAtomicVector T, typename U>
requires (RNumber<typename T::data_type>)
inline T& operator/=(T& lhs, const U& rhs) {
    CPPALLY_BINARY_OP_IN_PLACE(/=)
    return lhs;
}

template <RAtomicVector T, typename U>
requires (RNumber<typename T::data_type>)
inline T& operator%=(T& lhs, const U& rhs) {
    CPPALLY_BINARY_OP_IN_PLACE(%=)
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
          [](r_lgl v) noexcept { return !v; },
          /*simd = */ true,
          /*parallel = */ true
        );
        return std::move(x);
    }
  }
  return pmap_parallel_simd([](r_lgl v) noexcept { return !v; }, x);
}

template <RAtomicVector T>
requires (RNumber<typename std::remove_cvref_t<T>::data_type>)
inline std::remove_cvref_t<T> operator+(T&& x){
    return std::forward<T>(x);
}

template <RAtomicVector T>
requires (RNumber<typename std::remove_cvref_t<T>::data_type>)
inline std::remove_cvref_t<T> operator-(T&& x){
  using data_t = typename std::remove_cvref_t<T>::data_type;

  if constexpr (std::is_same_v<T, std::remove_cvref_t<T>>){
    if (x.is_exclusive()){
      x.apply(
      /*fn = */ [](auto v) noexcept { return -v; },
      /*simd = */ true,
      /*parallel = */ true
      );
      return std::move(x);
    }
  }
  return pmap_parallel_simd([](data_t v) noexcept { return -v; }, x);
}

inline r_vec<r_int> operator+(const r_vec<r_lgl>& x){
    return pmap_parallel_simd([](r_lgl v) noexcept { return +v; }, x);
}
inline r_vec<r_int> operator-(const r_vec<r_lgl>& x){
    return pmap_parallel_simd([](r_lgl v) noexcept { return -v; }, x);
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
