#include <cppally.hpp>
using namespace cppally;

// Simple example showing how to register a C++ function to R
// You can compile and register this via `cppally::load_all()`

// For more info on cppally see https://nicchr.github.io/cppally/index.html
    

[[cppally::register]]
SEXP foo(const r_vec<r_sexp>& x){
  return x.lengths();
}

