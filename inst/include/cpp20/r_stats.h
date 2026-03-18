#ifndef CPP20_R_STATS_H
#define CPP20_R_STATS_H

#include <cpp20/r_vec.h>

namespace cpp20 {
    
template <RMathType T> 
r_dbl sum(const r_vec<T> &x, bool na_rm = false){
    r_size_t n = x.length();
    double out_ = 0;
    const auto* RESTRICT p_x = x.data();

    if (na_rm){
        #pragma omp simd reduction(+:out_)
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
        #pragma omp simd reduction(+:out_)
        for (r_size_t i = 0; i < n; ++i){
            out_ += is_na(r_dbl(p_x[i])) ? 0 : p_x[i];
        }
    } else {
        #pragma omp simd reduction(+:out_)
        for (r_size_t i = 0; i < n; ++i){
            out_ += p_x[i];
        }
        
    }
    return r_dbl(out_);
}

// Integer specific sum (user must accept there may be overflow)
template <RIntegerType T>
auto sum_int(const r_vec<T> &x, bool na_rm = false){
    r_size_t n = x.length();
    int64_t out_ = 0;
    const auto* RESTRICT p_x = x.data();

    if (na_rm){
        #pragma omp simd reduction(+:out_)
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

    // Can't use SIMD, `cpp20::min/max` checks for NAs automatically
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
    out.set(0, lo);
    out.set(1, hi);
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
        #pragma omp simd reduction(std::min:lo_) reduction(std::max:hi_)
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
        #pragma omp simd reduction(std::min:lo_) reduction(std::max:hi_)
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

template <RNumericType T>
T min(const r_vec<T> &x, bool na_rm = false){
    return range(x, na_rm).get(0);
}
template <RNumericType T>
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

} 

#endif
