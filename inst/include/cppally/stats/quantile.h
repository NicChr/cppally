#ifndef CPPALLY_QUANTILE_H
#define CPPALLY_QUANTILE_H

#include <cppally/vector/r_vector.h>
#include <cppally/sort/is_sorted.h>
#include <algorithm>

namespace cppally {

namespace internal {

// No memory allocated when `sorted == true` unless na_rm = true and NAs are present
inline r_dbl quantile_impl(const r_vec<r_dbl>& x, r_dbl p, bool sorted, bool na_rm = false){

    r_size_t n = x.length();

    if (is_na(p)){
        return na<r_dbl>();
    }
    if ( (p < 0 || p > 1).is_true() ) [[unlikely]] {
        abort("`p` must be in [0, 1]");
    }

    if (n == 0){
        return na<r_dbl>();
    }

    r_size_t n_na = x.na_count();
    
    if (n_na > 0 && !na_rm){
        return r_dbl::na();
    }

    n -= n_na;

    r_vec<r_dbl> v;

    if (sorted){
        v = x;
    } else if (n_na > 0){
        v = r_vec<r_dbl>(n);
        r_size_t k = 0;
        for (r_size_t i = 0; i < n + n_na; ++i){
            const r_dbl elem = x.get(i);
            if (!is_na(elem)){
                v.set(k++, elem);
            }
        }
    } else {
        v = x.copy();
    }

    if (n == 0){
        return na<r_dbl>();
    }

    double* RESTRICT x_data = v.data();

    // m = 1 - p
    // np_m = np + m
    // j = ⌊np_m⌋
    // gamma = np_m - j

    const double m = 1.0 - unwrap(p);
    const double np_m = n * unwrap(p) + m; // fractional position of quantile (of sorted data)
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
        return r_dbl(x_j);
    }

    // Since nth_element() guarantees elements to the right of x[j] are >= x[j]
    // x[j+1] is the min of that set such that x[j+1] >= x[j]
    const double x_j1 = sorted ? x_data[j] : *std::min_element(x_data + j, x_data + n);
    // Qi(p) = (1 − γ)x[j] + γx[j+1]
    return r_dbl((1.0 - gamma) * x_j + gamma * x_j1);
}

}

inline r_dbl quantile(const r_vec<r_dbl>& x, r_dbl prob, bool na_rm = false){
    return internal::quantile_impl(x, prob, is_sorted(x), na_rm);
}

}

#endif
