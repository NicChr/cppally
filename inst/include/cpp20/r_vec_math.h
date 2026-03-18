#ifndef CPP20_R_VEC_MATH_H
#define CPP20_R_VEC_MATH_H

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

template<RIntegerType T>
T gcd(const r_vec<T> &x, bool na_rm = false, T tol = r_limits<T>::tolerance()){
  if (tol < 0 || tol >= 1){
    abort("`tol` must be >= 0 and < 1");
  }
  r_size_t n = x.length();

  if (n == 0){
    return na<T>();
  }

  auto out = x.get(0);
  for (r_size_t i = 1; i < n; ++i) {
      out = gcd(out, x.get(i), na_rm);
      if (!na_rm && is_na(out)){
          break;
      } else if (out == 1){
          break;
      }
  }
  return out;
}

template<RMathType T>
T gcd(const r_vec<T> &x, bool na_rm = false, T tol = r_limits<T>::tolerance()){
  if (tol < 0 || tol >= 1){
    abort("`tol` must be >= 0 and < 1");
  }
  r_size_t n = x.length();

  if (n == 0){
    return na<T>();
  }

  auto out = x.get(0);
  for (r_size_t i = 1; i < n; ++i) {
      out = gcd(out, x.get(i), na_rm);
      if (!na_rm && is_na(out)){
          break;
      }
      if (out > T(0.0) && out < (tol + tol)){
        out = tol;
        break;
      }
  }
  return out;
}

template<RMathType T>
T lcm(const r_vec<T> &x, bool na_rm = false, T tol = r_limits<T>::tolerance()){
    if (tol < 0 || tol >= 1){
      Rf_error("tol must be >= 0 and < 1");
    }
    r_size_t n = x.length();
    if (n == 0){
        return na<T>();
    }

    // Initialise first value as lcm
    T out = x.get(0);

    for (r_size_t i = 1; i < n; ++i) {
    if (!na_rm && is_na(out)){
        break;
    } else if (is_pos_inf(out)){
        break;
    }
    out = lcm(out, x.get(i), na_rm, tol);
    }
    return out;
  }


// r_lgl not bool because bool can't be NA
template <RVal T>
inline r_lgl all_whole_numbers(r_vec<T> x, bool na_rm = false, r_dbl tol = r_limits<r_dbl>::tolerance()){

    if constexpr (RIntegerType<T>){
        return r_true;
    } else if constexpr (RFloatType<T>){

        r_size_t n = x.length();

        r_lgl out = r_true;
        r_size_t na_count = 0;

        for (r_size_t i = 0; i < n; ++i) {
            out = is_whole_number(x.get(i), tol);
            na_count += is_na(out);
            if (out == r_false){
                break;
            }
        }
        if (out == r_true && !na_rm && na_count > 0){
            out = r_na;
        } else if (na_rm && na_count == n){
            out = r_true;
        }
        return out;
    } else {
        return r_false;
    }
}

template<RVector T, typename U>
inline r_vec<r_lgl> between(const T& x, const U& lo, const U& hi) {

    r_size_t x_size = x.length();

    if constexpr (RVector<U>){
        r_size_t lo_size = lo.length();
        r_size_t hi_size = hi.length();
        
        if (lo_size == 1 && hi_size == 1){
            return between(x, unwrap(lo.get(0)), unwrap(hi.get(0)));
        } else if (x_size == lo_size && lo_size == hi_size){
            r_vec<r_lgl> out(x_size);
            OMP_SIMD
            for (r_size_t i = 0; i < x_size; ++i){
                out.set(i, between(x.get(i), unwrap(lo.get(i)), unwrap(hi.get(i))));
            }
            return out;
        } else {
            // Slower recycling approach
            r_size_t n = std::max(std::max(x_size, lo_size), hi_size);
            if (x_size == 0 || lo_size == 0 || hi_size == 0){
                n = 0;
            }
            r_vec<r_lgl> out(n);
            for (r_size_t i = 0, xi = 0, loi = 0, hii = 0; i < n;
                recycle_index(xi, x_size),
                recycle_index(loi, lo_size),
                recycle_index(hii, hi_size),
                ++i){
                out.set(i, between(x.get(xi), unwrap(lo.get(loi)), unwrap(hi.get(hii))));
            }
            return out;
        }
    } else {
        r_vec<r_lgl> out(x_size);
        OMP_SIMD
        for (r_size_t i = 0; i < x_size; ++i){
            out.set(i, between(x.get(i), unwrap(lo), unwrap(hi)));
        }
        return out;
    }
}

}


#endif
