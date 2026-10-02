#ifndef CPPALLY_QUANTILE_H
#define CPPALLY_QUANTILE_H

#include <cppally/vector/r_vector.h>
#include <cppally/sort/is_sorted.h>
#include <cppally/sort/sort.h>
#include <algorithm>

namespace cppally {

namespace internal {

inline double quantile_impl(double* x_data, r_size_t n, double p, bool sorted){

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
    if (!sorted){
        std::nth_element(x_data, x_data + (j - 1), x_data + n);
    }
    const double x_j = x_data[j - 1]; // The value of quantile index if data were sorted (j-th order statistic)

    // If gamma is 0 then the j-th order statistic is exactly the quantile
    if (gamma <= 0.0 || j == n){
        return x_j;
    }

    // Since nth_element() guarantees elements to the right of x[j] are >= x[j]
    // x[j+1] is the min of that set such that x[j+1] >= x[j]
    const double x_j1 = sorted ? x_data[j] : *std::min_element(x_data + j, x_data + n);
    // Qi(p) = (1 − γ)x[j] + γx[j+1]
    return (1.0 - gamma) * x_j + gamma * x_j1;
}

inline r_size_t sorted_na_count(const r_vec<r_dbl>& x){
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

}

inline r_dbl quantile(const r_vec<r_dbl>& x, r_dbl p, bool na_rm = false){

    if (is_na(p)){
        return na<r_dbl>();
    }
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

    r_vec<r_dbl> v;

    if (n_na > 0){
        // New vector without NAs
        v = r_vec<r_dbl>(n - n_na);
        r_size_t k = 0;
        for (r_size_t i = 0; i < n; ++i){
            const r_dbl elem = x.get(i);
            if (!is_na(elem)){
                v.set(k++, elem);
            }
        }
    } else {
        v = x.copy();
    }

    return r_dbl(internal::quantile_impl(v.data(), n - n_na, unwrap(p), false));
}

inline r_vec<r_dbl> quantile(const r_vec<r_dbl>& x, const r_vec<r_dbl>& probs, bool na_rm = false){

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

    if ((n_na > 0 && !na_rm) || n_na == n){
        return out;
    }

    // Sort the data when either n probs > 20 or there are NAs (so that they can be moved to the end)

    if (n_probs <= 20 && n_na == 0){
        r_vec<r_dbl> x_copy = x.copy();
        double* x_data = x_copy.data();
    
        for (r_size_t i = 0; i < n_probs; ++i){
            const r_dbl p = probs.get(i);
            if (!is_na(p)){
                out.set(i, r_dbl(internal::quantile_impl(x_data, n, unwrap(p), false)));
            }
        }
    } else {
        r_vec<r_dbl> sorted_x = sort(x);
        double* x_data = sorted_x.data();
        
        // NAs are at the tail end of the vector if they exist
        const r_size_t n_ok = n - n_na;
    
        for (r_size_t i = 0; i < n_probs; ++i){
            const r_dbl p = probs.get(i);
            if (!is_na(p)){
                // out.set(i, r_dbl(internal::quantile_impl(x_data, n_ok, unwrap(p), true)));
                out.set(i, r_dbl(internal::quantile_impl(x_data, n_ok, unwrap(p), true)));
            }
        }
    }
    return out;
}

}

#endif
