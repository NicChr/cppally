#ifndef CPP20_R_VEC_MATH_H
#define CPP20_R_VEC_MATH_H

#include <cpp20/r_utils.h>
#include <cpp20/r_vec.h>

namespace cpp20 {

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

#define CPP20_BINARY_OP_IN_PLACE(OP)                  \
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
    CPP20_BINARY_OP_IN_PLACE(+);
}

template<RVector T, typename U>
inline T& operator-=(T& lhs, const U& rhs) {
    CPP20_BINARY_OP_IN_PLACE(-);
}

template<RVector T, typename U>
inline T& operator*=(T& lhs, const U& rhs) {
    CPP20_BINARY_OP_IN_PLACE(*);
}

template<RVector T, typename U>
inline T& operator/=(T& lhs, const U& rhs) {
    CPP20_BINARY_OP_IN_PLACE(/);
}

template<RVector T, typename U>
inline T& operator%=(T& lhs, const U& rhs) {
    CPP20_BINARY_OP_IN_PLACE(%);
}

}


#endif
