#include <cpp20_light.hpp>
using namespace cpp20;


[[cpp20::register]]
r_vec<r_sexp> test_attrs(SEXP x){
  return attr::get_attrs(x);
}

[[cpp20::register]]
r_vec<r_sexp> test_df(r_vec<r_sexp> cols){
  return r_vec<r_sexp>(r_df(cols));
}

