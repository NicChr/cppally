#ifndef CPPALLY_R_OMP_H
#define CPPALLY_R_OMP_H

#include <cppally/r_vec.h>

namespace cppally {

namespace omp {

// OMP helpers

// Apply unary function to source's elements and set target's elements to output (via OMP SIMD)
template <RVectorisable T, RVectorisable U>
void simd_apply(r_vec<T>& source, r_vec<U>& target, std::invocable<T> auto f, bool parallel = true) {
    
    r_size_t n = source.length();

    if (n == 0){
        return;
    }

    if (n != target.length() && n != 1) [[unlikely]] {
        abort("`length(source)` must equal `length(target)` or 1");
    }
    const unwrap_t<T>* p_source = source.data();
    unwrap_t<U>* p_target = target.data();
    
    int n_threads = parallel ? internal::calc_threads(n) : 1;

    if (n == 1){
        T source_val = T{p_source[0]};
        if (n_threads > 1){
            OMP_PARALLEL_FOR_SIMD(n_threads)
            for (r_size_t i = 0; i < n; ++i) p_target[i] = unwrap(f(source_val));
        } else {
            OMP_SIMD
            for (r_size_t i = 0; i < n; ++i) p_target[i] = unwrap(f(source_val));
        }
    } else {
        if (n_threads > 1){
            OMP_PARALLEL_FOR_SIMD(n_threads)
            for (r_size_t i = 0; i < n; ++i) p_target[i] = unwrap(f(T(p_source[i])));
        } else {
            OMP_SIMD
            for (r_size_t i = 0; i < n; ++i) p_target[i] = unwrap(f(T(p_source[i])));
        }
    }
}

// apply unary function directly to target (via OMP SIMD)
template <RVectorisable T>
void simd_apply(r_vec<T>& target, std::invocable<T> auto f, bool parallel = true) {
    simd_apply(target, target, f, parallel);
}

// Apply binary function (via OMP SIMD)
template <RVectorisable T, RVectorisable U, RVectorisable V>
void simd_apply(r_vec<T>& lhs, r_vec<U>& rhs, r_vec<V>& target, std::invocable<T, U> auto f, bool parallel = true) {
    
    r_size_t n = target.length();

    if (n == 0){
        return;
    }
    
    const unwrap_t<T>* p_lhs = lhs.data();
    const unwrap_t<U>* p_rhs = rhs.data();
    unwrap_t<V>* p_target = target.data();

    int n_threads = parallel ? internal::calc_threads(n) : 1;

    if (lhs.length() == 1 && rhs.length() == 1){
        T lhs_v = T(p_lhs[0]);
        U rhs_v = U(p_rhs[0]);
        if (n_threads > 1){
            OMP_PARALLEL_FOR_SIMD(n_threads)
            for (r_size_t i = 0; i < n; ++i) p_target[i] = unwrap(f(lhs_v, rhs_v));
        } else {
            OMP_SIMD
            for (r_size_t i = 0; i < n; ++i) p_target[i] = unwrap(f(lhs_v, rhs_v));
        }
    } else if (lhs.length() == 1 && rhs.length() == n){
        T lhs_v = T(p_lhs[0]);
        if (n_threads > 1){
            OMP_PARALLEL_FOR_SIMD(n_threads)
            for (r_size_t i = 0; i < n; ++i) p_target[i] = unwrap(f(lhs_v, U(p_rhs[i])));
        } else {
            OMP_SIMD
            for (r_size_t i = 0; i < n; ++i) p_target[i] = unwrap(f(lhs_v, U(p_rhs[i])));
        }
    } else if (rhs.length() == 1 && lhs.length() == n){
        U rhs_v = U(p_rhs[0]);
        if (n_threads > 1){
            OMP_PARALLEL_FOR_SIMD(n_threads)
            for (r_size_t i = 0; i < n; ++i) p_target[i] = unwrap(f(T(p_lhs[i]), rhs_v));
        } else {
            OMP_SIMD
            for (r_size_t i = 0; i < n; ++i) p_target[i] = unwrap(f(T(p_lhs[i]), rhs_v));
        }
    } else if (lhs.length() == n && rhs.length() == n){
        if (n_threads > 1){
            OMP_PARALLEL_FOR_SIMD(n_threads)
            for (r_size_t i = 0; i < n; ++i) p_target[i] = unwrap(f(T(p_lhs[i]), U(p_rhs[i])));
        } else {
            OMP_SIMD
            for (r_size_t i = 0; i < n; ++i) p_target[i] = unwrap(f(T(p_lhs[i]), U(p_rhs[i])));
        }
    } else {
        abort("`length(lhs)` or `length(rhs)` must be 1 or both must equal `length(target)`");
    }
}

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
