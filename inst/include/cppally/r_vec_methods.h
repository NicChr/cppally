#ifndef CPPALLY_R_VEC_METHODS_H
#define CPPALLY_R_VEC_METHODS_H

#include <cppally/r_utils.h>
#include <cppally/r_vec.h>

// Vectorised binary operators: +,-,*,/,&,|,+=,-=,*=,/=,==,<=,<,>=,>
// Vectorised unary operators: !,-,

namespace cppally {

template<typename T, typename U>
requires (RVector<T> || RVector<U>)
inline auto operator+(const T& lhs, const U& rhs) {
    if constexpr (RVector<T> && RVector<U>){
        r_size_t lhs_size = lhs.length();
        r_size_t rhs_size = rhs.length();
        if (lhs_size == 1){
            return lhs.get(0) + rhs;
        } else if (rhs_size == 1){
            return lhs + rhs.get(0);
        } else if (lhs_size == rhs_size){
            using common_t = common_math_t<typename T::data_type, typename U::data_type>;
            r_vec<common_t> out(lhs_size);
            OMP_SIMD
            for (r_size_t i = 0; i < lhs_size; ++i){
                out.set(i, lhs.get(i) + rhs.get(i));
            }
            return out;
        } else {
            // Slower recycling approach
            r_size_t n = std::max(lhs_size, rhs_size);
            if (lhs_size == 0 || rhs_size == 0){
                n = 0;
            }
            using common_t = common_math_t<typename T::data_type, typename U::data_type>;
            r_vec<common_t> out(n);
            for (r_size_t i = 0, lhsi = 0, rhsi = 0; i < n;
                recycle_index(lhsi, lhs_size),
                recycle_index(rhsi, rhs_size),
                ++i){
                out.set(i, lhs.get(lhsi) + rhs.get(rhsi));
            }
            return out;
        }
    } else if constexpr (RVector<T>){
        using common_t = common_math_t<typename T::data_type, U>;
        r_size_t n = lhs.length();
        r_vec<common_t> out(n);
        OMP_SIMD
        for (r_size_t i = 0; i < n; ++i){
            out.set(i, lhs.get(i) + rhs);
        }
        return out;
    } else {
        using common_t = common_math_t<T, typename U::data_type>;
        r_size_t n = rhs.length();
        r_vec<common_t> out(n);
        OMP_SIMD
        for (r_size_t i = 0; i < n; ++i){
            out.set(i, rhs.get(i) + lhs);
        }
        return out;
    }
}

template<typename T, typename U>
requires (RVector<T> || RVector<U>)
inline auto operator-(const T& lhs, const U& rhs) {
    if constexpr (RVector<T> && RVector<U>){
        r_size_t lhs_size = lhs.length();
        r_size_t rhs_size = rhs.length();
        if (lhs_size == 1){
            return lhs.get(0) - rhs;
        } else if (rhs_size == 1){
            return lhs - rhs.get(0);
        } else if (lhs_size == rhs_size){
            using common_t = common_math_t<typename T::data_type, typename U::data_type>;
            r_vec<common_t> out(lhs_size);
            OMP_SIMD
            for (r_size_t i = 0; i < lhs_size; ++i){
                out.set(i, lhs.get(i) - rhs.get(i));
            }
            return out;
        } else {
            // Slower recycling approach
            r_size_t n = std::max(lhs_size, rhs_size);
            if (lhs_size == 0 || rhs_size == 0){
                n = 0;
            }
            using common_t = common_math_t<typename T::data_type, typename U::data_type>;
            r_vec<common_t> out(n);
            for (r_size_t i = 0, lhsi = 0, rhsi = 0; i < n;
                recycle_index(lhsi, lhs_size),
                recycle_index(rhsi, rhs_size),
                ++i){
                out.set(i, lhs.get(lhsi) - rhs.get(rhsi));
            }
            return out;
        }
    } else if constexpr (RVector<T>){
        using common_t = common_math_t<typename T::data_type, U>;
        r_size_t n = lhs.length();
        r_vec<common_t> out(n);
        OMP_SIMD
        for (r_size_t i = 0; i < n; ++i){
            out.set(i, lhs.get(i) - rhs);
        }
        return out;
    } else {
        using common_t = common_math_t<T, typename U::data_type>;
        r_size_t n = rhs.length();
        r_vec<common_t> out(n);
        OMP_SIMD
        for (r_size_t i = 0; i < n; ++i){
            out.set(i, rhs.get(i) - lhs);
        }
        return out;
    }
}

template<typename T, typename U>
requires (RVector<T> || RVector<U>)
inline auto operator*(const T& lhs, const U& rhs) {
    if constexpr (RVector<T> && RVector<U>){
        r_size_t lhs_size = lhs.length();
        r_size_t rhs_size = rhs.length();
        if (lhs_size == 1){
            return lhs.get(0) * rhs;
        } else if (rhs_size == 1){
            return lhs * rhs.get(0);
        } else if (lhs_size == rhs_size){
            using common_t = common_math_t<typename T::data_type, typename U::data_type>;
            r_vec<common_t> out(lhs_size);
            OMP_SIMD
            for (r_size_t i = 0; i < lhs_size; ++i){
                out.set(i, lhs.get(i) * rhs.get(i));
            }
            return out;
        } else {
            // Slower recycling approach
            r_size_t n = std::max(lhs_size, rhs_size);
            if (lhs_size == 0 || rhs_size == 0){
                n = 0;
            }
            using common_t = common_math_t<typename T::data_type, typename U::data_type>;
            r_vec<common_t> out(n);
            for (r_size_t i = 0, lhsi = 0, rhsi = 0; i < n;
                recycle_index(lhsi, lhs_size),
                recycle_index(rhsi, rhs_size),
                ++i){
                out.set(i, lhs.get(lhsi) * rhs.get(rhsi));
            }
            return out;
        }
    } else if constexpr (RVector<T>){
        using common_t = common_math_t<typename T::data_type, U>;
        r_size_t n = lhs.length();
        r_vec<common_t> out(n);
        OMP_SIMD
        for (r_size_t i = 0; i < n; ++i){
            out.set(i, lhs.get(i) * rhs);
        }
        return out;
    } else {
        using common_t = common_math_t<T, typename U::data_type>;
        r_size_t n = rhs.length();
        r_vec<common_t> out(n);
        OMP_SIMD
        for (r_size_t i = 0; i < n; ++i){
            out.set(i, rhs.get(i) * lhs);
        }
        return out;
    }
}

template<typename T, typename U>
requires (RVector<T> || RVector<U>)
inline r_vec<r_dbl> operator/(const T& lhs, const U& rhs) {

    if constexpr (RVector<T> && RVector<U>){
        r_size_t lhs_size = lhs.length();
        r_size_t rhs_size = rhs.length();

        if (lhs_size == 1){
            return lhs.get(0) / rhs;
        } else if (rhs_size == 1){
            return lhs / rhs.get(0);
        } else if (lhs_size == rhs_size){
            r_vec<r_dbl> out(lhs_size);
            OMP_SIMD
            for (r_size_t i = 0; i < lhs_size; ++i){
                out.set(i, lhs.get(i) / rhs.get(i));
            }
            return out;
        } else {
            // Slower recycling approach
            r_size_t n = std::max(lhs_size, rhs_size);
            if (lhs_size == 0 || rhs_size == 0){
                n = 0;
            }
            r_vec<r_dbl> out(n);
            for (r_size_t i = 0, lhsi = 0, rhsi = 0; i < n;
                recycle_index(lhsi, lhs_size),
                recycle_index(rhsi, rhs_size),
                ++i){
                out.set(i, lhs.get(lhsi) / rhs.get(rhsi));
            }
            return out;
        }
    } else if constexpr (RVector<T>){
        r_size_t n = lhs.length();
        r_vec<r_dbl> out(n);
        OMP_SIMD
        for (r_size_t i = 0; i < n; ++i){
            out.set(i, lhs.get(i) / rhs);
        }
        return out;
    } else {
        r_size_t n = rhs.length();
        r_vec<r_dbl> out(n);
        OMP_SIMD
        for (r_size_t i = 0; i < n; ++i){
            out.set(i, rhs.get(i) / lhs);
        }
        return out;
    }
}

template<typename T, typename U>
requires (RVector<T> || RVector<U>)
inline auto operator%(const T& lhs, const U& rhs) {
    if constexpr (RVector<T> && RVector<U>){
        r_size_t lhs_size = lhs.length();
        r_size_t rhs_size = rhs.length();
        if (lhs_size == 1){
            return lhs.get(0) % rhs;
        } else if (rhs_size == 1){
            return lhs % rhs.get(0);
        } else if (lhs_size == rhs_size){
            using common_t = common_math_t<typename T::data_type, typename U::data_type>;
            r_vec<common_t> out(lhs_size);
            OMP_SIMD
            for (r_size_t i = 0; i < lhs_size; ++i){
                out.set(i, lhs.get(i) % rhs.get(i));
            }
            return out;
        } else {
            // Slower recycling approach
            r_size_t n = std::max(lhs_size, rhs_size);
            if (lhs_size == 0 || rhs_size == 0){
                n = 0;
            }
            using common_t = common_math_t<typename T::data_type, typename U::data_type>;
            r_vec<common_t> out(n);
            for (r_size_t i = 0, lhsi = 0, rhsi = 0; i < n;
                recycle_index(lhsi, lhs_size),
                recycle_index(rhsi, rhs_size),
                ++i){
                out.set(i, lhs.get(lhsi) % rhs.get(rhsi));
            }
            return out;
        }
    } else if constexpr (RVector<T>){
        using common_t = common_math_t<typename T::data_type, U>;
        r_size_t n = lhs.length();
        r_vec<common_t> out(n);
        OMP_SIMD
        for (r_size_t i = 0; i < n; ++i){
            out.set(i, lhs.get(i) % rhs);
        }
        return out;
    } else {
        using common_t = common_math_t<T, typename U::data_type>;
        r_size_t n = rhs.length();
        r_vec<common_t> out(n);
        OMP_SIMD
        for (r_size_t i = 0; i < n; ++i){
            out.set(i, rhs.get(i) % lhs);
        }
        return out;
    }
}

#define CPPALLY_BINARY_OP_IN_PLACE(OP)                  \
r_size_t lhs_size = lhs.length();                     \
if constexpr (RVector<U>){                            \
  r_size_t rhs_size = rhs.length();                   \
  if (rhs_size == 1){                                 \
    return lhs OP##= rhs.get(0);                      \
  } else if (lhs_size == rhs_size){                   \
    OMP_SIMD                                          \
    for (r_size_t i = 0; i < lhs_size; ++i){          \
      lhs.set(i, lhs.get(i) OP rhs.get(i));           \
    }                                                 \
    return lhs;                                       \
  } else {                                            \
    r_size_t n = lhs_size;                            \
    for (r_size_t i = 0, rhsi = 0; i < n;             \
    recycle_index(rhsi, rhs_size),                    \
    ++i){                                             \
      lhs.set(i, lhs.get(i) OP rhs.get(rhsi));        \
    }                                                 \
    return lhs;                                       \
  }                                                   \
} else {                                              \
  r_size_t n = lhs_size;                              \
  OMP_SIMD                                            \
  for (r_size_t i = 0; i < n; ++i){                   \
    lhs.set(i, lhs.get(i) OP rhs);                    \
  }                                                   \
  return lhs;                                         \
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
  if (n2 == 1){                                                                                    \
    res_t out(n);                                                                                  \
    auto val = rhs.view(0);                                                                        \
    OMP_SIMD                                                                                       \
    for (r_size_t i = 0; i < n; ++i){                                                              \
      out.set(i, lhs.view(i) OP val);                                                              \
    }                                                                                              \
    return out;                                                                                    \
  } else if (n1 == 1){                                                                             \
    res_t out(n);                                                                                  \
    auto val = lhs.view(0);                                                                        \
    OMP_SIMD                                                                                       \
    for (r_size_t i = 0; i < n; ++i){                                                              \
      out.set(i, val OP rhs.view(i));                                                              \
    }                                                                                              \
    return out;                                                                                    \
  } else if (n1 == n2){                                                                            \
    res_t out(n);                                                                                  \
    OMP_SIMD                                                                                       \
    for (r_size_t i = 0; i < n; ++i){                                                              \
      out.set(i, lhs.view(i) OP rhs.view(i));                                                      \
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
  OMP_SIMD                                                                                         \
  for (r_size_t i = 0; i < n; ++i){                                                                \
    out.set(i, lhs.view(i) OP rhs);                                                                \
  }                                                                                                \
  return out;                                                                                      \
} else {                                                                                           \
  r_size_t n = rhs.length();                                                                       \
  res_t out(n);                                                                                    \
  OMP_SIMD                                                                                         \
  for (r_size_t i = 0; i < n; ++i){                                                                \
    out.set(i, lhs OP rhs.view(i));                                                                \
  }                                                                                                \
  return out;                                                                                      \
}

template<typename T, typename U>
requires (RVector<T> || RVector<U>)
inline r_vec<r_lgl> operator==(const T& lhs, const U& rhs) {
    CPPALLY_BINARY_OP(lhs, rhs, ==, r_vec<r_lgl>)
}
template<typename T, typename U>
requires (RVector<T> || RVector<U>)
inline r_vec<r_lgl> operator!=(const T& lhs, const U& rhs) {
    CPPALLY_BINARY_OP(lhs, rhs, !=, r_vec<r_lgl>)
}
template<typename T, typename U>
requires (RVector<T> || RVector<U>)
inline r_vec<r_lgl> operator<=(const T& lhs, const U& rhs) {
    CPPALLY_BINARY_OP(lhs, rhs, <=, r_vec<r_lgl>)
}
template<typename T, typename U>
requires (RVector<T> || RVector<U>)
inline r_vec<r_lgl> operator<(const T& lhs, const U& rhs) {
    CPPALLY_BINARY_OP(lhs, rhs, <, r_vec<r_lgl>)
}
template<typename T, typename U>
requires (RVector<T> || RVector<U>)
inline r_vec<r_lgl> operator>=(const T& lhs, const U& rhs) {
    CPPALLY_BINARY_OP(lhs, rhs, >=, r_vec<r_lgl>)
}
template<typename T, typename U>
requires (RVector<T> || RVector<U>)
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
    OMP_SIMD
    for (r_size_t i = 0; i < n; ++i){
        out.set(i, !x.view(i));
    }
    return out;
}

template <RMathType T>
inline r_vec<T> operator-(const r_vec<T>& x){
    r_size_t n = x.length();
    r_vec<T> out(n);
    OMP_SIMD
    for (r_size_t i = 0; i < n; ++i){
        out.set(i, -x.view(i));
    }
    return out;
}


#undef CPPALLY_BINARY_OP
#undef CPPALLY_BINARY_OP_IN_PLACE

}

#endif
