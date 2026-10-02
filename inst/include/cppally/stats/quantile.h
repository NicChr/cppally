#ifndef CPPALLY_QUANTILE_H
#define CPPALLY_QUANTILE_H

#include <cppally/vector/r_vector.h>
#include <cppally/sort/is_sorted.h>
#include <cppally/sort/sort.h>
#include <algorithm>

namespace cppally {

namespace internal {

// Assumes data has no NAs in first n elements.
// Assumes n > 0
// Assumes p is strictly in [0, 1]
// If o is not nullptr, it overrides sorted. 
//
// ----- O(1) quantiles if data is already sorted or we know order permutation ---- 
// Supply sorted = true if you know the data is already sorted.
// Supply `o`, a permutation ordering that sorts the data.
template <CppNumber T>
inline double quantile_impl(T* x_data, r_size_t n, double p, bool sorted, const int* o = nullptr){

    // m = 1 - p
    // np_m = np + m
    // j = ⌊np_m⌋
    // gamma = np_m - j

    const double m = 1.0 - p;
    const double np_m = n * p + m; // fractional position of quantile (of sorted data)
    const r_size_t j = std::clamp(static_cast<r_size_t>(np_m), r_size_t(1), n); // integer position (index)
    const double gamma = np_m - j; // fractional [0, 1) part of the quantile position - i.e. how far along the gap between the j-th order statistic and the j+1-th order statistic the quantile is

    // nth_element() partitions the data so that x[j] is the j-th order statistic.
    // elements to the left are <= x[j] (but likely unsorted) and elements to the right are >= x[j] (also likely unsorted)

    if (!sorted && !o){
        std::nth_element(x_data, x_data + (j - 1), x_data + n);
    }
    const double x_j = x_data[o ? o[j - 1] : j - 1]; // The value of quantile index if data were sorted (j-th order statistic)

    // If gamma is 0 then the j-th order statistic is exactly the quantile
    if (gamma <= 0.0 || j == n){
        return x_j;
    }

    // Since nth_element() guarantees elements to the right of x[j] are >= x[j]
    // x[j+1] is the min of that set such that x[j+1] >= x[j]
    const double x_j1 = o ? x_data[o[j]] : (sorted ? x_data[j] : *std::min_element(x_data + j, x_data + n));
    // Qi(p) = (1 − γ)x[j] + γx[j+1]
    return (1.0 - gamma) * x_j + gamma * x_j1;
}

template <RNumber T>
inline r_size_t sorted_na_count(const r_vec<T>& x){
    const r_size_t n = x.length();
    r_size_t i = n;
    while (i > 0 && is_na(x.get(i - 1))){
        --i;
    }
    return n - i;
}

inline bool is_valid_prob(double p) noexcept {
    return p >= 0.0 && p <= 1.0;
}

template <CppNumber T>
inline void move_nas_to_end(T* RESTRICT data, r_size_t n) {
    std::partition(data, data + n, [](T v){ return !is_na(v); });
}

}

template <RNumber T>
inline r_dbl quantile(const r_vec<T>& x, r_dbl p, bool na_rm = false){

    if (!internal::is_valid_prob(p)) [[unlikely]] {
        abort("probability must be in [0, 1]");
    }

    const r_size_t n = x.length();

    if (is_sorted(x)){
        // NAs are at the tail end of the vector if they exist
        const r_size_t n_na = internal::sorted_na_count(x);
        if ((n_na > 0 && !na_rm) || n_na == n){
            return na<r_dbl>();
        }
        return r_dbl(internal::quantile_impl(x.data(), n - n_na, unwrap(p), true));
    }

    const r_size_t n_na = x.na_count();

    if (n_na > 0 && !na_rm){
        return na<r_dbl>();
    }

    const auto* x_data = x.data();
    std::vector<unwrap_t<T>> v(x_data, x_data + n);

    if (n_na > 0){
        internal::move_nas_to_end(v.data(), n);
    }

    return r_dbl(internal::quantile_impl(v.data(), n - n_na, unwrap(p), false));
}

template <RNumber T>
inline r_vec<r_dbl> quantile(const r_vec<T>& x, const r_vec<r_dbl>& probs, bool na_rm = false){

    const r_size_t n_probs = probs.length();

    for (r_size_t i = 0; i < n_probs; ++i){
        const r_dbl p = probs.get(i);
        if (!internal::is_valid_prob(p)) [[unlikely]] {
            abort("probability must be in [0, 1]");
        }
    }

    if (n_probs == 0){
        return r_vec<r_dbl>();
    }
    if (n_probs == 1){
        return r_vec<r_dbl>(1, quantile(x, probs.get(0), na_rm));
    }

    r_vec<r_dbl> out(n_probs, na<r_dbl>());

    const r_size_t n = x.length();
    const r_size_t n_na = x.na_count();
    const r_size_t n_ok = n - n_na;

    if ((n_na > 0 && !na_rm) || n_na == n){
        return out;
    }

    bool sorted = is_sorted(x);

    if (sorted){
        auto* x_data = x.data();
        for (r_size_t i = 0; i < n_probs; ++i){
            const r_dbl p = probs.get(i);
            if (!is_na(p)){
                out.set(i, r_dbl(internal::quantile_impl(x_data, n_ok, unwrap(p), true)));
            }
        }
    } else if (n_probs < 25){
        
        // If number of probs is small, it's faster to compute them (via nth_element())

        std::vector<unwrap_t<T>> v(x.data(), x.data() + n);

        // Move NAs to the end of the vector
        if (n_na > 0){
            internal::move_nas_to_end(v.data(), n);
        }

        for (r_size_t i = 0; i < n_probs; ++i){
            const r_dbl p = probs.get(i);
            if (!is_na(p)){
                out.set(i, r_dbl(internal::quantile_impl(v.data(), n_ok, unwrap(p), false)));
            }
        }
    } else {

        // Use order permutation to directly calculate quantiles
    
        r_vec<r_int> o = order(x);

        auto* x_data = x.data();
        const int* o_data = o.data();

        for (r_size_t i = 0; i < n_probs; ++i){
            const r_dbl p = probs.get(i);
            if (!is_na(p)){
                out.set(i, r_dbl(internal::quantile_impl(x_data, n_ok, unwrap(p), false, o_data)));
            }
        }
    }

    return out;
}

}

#endif
