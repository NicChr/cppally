#include <cpp20.hpp>
using namespace cpp20;
#include <chrono>

[[cpp20::register]]
r_dbl bench_protect_insert_release(r_int n_iters) {
    int n = unwrap(n_iters);
    SEXP dummy = Rf_ScalarInteger(42);
    R_PreserveObject(dummy);
    
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < n; ++i) {
        r_sexp x(dummy);  // insert into pool
    }                      // destructor → release from pool
    auto end = std::chrono::high_resolution_clock::now();
    
    R_ReleaseObject(dummy);
    double ns = std::chrono::duration<double, std::nano>(end - start).count();
    return r_dbl(ns / n);  // nanoseconds per insert+release cycle
}


[[cpp20::register]]
r_dbl bench_protect_copy(r_int n_iters) {
    int n = unwrap(n_iters);
    SEXP dummy = Rf_ScalarInteger(42);
    r_sexp dummy2 = r_sexp(dummy);
    
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < n; ++i) {
        r_sexp x(dummy2); // Copy
    }
    auto end = std::chrono::high_resolution_clock::now();
    
    double ns = std::chrono::duration<double, std::nano>(end - start).count();
    return r_dbl(ns / n);  // nanoseconds per copy
}



