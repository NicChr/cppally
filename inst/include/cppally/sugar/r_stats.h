#ifndef CPPALLY_R_STATS_H
#define CPPALLY_R_STATS_H

#include <cppally/r_vec.h>

namespace cppally {
    
template <RMathType T> 
r_dbl sum(const r_vec<T> &x, bool na_rm = false){
    r_size_t n = x.length();
    double out_ = 0;
    const auto* RESTRICT p_x = x.data();

    if (na_rm){
        OMP_SIMD_REDUCTION1(+:out_)
        for (r_size_t i = 0; i < n; ++i){
            out_ += (is_na(T(p_x[i]))) ? 0 : p_x[i];
        }
    } else {
        for (r_size_t i = 0; i < n; ++i){
            if (is_na(T(p_x[i]))){
                return na<r_dbl>();
            }
            out_ += p_x[i];
        }
    }
    return r_dbl(out_);
}

// Optimisation for r_dbl
template <>
inline r_dbl sum(const r_vec<r_dbl> &x, bool na_rm){
    r_size_t n = x.length();
    double out_ = 0;
    const auto* RESTRICT p_x = x.data();

    if (na_rm){
        OMP_SIMD_REDUCTION1(+:out_)
        for (r_size_t i = 0; i < n; ++i){
            out_ += is_na(r_dbl(p_x[i])) ? 0 : p_x[i];
        }
    } else {
        OMP_SIMD_REDUCTION1(+:out_)
        for (r_size_t i = 0; i < n; ++i){
            out_ += p_x[i];
        }
        
    }
    return r_dbl(out_);
}

// Integer specific sum (user must accept there may be overflow)
template <RIntegerType T>
r_int64 sum_int(const r_vec<T> &x, bool na_rm = false){
    r_size_t n = x.length();
    int64_t out_ = 0;
    const auto* RESTRICT p_x = x.data();

    if (na_rm){
        OMP_SIMD_REDUCTION1(+:out_)
        for (r_size_t i = 0; i < n; ++i){
            out_ += (is_na(as_r_val(p_x[i]))) ? int64_t(0) : p_x[i];
        }
    } else {
        for (r_size_t i = 0; i < n; ++i){
            if (is_na(as_r_val(p_x[i]))){
                return na<r_int64>();
            }
            out_ += p_x[i];
        }
    }
    return r_int64(out_);
}

template <RSortableType T>
r_vec<T> range(const r_vec<T> &x, bool na_rm = false){
    
    r_size_t n = x.length();

    T lo = r_limits<T>::max();
    T hi = r_limits<T>::min();

    // Can't use SIMD, `cppally::min/max` checks for NAs automatically
    if (na_rm){
        for (r_size_t i = 0; i < n; ++i){
            const auto v = x.get(i);
            if (is_na(v)){
                continue;
            } else {
                lo = min(lo, v);
                hi = max(hi, v);
            }
        }
    } else {
        for (r_size_t i = 0; i < n; ++i){
            const auto v = x.get(i);
            lo = min(lo, v); 
            hi = max(hi, v);
        }
    }
    r_vec<T> out(2);
    out.set(0, lo);
    out.set(1, hi);
    return out;
}

template <RStringType T>
r_vec<T> range(const r_vec<T> &x, bool na_rm = false){
    r_size_t n = x.length();

    r_str_view lo = na<r_str_view>();
    r_str_view hi = na<r_str_view>();
    bool any_na = false;

    for (r_size_t i = 0; i < n; ++i){
        const auto v = x.view(i);
        if (is_na(v)) {
            any_na = true;
            continue;
        }
        lo = is_na(lo) ? v : min(lo, v);
        hi = is_na(hi) ? v : max(hi, v);
    }

    if (!na_rm && any_na) {
        lo = hi = na<r_str_view>();
    }

    r_vec<T> out(2);
    out.set(0, T(lo));
    out.set(1, T(hi));
    return out;
}

// SIMD optimisation for integer types
template <RIntegerType T>
r_vec<T> range(const r_vec<T> &x, bool na_rm = false){
    
    r_size_t n = x.length();

    T max_val = r_limits<T>::max();
    T min_val = r_limits<T>::min();

    T lo = max_val;
    T hi = min_val;

    auto lo_ = unwrap(lo);
    auto hi_ = unwrap(hi);

    const auto* RESTRICT p_x = x.data();

    if (na_rm){ 
        OMP_SIMD_REDUCTION2(min:lo_, max:hi_)
        for (r_size_t i = 0; i < n; ++i){
            // Ignore NA for min()
            lo_ = is_na(T(p_x[i])) ? lo_ : std::min(lo_, p_x[i]);
            // No need to ignore NA for max() because NA is defined as lowest representable value
            hi_ = std::max(hi_, p_x[i]);
        }
        lo = T(lo_);
        hi = T(hi_);

        // If lo/hi are still the same values as when initialised, this either means the vector was full of NAs, or the range really is max/min int
        // Either way, we check in this rare case
        if (lo == max_val && hi == min_val && x.all_na()){
            lo = na<T>();
            hi = na<T>();
        }

    } else {
        OMP_SIMD_REDUCTION2(min:lo_, max:hi_)
        for (r_size_t i = 0; i < n; ++i){
            lo_ = std::min(lo_, p_x[i]); 
            hi_ = std::max(hi_, p_x[i]);
        }
        lo = T(lo_);
        hi = T(hi_);

        // We use the fact that if there were NAs then min(x) would be NA
        // Only works for R's integer types
        bool has_nas = is_na(lo);

        if (has_nas){
            lo = na<T>();
            hi = na<T>();
        }
    }
    r_vec<T> out(2);
    out.set(0, lo);
    out.set(1, hi);
    return out;
}

template <RSortableType T>
T min(const r_vec<T> &x, bool na_rm = false){
    return range(x, na_rm).get(0);
}
template <RSortableType T>
T max(const r_vec<T> &x, bool na_rm = false){
    return range(x, na_rm).get(1);
}

template <RMathType T>
r_vec<T> abs(const r_vec<T> &x){
    r_size_t n = x.length();
    r_vec<T> out(n);
    int n_threads = internal::calc_threads(n);
    if (n_threads > 1) {
        OMP_PARALLEL_FOR_SIMD(n_threads)
        for (r_size_t i = 0; i < n; ++i){
            out.set(i, abs(x.get(i)));
        }
    } else {
        OMP_SIMD
        for (r_size_t i = 0; i < n; ++i){
            out.set(i, abs(x.get(i)));
        }
    }
    return out;
}


template <RMathType T>
r_dbl mean(const r_vec<T> &x, bool na_rm = false){
    r_dbl total = sum(x, na_rm);
    if (na_rm){
        return total / (x.length() - x.na_count());
    } else {
        return total / x.length();
    }
}


template <RMathType T>
r_dbl var(const r_vec<T> &x, bool na_rm = false){

    r_size_t N;

    if (na_rm){
        N = x.length() - x.na_count();
    } else {
        N = x.length();
    }

    if (N < 2){
        return na<r_dbl>();
    }

    --N;

    r_dbl mu = mean(x, na_rm);

    if (is_na(mu)){
        return mu;
    }
    // Sum of squared differences

    r_size_t n = x.length();

    double sum_sq_diff(0);
    
    for (r_size_t i = 0; i < n; ++i){
        if (is_na(x.get(i))) continue;
        double diff = unwrap(x.get(i)) - unwrap(mu);
        sum_sq_diff += diff * diff;
      }
      return r_dbl(sum_sq_diff) / N;
}

template <RMathType T>
r_dbl sd(const r_vec<T> &x, bool na_rm = false){
    return sqrt(var(x, na_rm));
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
      abort("tol must be >= 0 and < 1");
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
    } else if (identical(out, pos_inf)){
        break;
    }
    out = lcm(out, x.get(i), na_rm, tol);
    }
    return out;
  }


