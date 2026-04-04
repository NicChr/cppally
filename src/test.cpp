#include <cpp20.hpp>
using namespace cpp20;


[[cpp20::register]]
r_str cpp20_typeof(SEXP x){
  return r_str(internal::r_type_to_str(internal::CPP20_TYPEOF(x)));
}



[[cpp20::register]]
r_vec<r_int> test_which(r_vec<r_lgl> const& x){
  return x.find(r_true);
}

[[cpp20::register]]
r_vec<r_int> test_which_inverted(r_vec<r_lgl> const& x){
  return x.find(r_true, true);
}
