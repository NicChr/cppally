#pragma once

#include <cpp20.hpp>

using namespace cpp20;

// What type is deduced by dispatch?
template <typename T>
[[cpp20::register]]
r_vec<r_str> test_deduced_type(T x){
  return r_vec<r_str>(1, r_str(type_str<decltype(x)>()));
}

// Testing a few things here at once:
// R_NilValue can be returned
// Multiple args with the same template deduce to the same type
template <typename T>
[[cpp20::register]]
r_sexp test_multiple_deduction(T x, T y){
  Rprintf("deduced x type: %s\n", type_str<decltype(x)>());
  Rprintf("deduced y type: %s", type_str<decltype(y)>());
  if (!is<decltype(x), decltype(y)>){
    abort("deduced type of x: %s does not match deduced type of y %s", type_str<decltype(x)>(), type_str<decltype(y)>());
  }
  return r_null;
}

// Deduced type when constraint is a vector
template <RVector T>
[[cpp20::register]]
r_vec<r_str> test_deduced_vec_type(T x){
  return r_vec<r_str>(1, r_str(type_str<decltype(x)>()));
}

// Deduced type when constraint is a scalar
template <RScalar T>
[[cpp20::register]]
r_vec<r_str> test_deduced_scalar_type(T x){
  return r_vec<r_str>(1, r_str(type_str<decltype(x)>()));
}

// Super permissive identity fn
template <typename T>
[[cpp20::register]]
T test_identity(T x){
  return x;
}

// Basic identity fn
[[cpp20::register]]
SEXP test_identity2(SEXP x){
  return x;
}


template <typename T>
[[cpp20::register]]
r_sexp test_template_null(T x){
  Rprintf("deduced_type: %s", type_str<decltype(x)>());
  return r_null;
}

// Generic type, non RVal input + output
template <RIntegerType T>
[[cpp20::register]]
inline int test_scalar(int x, T y){
  return x + unwrap(y);
}
template <CppIntegerType T>
[[cpp20::register]]
inline int test_scalar2(r_int x, T y){
  return x + unwrap(y);
}

template <IntegerType T>
[[cpp20::register]]
inline r_int test_scalar3(r_int x, T y){
  return as<r_int>(x + y);
}

template <RVal T>
[[cpp20::register]]
T test_rval_identity(T x){
  return x;
}

// 1 scalar arg
template <RMathType T>
[[cpp20::register]]
r_dbl scalar1(T x){
  return as<r_dbl>(x);
}

// 1 scalar arg + more complex return value
template <RMathType T>
[[cpp20::register]]
unwrap_t<T> scalar2(T x){
  return unwrap(x);
}

// 1 vector arg
template <RMathType T>
[[cpp20::register]]
r_dbl vector1(r_vec<T> x){
  return as<r_dbl>(x.get(0));
}

// RVector template typename
template <RVector T>
[[cpp20::register]]
r_dbl vector2(T x){
  return as<r_dbl>(x.get(1));
}

// 2 scalar args (same typename)
template <RMathType T>
[[cpp20::register]]
r_dbl scalar3(T x, T y){
  return as<r_dbl>(x + y);
}
// 2 scalar args (different typenames)
template <RMathType T, RMathType U>
[[cpp20::register]]
r_dbl scalar4(T x, U y){
  return as<r_dbl>(x + y);
}
// SEXP return
template <RVector T>
[[cpp20::register]]
SEXP test_sexp(T x){
  return x.sexp;
}

template <typename T>
requires (is_sexp<T>)
[[cpp20::register]]
T test_sexp4(T x){
  return x;
}

// scalar and vector args (same typenames)
template <RMathType T>
[[cpp20::register]]
r_vec<T> scalar_vec1(r_vec<T> a, T b){
  return as<r_vec<T>>(a + b);
}

// scalar and vector args (different typenames)
template <RMathType T, RMathType U>
[[cpp20::register]]
r_vec<T> scalar_vec2(r_vec<T> a, U b){
  return as<r_vec<T>>(a + b);
}
// scalar and vector args 
template <RMathType T, RMathType U>
[[cpp20::register]]
r_vec<T> scalar_vec3(r_vec<T> z, T x, U y, r_vec<U> a){
  return as<r_vec<T>>(x + y + z + a);
}
// Complicated mix of scalar, vector and plain C types
template <RMathType T, RVector V>
requires (RMathType<typename V::data_type>)
[[cpp20::register]]
r_vec<T> test_mix2(r_vec<T> a, double b, T c, int d, T e, T f, V g){
  return as<r_vec<T>>(a + b + c + d + e + f + g);
}

