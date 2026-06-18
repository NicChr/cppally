#ifndef CPPALLY_R_OMP_H
#define CPPALLY_R_OMP_H

#include <cppally/r_vec.h>

namespace cppally {

namespace omp {

// OMP helpers

// Apply a function to *this across range of target
template <RVectorisable T, RVectorisable U>
void simd_apply(r_vec<T>& source, r_vec<U>& target, auto f) {
    r_size_t n = source.length();
    if (n != target.length()) [[unlikely]] {
        abort("`length(source)` must equal `length(target)`");
    }
    const unwrap_t<T>* p_source = source.data();
    unwrap_t<U>* p_target = target.data();
    int n_threads = internal::calc_threads(n);
    if (n_threads > 1){
        OMP_PARALLEL_FOR_SIMD(n_threads)
        for (r_size_t i = 0; i < n; ++i) p_target[i] = unwrap(f(T(p_source[i])));
    } else {
        OMP_SIMD
        for (r_size_t i = 0; i < n; ++i) p_target[i] = unwrap(f(T(p_source[i])));
    }
}

template <RVectorisable T>
void simd_apply(r_vec<T>& x, auto f) {
    simd_apply(x, x, f);
}

template <RVectorisable T, typename Acc>
void simd_reduce_add(const r_vec<T>& x, Acc& init, auto f) noexcept {
    r_size_t n = x.length();
    const unwrap_t<T>* RESTRICT p_x = x.data();
    int n_threads = internal::calc_threads(n);
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
