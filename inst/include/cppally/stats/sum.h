#ifndef CPPALLY_SUM_H
#define CPPALLY_SUM_H

#include <cppally/vector/r_vector.h>
#include <algorithm>

namespace cppally {

namespace internal {

template <RVectorisable T, typename Acc>
void simd_reduce_add(const r_vec<T>& x, std::invocable<T> auto f, Acc& total_init) {

    Acc local_acc = total_init;

    r_size_t n = x.length();
    const unwrap_t<T>* RESTRICT p_x = x.data();
    int n_threads = internal::calc_threads(n);
    if (n_threads > 1){
        OMP_PARALLEL_FOR_SIMD_REDUCTION1(n_threads, +:local_acc)
        for (r_size_t i = 0; i < n; ++i){
            local_acc += f(T(p_x[i]));
        }
    } else {
        OMP_SIMD_REDUCTION1(+:local_acc)
        for (r_size_t i = 0; i < n; ++i){
            local_acc += f(T(p_x[i]));
        }
    }
    total_init = local_acc;
}

template <RVectorisable T, typename Acc>
void simd_reduce_add(const r_vec<T>& x, std::invocable<T> auto f, Acc& total_init, int_fast64_t& na_count_init) {

    Acc local_acc = total_init;
    int_fast64_t local_na_count = na_count_init;

    r_size_t n = x.length();
    const unwrap_t<T>* RESTRICT p_x = x.data();
    int n_threads = internal::calc_threads(n);
    if (n_threads > 1){
        OMP_PARALLEL_FOR_SIMD_REDUCTION2(n_threads, +:local_acc, +:local_na_count)
        for (r_size_t i = 0; i < n; ++i){
            const T v = T(p_x[i]);
            local_acc += f(v);
            local_na_count += v.is_na();
        }
    } else {
        OMP_SIMD_REDUCTION2(+:local_acc, +:local_na_count)
        for (r_size_t i = 0; i < n; ++i){
            const T v = T(p_x[i]);
            local_acc += f(v);
            local_na_count += v.is_na();
        }
    }
    total_init = local_acc;
    na_count_init = local_na_count;
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

        r_size_t k = std::min(r_size_t(20), x.length());

        for (r_size_t i = 0; i < k; ++i){
            if (is_na(x.get(i))){
                return na<r_int64>();
            }
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
                return internal::coerce_number<r_dbl>(x.get(i));
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

    if constexpr (int128_available){
        int128_otherwise_64_t out_ = 0;
        for (r_size_t i = 0; i < n; ++i){
            
            const r_int64 v = x.get(i);

            if (is_na(v)){
                if (na_rm){
                    continue;
                } else {
                    return na<r_int64>();
                }
            }
            out_ += unwrap(v);
        }

        // [INT64_MIN+1, INT64_MAX] because NA is reserved for INT64_MIN
        if (out_ > std::numeric_limits<int64_t>::max() || out_ <= std::numeric_limits<int64_t>::min()){
            return na<r_int64>();
        }

        return r_int64(static_cast<int64_t>(out_));

    } else {
        r_int64 out(0);
        for (r_size_t i = 0; i < n; ++i){
            const r_int64 v = x.get(i);
            out = out + (na_rm && is_na(v) ? r_int64(0) : v);
        }
        return out;
    }
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

}

#endif
