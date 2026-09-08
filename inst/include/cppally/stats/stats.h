#ifndef CPPALLY_R_STATS_H
#define CPPALLY_R_STATS_H

#include <cppally/vector/r_vector.h>
#include <cppally/math/math.h>
#include <cppally/coerce.h>

namespace cppally {

namespace internal {

// A heuristic to scan the first n elements to check for NAs early in the vector.
// If a vector is saturated with NAs, this will usually find it quickly. 
// The rationale for this is that many SIMD vectorised functions in this file do NOT return early when NA is present.
template <RVector T>
bool any_na_early_on(const T& x, r_size_t k = 20){

    k = std::min(k, x.length());

    for (r_size_t i = 0; i < k; ++i){
        if (is_na(x.get(i))){
            return true;
        }
    }

    return false;
}

template <RVectorisable T, typename Acc>
void simd_reduce_add(const r_vec<T>& x, std::invocable<T> auto f, Acc& total_init) {
    r_size_t n = x.length();
    const unwrap_t<T>* RESTRICT p_x = x.data();
    int n_threads = internal::calc_threads(n);
    if (n_threads > 1){
        OMP_PARALLEL_FOR_SIMD_REDUCTION1(n_threads, +:total_init)
        for (r_size_t i = 0; i < n; ++i){
            total_init += f(T(p_x[i]));
        }
    } else {
        OMP_SIMD_REDUCTION1(+:total_init)
        for (r_size_t i = 0; i < n; ++i){
            total_init += f(T(p_x[i]));
        }
    }
}

template <RVectorisable T, typename Acc>
void simd_reduce_add(const r_vec<T>& x, std::invocable<T> auto f, Acc& total_init, int_fast64_t& na_count_init) {
    r_size_t n = x.length();
    const unwrap_t<T>* RESTRICT p_x = x.data();
    int n_threads = internal::calc_threads(n);
    if (n_threads > 1){
        OMP_PARALLEL_FOR_SIMD_REDUCTION2(n_threads, +:total_init, +:na_count_init)
        for (r_size_t i = 0; i < n; ++i){
            const T v = T(p_x[i]);
            total_init += f(v);
            na_count_init += v.is_na();
        }
    } else {
        OMP_SIMD_REDUCTION2(+:total_init, +:na_count_init)
        for (r_size_t i = 0; i < n; ++i){
            const T v = T(p_x[i]);
            total_init += f(v);
            na_count_init += v.is_na();
        }
    }
}

template <RVectorisable T, typename Acc>
void simd_reduce_minmax(const r_vec<T>& x, std::invocable<T> auto f_min, std::invocable<T> auto f_max, Acc& min_init, Acc& max_init) {
    r_size_t n = x.length();
    const unwrap_t<T>* RESTRICT p_x = x.data();
    int n_threads = internal::calc_threads(n);
    if (n_threads > 1){
        OMP_PARALLEL_FOR_SIMD_REDUCTION2(n_threads, min:min_init, max:max_init)
        for (r_size_t i = 0; i < n; ++i){
            const T v = T(p_x[i]);
            min_init = std::min(min_init, f_min(v));
            max_init = std::max(max_init, f_max(v));
        }
    } else {
        OMP_SIMD_REDUCTION2(min:min_init, max:max_init)
        for (r_size_t i = 0; i < n; ++i){
            const T v = T(p_x[i]);
            min_init = std::min(min_init, f_min(v));
            max_init = std::max(max_init, f_max(v));
        }
    }
}

template <RVectorisable T, typename Acc>
void simd_reduce_minmax(const r_vec<T>& x, std::invocable<T> auto f_min, std::invocable<T> auto f_max, Acc& min_init, Acc& max_init, int_fast64_t& na_count) {
    r_size_t n = x.length();
    const unwrap_t<T>* RESTRICT p_x = x.data();
    int n_threads = internal::calc_threads(n);
    if (n_threads > 1){
        OMP_PARALLEL_FOR_SIMD_REDUCTION3(n_threads, min:min_init, max:max_init, +:na_count)
        for (r_size_t i = 0; i < n; ++i){
            const T v = T(p_x[i]);
            min_init = std::min(min_init, f_min(v));
            max_init = std::max(max_init, f_max(v));
            na_count += v.is_na();
        }
    } else {
        OMP_SIMD_REDUCTION3(min:min_init, max:max_init, +:na_count)
        for (r_size_t i = 0; i < n; ++i){
            const T v = T(p_x[i]);
            min_init = std::min(min_init, f_min(v));
            max_init = std::max(max_init, f_max(v));
            na_count += v.is_na();
        }
    }
}

}
    
// Very fast integer sum
template <RIntegerType T> 
r_int64 sum(const r_vec<T>& x, bool na_rm = false){

    // Use int64_t since (2^31-1)^2 < INT64_MAX
    int_fast64_t res = 0;

    if (na_rm){
        internal::simd_reduce_add(x, [](auto v) noexcept { return is_na(v) ? 0 : static_cast<int_fast64_t>(unwrap(v)); }, res);
    } else {

        if (internal::any_na_early_on(x)){
            return na<r_int64>();
        }
        
        int_fast64_t na_count = 0;
        internal::simd_reduce_add(x, [](auto v) noexcept { return is_na(v) ? 0 : static_cast<int_fast64_t>(unwrap(v)); }, res, na_count);
        if (na_count > 0){
            return na<r_int64>();
        }
    }

    return r_int64(static_cast<int64_t>(res));
}

template <RMathType T> 
r_dbl sum(const r_vec<T>& x, bool na_rm = false){

    double out_ = 0;

    if (na_rm){
        internal::simd_reduce_add(x, [](auto v) noexcept { return is_na(v) ? 0 : unwrap(v); }, out_);
    } else {

        // Find NA OR NaN early on and return early if there is
        r_size_t n = x.length();
        r_size_t n_to_scan = std::min(n, r_size_t(20));
        for (r_size_t i = 0; i < n_to_scan; ++i){
            if (is_na(x.get(i))){
                return as<r_dbl>(x.get(i));
            }
        }

        if constexpr (RFloatType<T>){
            // Let IEEE 754 rules propagate NA/NaN
            internal::simd_reduce_add(x, [](auto v) noexcept { return unwrap(v); }, out_);
        } else {
            int_fast64_t na_count = 0;
            internal::simd_reduce_add(x, [](auto v) noexcept { return is_na(v) ? 0 : unwrap(v); }, out_, na_count);

            if (na_count > 0){
                return na<r_dbl>();
            }
        }

    }
    
    return r_dbl(out_);
}

template <> 
inline r_int64 sum(const r_vec<r_int64>& x, bool na_rm){
    r_size_t n = x.length();

    int128_otherwise_64_t out_ = 0;
    for (r_size_t i = 0; i < n; ++i){
        if (is_na(x.get(i))){
            if (na_rm){
                continue;
            } else {
                return na<r_int64>();
            }
        }
        if constexpr (int128_available){
            out_ += unwrap(x.get(i));
        } else {
            r_int64 temp = r_int64(static_cast<int64_t>(out_)) + x.get(i);
            out_ = unwrap(temp);
        }
    }
    // [INT64_MIN+1, INT64_MAX] because NA is reserved for INT64_MIN
    if (out_ > std::numeric_limits<int64_t>::max() || out_ <= std::numeric_limits<int64_t>::min()){
        return na<r_int64>();
    }
    return r_int64(static_cast<int64_t>(out_));
}

template <RNumericType T>
r_vec<T> range(const r_vec<T>& x, bool na_rm = false){
    
    r_size_t n = x.length();

    if (n == 0){
        return r_vec<T>( { na<T>(), na<T>() } );
    }

    T lo = r_limits<T>::max();
    T hi = r_limits<T>::min();

    unwrap_t<T> lo_ = unwrap(lo);
    unwrap_t<T> hi_ = unwrap(hi);

    int_fast64_t na_count = 0;

    internal::simd_reduce_minmax(
        x,
        [lo](const T& v) noexcept { return is_na(v) ? unwrap(lo) : unwrap(v); },
        [hi](const T& v) noexcept { return is_na(v) ? unwrap(hi) : unwrap(v); },
        lo_, hi_, na_count
    );

    if (na_rm){
        if (na_count == n){
            lo_ = unwrap(na<T>());
            hi_ = lo_;
        }
    } else {
        if (na_count > 0){
            lo_ = unwrap(na<T>());
            hi_ = lo_;
        }
    }
    
    return r_vec<T>( { T(lo_), T(hi_) } );
}

template <RStringType T>
r_vec<T> range(const r_vec<T>& x, bool na_rm = false){
    r_size_t n = x.length();

    r_str_view lo = na<r_str_view>();
    r_str_view hi = na<r_str_view>();
    bool any_na = false;

    for (r_size_t i = 0; i < n; ++i){
        const r_str_view v = x.view(i);
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

    return r_vec<T>( {T(lo, internal::view_tag{}), T(hi, internal::view_tag{})} );
}

// SIMD optimisation for integer types
template <RIntegerType T>
r_vec<T> range(const r_vec<T>& x, bool na_rm = false){

    T lo = r_limits<T>::max();
    T hi = r_limits<T>::min();

    unwrap_t<T> lo_ = unwrap(lo);
    unwrap_t<T> hi_ = unwrap(hi);

    if (na_rm){

        internal::simd_reduce_minmax(
            x,
            [lo](auto v) noexcept { return is_na(v) ? unwrap(lo) : unwrap(v); },
            [](auto v) noexcept { return unwrap(v); },
            lo_, hi_
        );
    } else {

        if (internal::any_na_early_on(x)){
            return r_vec<T>( {na<T>(), na<T>()} );
        }

        internal::simd_reduce_minmax(
            x,
            [](auto v) noexcept { return unwrap(v); },
            [](auto v) noexcept { return unwrap(v); },
            lo_, hi_
        );

        // We use the fact that if there were NAs then min(x) would be NA
        // Only works for R's integer types
        if (lo_ == unwrap(na<T>())){
            hi_ = unwrap(na<T>());
        }
    }

    // If lo/hi are still the values they were initialised to, this either means the vector was full of NAs, or the range really is max/min int
    // Either way, we check in this rare case
    if (lo_ == unwrap(lo) && hi_ == unwrap(hi) && (x.na_count() == x.length())){
        lo_ = unwrap(na<T>());
        hi_ = unwrap(na<T>());
    }

    return r_vec<T>( {T(lo_), T(hi_)} );
}

template <RMathType T>
r_dbl mean(const r_vec<T>& x, bool na_rm = false){
    auto total = sum(x, na_rm);
    if (na_rm){
        return total / (x.length() - x.na_count());
    } else {
        return total / x.length();
    }
}


template <RMathType T>
r_dbl var(const r_vec<T>& x, bool na_rm = false){

    r_size_t n = x.length();
    r_size_t N;

    if (na_rm){
        N = n - x.na_count();
    } else {
        N = n;
    }

    if (N < 2 || n < 2){
        return na<r_dbl>();
    }

    r_dbl mu = sum(x, na_rm) / N;

    if (is_na(mu)){
        return mu;
    }

    // Sum of squared differences
    double sum_sq_diff = 0;
    internal::simd_reduce_add(x, [mu, na_rm](auto v) noexcept {

        double diff = v - mu;
        return na_rm && v.is_na() ? 0 : diff * diff;
        
    }, sum_sq_diff);

     return r_dbl(sum_sq_diff) / (N - 1);
}

} 

#endif
