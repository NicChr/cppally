#ifndef CPPALLY_VAR_H
#define CPPALLY_VAR_H

#include <cppally/stats/sum.h>

namespace cppally {

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
