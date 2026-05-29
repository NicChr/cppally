#ifndef CPPALLY_R_STATS_H
#define CPPALLY_R_STATS_H

#include <cppally/r_vec.h>

namespace cppally {
    
// Very fast integer sum
template <RMathType T> 
requires (is<unwrap_t<T>, int>)
r_dbl sum(const r_vec<T>& x, bool na_rm = false){
    r_size_t n = x.length();

    // Use int64_t since (2^31-1)^2 < INT64_MAX
    int_fast64_t res = 0;
    const auto* RESTRICT p_x = x.data();

    if (na_rm){
        OMP_SIMD_REDUCTION1(+:res)
        for (r_size_t i = 0; i < n; ++i){
            res += is_na(p_x[i]) ? 0 : static_cast<int_fast64_t>(p_x[i]);
        }
    } else {
        for (r_size_t i = 0; i < n; ++i){
            if (is_na(p_x[i])){
                return na<r_dbl>();
            }
            res += static_cast<int_fast64_t>(p_x[i]);
        }
    }
    return r_dbl(static_cast<double>(res));
}

template <RMathType T> 
r_dbl sum(const r_vec<T>& x, bool na_rm = false){
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
inline r_dbl sum(const r_vec<r_dbl>& x, bool na_rm){
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

template <RSortableType T>
r_vec<T> range(const r_vec<T>& x, bool na_rm = false){
    
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
r_vec<T> range(const r_vec<T>& x, bool na_rm = false){
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
r_vec<T> range(const r_vec<T>& x, bool na_rm = false){
    
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
        if (lo == max_val && hi == min_val && x.all_val(na<T>())){
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

template <RMathType T>
r_dbl mean(const r_vec<T>& x, bool na_rm = false){
    r_dbl total = sum(x, na_rm);
    if (na_rm){
        return total / (x.length() - x.count(na<typename T::data_type>()));
    } else {
        return total / x.length();
    }
}


template <RMathType T>
r_dbl var(const r_vec<T>& x, bool na_rm = false){

    r_size_t N;

    if (na_rm){
        N = x.length() - x.count(na<typename T::data_type>());
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
r_dbl sd(const r_vec<T>& x, bool na_rm = false){
    return sqrt(var(x, na_rm));
}

template<RIntegerType T>
T gcd(const r_vec<T>& x, bool na_rm = false, T tol = r_limits<T>::tolerance()){
  if (tol < 0 || tol >= 1){
    abort("`tol` must be >= 0 and < 1");
  }
  r_size_t n = x.length();

  if (n == 0){
    return na<T>();
  }

  T out = x.get(0);
  for (r_size_t i = 1; i < n; ++i) {
      out = gcd(out, x.get(i), na_rm);
      if (!na_rm && is_na(out)){
          break;
      } else if ( (out == 1).is_true() ){
          break;
      }
  }
  return out;
}

template<RMathType T>
T gcd(const r_vec<T>& x, bool na_rm = false, T tol = r_limits<T>::tolerance()){
  if (tol < 0 || tol >= 1){
    abort("`tol` must be >= 0 and < 1");
  }
  r_size_t n = x.length();

  if (n == 0){
    return na<T>();
  }

  T out = x.get(0);
  for (r_size_t i = 1; i < n; ++i) {
    out = gcd(out, x.get(i), na_rm);
    if (!na_rm && is_na(out)){
        break;
    }
    if ( (out > T(0.0) && out < (tol + tol)).is_true() ){
      out = tol;
      break;
    }
  }
  return out;
}

template<RMathType T>
T lcm(const r_vec<T>& x, bool na_rm = false, T tol = r_limits<T>::tolerance()){
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

} 

#endif
