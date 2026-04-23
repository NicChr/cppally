#include <cppally.hpp>
using namespace cppally;

void static_tests(){
  static_assert(is<unwrap_t<r_lgl>, int>);
  static_assert(is<unwrap_t<r_dbl>, double>);
  static_assert(is<unwrap_t<r_cplx>, std::complex<double>>);
  static_assert(is<unwrap_t<r_raw>, Rbyte>);
  static_assert(is<unwrap_t<r_sexp>, SEXP>);
  static_assert(is<unwrap_t<r_date>, double>);
  static_assert(is<unwrap_t<r_vec<r_str>>, SEXP>);
  static_assert(is<unwrap_t<r_factors>, SEXP>);

  static_assert(is<decltype(unwrap(r_lgl())), int>);
  static_assert(is<decltype(unwrap(r_dbl())), double>);
  static_assert(is<decltype(unwrap(r_cplx())), std::complex<double>>);
  static_assert(is<decltype(unwrap(r_raw())), Rbyte>);
  static_assert(is<decltype(unwrap(r_sexp())), SEXP>);
  static_assert(is<decltype(unwrap(r_date())), double>);
  static_assert(is<decltype(unwrap(r_vec<r_int>())), SEXP>);
  static_assert(is<decltype(unwrap(r_factors())), SEXP>);
}

// Testing cpp::init
static int foo_int = 1;

[[cpp::init]]
void foo(DllInfo* dll){
  foo_int = 2;
}

[[cpp::init]]
void bar(DllInfo* dll){
  foo_int = 3;
}

[[cpp::register]]
void cpp_set_threads(int n){
  set_threads(n);
}

[[cpp::register]]
r_int cpp_get_threads(){
  return r_int(get_threads());
}

[[cpp::register]]
r_int cpp_get_max_threads(){
  return r_int(OMP_MAX_THREADS);
}

[[cpp::register]]
r_str cpp_typeof(SEXP x){
  return r_str(internal::r_type_to_str(internal::CPPALLY_TYPEOF(x)));
}


// Basic identity fn
[[cpp::register]]
SEXP test_identity2(SEXP x){
  return x;
}

// R strings
[[cpp::register]]
r_vec<r_str> test_str1(r_str x){
  return as<r_vec<r_str>>(x);
}

[[cpp::register]]
r_vec<r_str_view> test_str2(r_str_view x){
  return as<r_vec<r_str_view>>(x);
}

[[cpp::register]]
r_vec<r_date> test_as_date(SEXP x){
  return as<r_vec<r_date>>(x);
} 

[[cpp::register]]
r_vec<r_date> test_construct_date(SEXP x){
  return r_vec<r_date>(x);
} 

[[cpp::register]]
r_vec<r_date> test_as_date2(r_vec<r_date> x){
  return as<r_vec<r_date>>(x);
} 

[[cpp::register]]
r_sexp test_null(){
  return r_null;
}

[[cpp::register]]
r_sym test_sym(r_sym x){
  return x;
}

[[cpp::register]]
r_sexp test_sexp2(r_sexp x){
  return x;
}

[[cpp::register]]
r_vec<r_sexp> test_sexp3(r_vec<r_sexp> x){
  return x;
}

[[cpp::register]]
r_vec<r_int> test_coerce1(const r_vec<r_sexp>& x){
  return as<r_vec<r_int>>(x);
}

[[cpp::register]]
r_vec<r_date> test_dates1(r_vec<r_date> x){
  return x;
}


[[cpp::register]]
r_str test_tz(r_vec<r_psxct> x){
  x.set_tzone("America/New_York");
  return x.tzone();
}


[[cpp::register]]
r_vec<r_int> test_lengths(const r_vec<r_sexp>& x){
  return x.lengths();
}

[[cpp::register]]
r_lgl test_lgl(){
  r_int x(5);
  r_int y(5);
  return (x == y || x != y || r_true != r_false);
}
