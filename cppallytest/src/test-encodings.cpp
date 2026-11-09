#include <cppally.hpp>
using namespace cppally;

[[cppally::register]]
r_vec<r_lgl> str_eq(r_vec<r_str> x, r_vec<r_str> y){
    return x == y;
}

[[cppally::register]]
r_vec<r_lgl> str_lt(r_vec<r_str> x, r_vec<r_str> y){
    return x < y;
}
