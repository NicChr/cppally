#pragma once

#include <cppally.hpp>
using namespace cppally;

[[cppally::register]]
SEXP test_by_value(r_vec<r_dbl> x){
  return x;
}

[[cppally::register]]
SEXP test_by_lvalue(r_vec<r_dbl>& x){
  return x;
}

[[cppally::register]]
SEXP test_by_rvalue(r_vec<r_dbl>&& x){
  return x;
}

[[cppally::register]]
SEXP by_const_lvalue(const r_vec<r_dbl>& x){
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
SEXP test_temp_by_lvalue(T& x){
  return x;
}
template <typename T>
requires (is<typename T::data_type, r_dbl>)
[[cppally::register]]
SEXP test_temp_by_rvalue(T&& x){
  return x;
}
template <typename T>
requires (is<typename T::data_type, r_dbl>)
[[cppally::register]]
SEXP temp_by_const_lvalue(const T& x){
  return x;
}
