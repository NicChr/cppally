#include <cpp20/r_attrs.h>
using namespace cpp20;


[[cpp20::register]]
r_vec<r_sexp> test_attrs(SEXP x){
  return attr::get_attrs(x);
}
