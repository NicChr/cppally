#include <cppally.hpp>
using namespace cppally;

[[cpp::register]]
r_vec<r_sexp> test_factor1(r_factors x){
  return make_vec<r_sexp>(x, x.levels(), x.value, r_factors(), r_factors(3), as<r_vec<r_str_view>>(x), as<r_vec<r_str_view>>(x));
}

[[cpp::register]]
r_dbl test_factor3(r_factors x){
  return as<r_dbl>(x);
}
