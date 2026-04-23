#include <cppally_light.hpp>
using namespace cppally;


[[cpp::register]]
r_vec<r_sexp> test_attrs(SEXP x){
  return attr::get_attrs(x);
}

[[cpp::register]]
r_vec<r_sexp> test_df(r_vec<r_sexp> cols){
  return r_vec<r_sexp>(r_df(cols));
}