// r_lgl not bool because bool can't be NA
// template <RVal T>
// inline r_lgl all_whole_numbers(r_vec<T> x, bool na_rm = false, r_dbl tol = r_limits<r_dbl>::tolerance()){

//     if constexpr (RIntegerType<T>){
//         return r_true;
//     } else if constexpr (RFloatType<T>){

//         r_size_t n = x.length();

//         r_lgl out = r_true;
//         r_size_t na_count = 0;

//         for (r_size_t i = 0; i < n; ++i) {
//             out = is_whole_number(x.get(i), tol);
//             na_count += is_na(out);
//             if (out == r_false){
//                 break;
//             }
//         }
//         if (out == r_true && !na_rm && na_count > 0){
//             out = r_na;
//         } else if (na_rm && na_count == n){
//             out = r_true;
//         }
//         return out;
//     } else {
//         return r_false;
//     }
// }

// template<RVector T, typename U>
// inline r_vec<r_lgl> between(const T& x, const U& lo, const U& hi) {

//     r_size_t x_size = x.length();

//     if constexpr (RVector<U>){
//         r_size_t lo_size = lo.length();
//         r_size_t hi_size = hi.length();
        
//         if (lo_size == 1 && hi_size == 1){
//             return between(x, unwrap(lo.get(0)), unwrap(hi.get(0)));
//         } else if (x_size == lo_size && lo_size == hi_size){
//             r_vec<r_lgl> out(x_size);
//             OMP_SIMD
//             for (r_size_t i = 0; i < x_size; ++i){
//                 out.set(i, between(x.get(i), unwrap(lo.get(i)), unwrap(hi.get(i))));
//             }
//             return out;
//         } else {
//             // Slower recycling approach
//             r_size_t n = std::max(std::max(x_size, lo_size), hi_size);
//             if (x_size == 0 || lo_size == 0 || hi_size == 0){
//                 n = 0;
//             }
//             r_vec<r_lgl> out(n);
//             for (r_size_t i = 0, xi = 0, loi = 0, hii = 0; i < n;
//                 recycle_index(xi, x_size),
//                 recycle_index(loi, lo_size),
//                 recycle_index(hii, hi_size),
//                 ++i){
//                 out.set(i, between(x.get(xi), unwrap(lo.get(loi)), unwrap(hi.get(hii))));
//             }
//             return out;
//         }
//     } else {
//         r_vec<r_lgl> out(x_size);
//         OMP_SIMD
//         for (r_size_t i = 0; i < x_size; ++i){
//             out.set(i, between(x.get(i), unwrap(lo), unwrap(hi)));
//         }
//         return out;
//     }
// }

} 

#endif