// R strings
[[cpp20::register]]
inline r_vec<r_str> test_str1(r_str x){
  return as<r_vec<r_str>>(x);
}

[[cpp20::register]]
inline r_vec<r_str_view> test_str2(r_str_view x){
  return as<r_vec<r_str_view>>(x);
}
template <RStringType T>
[[cpp20::register]]
inline r_vec<r_str> test_str3(T x){
  return as<r_vec<r_str>>(x);
}
template <RStringType T>
[[cpp20::register]]
inline r_vec<r_str> test_str4(T x){
  return as<r_vec<r_str>>(x);
}

template <typename T>
[[cpp20::register]]
inline r_sym test_as_sym(T x){
  return as<r_sym>(x);
}

// Testing template specialisations
template <RMathType T>
[[cpp20::register]]
r_vec<T> test_specialisation(r_vec<T> x) {
  return r_vec<T>(1, T(0)); 
}

template <> 
inline r_vec<r_int> test_specialisation<r_int>(r_vec<r_int> x) { 
  return r_vec<r_int>(1, r_int(1)); 
}


template <RVal T, RVal U>
[[cpp20::register]]
auto test_coerce(r_vec<T> x, r_vec<U> ptype) {
  return as<r_vec<U>>(x);
} 

[[cpp20::register]]
void cpp_set_threads(int n){
  set_threads(n);
}

[[cpp20::register]]
r_int cpp_get_threads(){
  return r_int(get_threads());
}

[[cpp20::register]]
r_sexp test_null(){
  return r_null;
}

[[cpp20::register]]
r_sym test_sym(r_sym x){
  return x;
}

[[cpp20::register]]
r_sexp test_sexp2(r_sexp x){
  return x;
}

[[cpp20::register]]
r_vec<r_sexp> test_sexp3(r_vec<r_sexp> x){
  return x;
}

[[cpp20::register]]
SEXP test_list_to_scalars(r_vec<r_sexp> x){
  return make_vec<r_sexp>(as<r_lgl>(x), as<r_int>(x), as<r_dbl>(x), make_vec<r_str>(as<r_str>(x)), as<r_sexp>(x), as<r_sym>(x));
}

[[cpp20::register]]
r_vec<r_int> test_coerce1(const r_vec<r_sexp>& x){
  return as<r_vec<r_int>>(x);
}

[[cpp20::register]]
r_vec<r_sexp> test_constructions(SEXP x){
  r_vec<r_sexp> out(100000);

  for (int i = 0; i < 100000; ++i){
    out.set(i, r_vec<r_int>(x));
  }
  return out;
}

[[cpp20::register]]
r_vec<r_sexp> test_constructions2(r_vec<r_int> x){
  r_vec<r_sexp> out(100000);

  for (int i = 0; i < 100000; ++i){
    out.set(i, x);
  }
  return out;
}

[[cpp20::register]]
r_vec<r_sexp> test_constructions3(r_vec<r_int> x){
  r_vec<r_sexp> out(100000);

  auto val = as<r_sexp>(x);

  for (int i = 0; i < 100000; ++i){
    out.set(i, val);
  }
  return out;
}

[[cpp20::register]]
r_vec<r_sexp> test_constructions4(r_vec<r_int> x){
  r_vec<r_sexp> out(100000);

  for (int i = 0; i < 100000; ++i){
    SET_VECTOR_ELT(out, i, x);
  }
  return out;
}

[[cpp20::register]]
r_vec<r_str_view> test_set_strs(r_vec<r_str_view> x){

  r_str a = r_str(x.get(0).c_str());

  r_size_t n = x.length();

  for (r_size_t i = 0; i < n; ++i){
    x.set(i, a);
  }
  return x;
}

[[cpp20::register]]
r_vec<r_str_view> test_set_strs2(r_vec<r_str_view> x){

  SEXP a = x.view(0);

  r_size_t n = x.length();

  for (r_size_t i = 0; i < n; ++i){
    SET_STRING_ELT(x, i, a);
  }
  return x;
}
