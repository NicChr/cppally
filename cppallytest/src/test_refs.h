#pragma once

#include <cppally_light.hpp>
using namespace cppally;

[[cppally::register]]
SEXP test_by_value(r_vec<r_dbl> x){
  return x;
}

[[cppally::register]]
SEXP test_by_lvalue_ref(r_vec<r_dbl>& x){
  return x;
}

[[cppally::register]]
SEXP test_by_rvalue_ref(r_vec<r_dbl>&& x){
  return x;
}

[[cppally::register]]
SEXP test_by_const_lvalue_ref(const r_vec<r_dbl>& x){
  return x;
}

template <typename T>
requires (is<typename T::data_type, r_dbl>)
[[cppally::register]]
SEXP test_temp_by_value(T x){
  return x;
}
template <typename T>
requires (is<typename T::data_type, r_dbl>)
[[cppally::register]]
SEXP test_temp_by_lvalue_ref(T& x){
  return x;
}
template <typename T>
requires (is<typename T::data_type, r_dbl>)
[[cppally::register]]
SEXP test_temp_by_rvalue_ref(T&& x){
  return x;
}
template <typename T>
requires (is<typename T::data_type, r_dbl>)
[[cppally::register]]
SEXP test_temp_by_const_lvalue_ref(const T& x){
  return x;
}
