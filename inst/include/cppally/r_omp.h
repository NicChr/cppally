#ifndef CPPALLY_R_OMP_H
#define CPPALLY_R_OMP_H

#include <cppally/r_vec.h>

namespace cppally {

namespace omp {

// OMP helpers

template <RVectorisable T, typename Acc>
void simd_reduce_add(const r_vec<T>& x, Acc& init, std::invocable<T> auto f, bool parallel = true) noexcept {
    r_size_t n = x.length();
    const unwrap_t<T>* RESTRICT p_x = x.data();
    int n_threads = parallel ? internal::calc_threads(n) : 1;
    if (n_threads > 1){
        OMP_PARALLEL_FOR_SIMD_REDUCTION1(n_threads, +:init)
        for (r_size_t i = 0; i < n; ++i) init += f(T(p_x[i]));
    } else {
        OMP_SIMD_REDUCTION1(+:init)
        for (r_size_t i = 0; i < n; ++i) init += f(T(p_x[i]));
    }
}

}

}

#endif
