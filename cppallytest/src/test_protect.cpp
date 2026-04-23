#include <cppally.hpp>
using namespace cppally;
#include <chrono>

[[cpp::register]]
double bench_protect_insert_release_cppally(int n) {
    SEXP dummy = Rf_ScalarInteger(42);
    R_PreserveObject(dummy);
    
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < n; ++i) {
        r_sexp x(dummy);  // insert into pool
    }                      // destructor → release from pool
    auto end = std::chrono::high_resolution_clock::now();
    
    R_ReleaseObject(dummy);
    double ns = std::chrono::duration<double, std::nano>(end - start).count();
    return ns / n;  // nanoseconds per insert+release cycle
}


[[cpp::register]]
double bench_protect_copy_cppally(int n) {
    SEXP dummy = Rf_ScalarInteger(42);
    r_sexp dummy2 = r_sexp(dummy);
    
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < n; ++i) {
        r_sexp x = dummy2; // Copy
    }
    auto end = std::chrono::high_resolution_clock::now();
    
    double ns = std::chrono::duration<double, std::nano>(end - start).count();
    return ns / n;  // nanoseconds per copy
}



