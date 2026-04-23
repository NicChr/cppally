#pragma once

#include <cppally.hpp>
using namespace cppally;

// What type is deduced by dispatch?
template <typename T>
[[cpp::register]]
r_vec<r_str> test_deduced_type(T x){
  return r_vec<r_str>(1, r_str(internal::type_str<decltype(x)>()));
}

// Testing a few things here at once:
// R_NilValue can be returned
// Multiple args with the same template deduce to the same type
template <typename T>
[[cpp::register]]
r_sexp test_multiple_deduction(T x, T y){
  if (!is<decltype(x), decltype(y)>){
    abort("deduced type of x: %s does not match deduced type of y %s", internal::type_str<decltype(x)>(), internal::type_str<decltype(y)>());
  }
  return r_null;
}

// Deduced type when constraint is a vector
template <RVector T>
[[cpp::register]]
r_vec<r_str> test_deduced_vec_type(T x){
  return r_vec<r_str>(1, r_str(internal::type_str<decltype(x)>()));
}

// Deduced type when constraint is a scalar
template <RScalar T>
[[cpp::register]]
r_vec<r_str> test_deduced_scalar_type(T x){
  return r_vec<r_str>(1, r_str(internal::type_str<decltype(x)>()));
}

template <RVal T>
[[cpp::register]]
r_vec<r_str> test_deduced_scalar_type2(T x){
  return r_vec<r_str>(1, r_str(internal::type_str<decltype(x)>()));
}

// Super permissive identity fn
template <typename T>
[[cpp::register]]
T test_identity(T x){
  return x;
}

template <typename T>
[[cpp::register]]
r_sexp test_template_null(T x){
  return r_null;
}

// Generic type, non RVal input + output
template <RIntegerType T>
[[cpp::register]]
inline int test_scalar(int x, T y){
  return x + unwrap(y);
}
template <CppIntegerType T>
[[cpp::register]]
inline int test_scalar2(r_int x, T y){
  return x + unwrap(y);
}

template <IntegerType T>
[[cpp::register]]
inline r_int test_scalar3(r_int x, T y){
  return as<r_int>(x + y);
}

template <RVal T>
[[cpp::register]]
T test_rval_identity(T x){
  return x;
}

// 1 scalar arg
template <RMathType T>
[[cpp::register]]
r_dbl scalar1(T x){
  return as<r_dbl>(x);
}

// 1 scalar arg + more complex return value
template <RMathType T>
[[cpp::register]]
unwrap_t<T> scalar2(T x){
  return unwrap(x);
}

// 1 vector arg
template <RMathType T>
[[cpp::register]]
r_dbl vector1(r_vec<T> x){
  return as<r_dbl>(x.get(0));
}

// RVector template typename
template <RVector T>
[[cpp::register]]
r_dbl vector2(T x){
  return as<r_dbl>(x.get(1));
}

// 2 scalar args (same typename)
template <RMathType T>
[[cpp::register]]
r_dbl scalar3(T x, T y){
  return as<r_dbl>(x + y);
}
// 2 scalar args (different typenames)
template <RMathType T, RMathType U>
[[cpp::register]]
r_dbl scalar4(T x, U y){
  return as<r_dbl>(x + y);
}
// SEXP return
template <RVector T>
[[cpp::register]]
SEXP test_sexp(T x){
  return x.sexp;
}

template <typename T>
requires (is_sexp<T>)
[[cpp::register]]
T test_sexp4(T x){
  return x;
}

// scalar and vector args (same typenames)
template <RMathType T>
[[cpp::register]]
r_vec<T> scalar_vec1(r_vec<T> a, T b){
  return as<r_vec<T>>(a + b);
}

// scalar and vector args (different typenames)
template <RMathType T, RMathType U>
[[cpp::register]]
r_vec<T> scalar_vec2(r_vec<T> a, U b){
  return as<r_vec<T>>(a + b);
}
// scalar and vector args 
template <RMathType T, RMathType U>
[[cpp::register]]
r_vec<T> scalar_vec3(r_vec<T> z, T x, U y, r_vec<U> a){
  return as<r_vec<T>>(x + y + z + a);
}
// Complicated mix of scalar, vector and plain C types
template <RMathType T, RVector V>
requires (RMathType<typename V::data_type>)
[[cpp::register]]
r_vec<T> test_mix2(r_vec<T> a, double b, T c, int d, T e, T f, V g){
  return as<r_vec<T>>(a + b + c + d + e + f + g);
}

template <RStringType T>
[[cpp::register]]
inline r_vec<r_str> test_str3(T x){
  return as<r_vec<r_str>>(x);
}
template <RStringType T>
[[cpp::register]]
inline r_vec<r_str> test_str4(T x){
  return as<r_vec<r_str>>(x);
}

// Testing template specialisations
template <RMathType T>
[[cpp::register]]
r_vec<T> test_specialisation(r_vec<T> x) {
  return r_vec<T>(1, T(0)); 
}

template <> 
inline r_vec<r_int> test_specialisation<r_int>(r_vec<r_int> x) { 
  return r_vec<r_int>(1, r_int(1)); 
}


template <RVal T, typename U>
[[cpp::register]]
auto test_coerce(r_vec<T> x, U ptype) {
  return as<U>(x);
} 

[[cpp::register]]
SEXP test_list_to_scalars(r_vec<r_sexp> x){
  return make_vec<r_sexp>(as<r_lgl>(x), as<r_int>(x), as<r_dbl>(x), make_vec<r_str>(as<r_str>(x)), as<r_sexp>(x), as<r_sym>(x));
}
template <RVal T>
[[cpp::register]]
auto test_combine2(T x, T y){
  return make_vec<r_sexp>(
    arg("first") = make_vec<T>(x, y),
    arg("second") = make_vec<T>(arg("x") = x, arg("y") = y)
  );
}

template <RVector T>
requires (is<T, r_vec<r_date>>)
[[cpp::register]]
T test_dates2(T x){
  return x;
}

template <RVector T>
[[cpp::register]]
T test_unique(T x){
  return unique(x);
}

template <RNumericType U, RNumericType V>
[[cpp::register]]
r_vec<r_sexp> test_seqs(r_vec<r_int> size, r_vec<U> from, r_vec<V> by){
  return sequences(size, from, by);
}

// r_vec<r_sexp> test_time_coerce(){
//   return make_vec<r_sexp>(
//     r_date(0),
//     r_psxct(0),

//     as<r_date_t<r_int>>(r_psxct_t<r_dbl>(0)),
//     as<r_date_t<r_int>>(r_psxct_t<r_int64>(0)),
//     as<r_date_t<r_int>>(r_dbl(0)),
//     as<r_date_t<r_dbl>>(r_psxct_t<r_dbl>(0)),
//     as<r_date_t<r_dbl>>(r_psxct_t<r_int64>(0)),
//     as<r_date_t<r_dbl>>(r_int(0)),

//     as<r_psxct_t<r_dbl>>(as<r_psxct_t<r_int64>>(r_date_t<r_dbl>(0))),
//     as<r_psxct_t<r_dbl>>(as<r_psxct_t<r_int64>>(r_date_t<r_int>(0))),
//     as<r_psxct_t<r_dbl>>(as<r_psxct_t<r_int64>>(r_dbl(0))),
//     as<r_psxct_t<r_dbl>>(r_date_t<r_dbl>(0)),
//     as<r_psxct_t<r_dbl>>(r_date_t<r_int>(0)),
//     as<r_psxct_t<r_dbl>>(r_int(0))
//   );

// }


template <RVector T>
[[cpp::register]]
r_vec<r_int> test_group_id(T x, bool order){
  auto out = make_groups(x, order).ids;
  out += 1;
  return out;
}

template <RVector T>
[[cpp::register]]
r_vec<r_int> test_group_counts(T x, bool order){
  return make_groups(x, order).counts();
}

template <RVal T>
[[cpp::register]]
auto test_match(r_vec<T> x, r_vec<T> y){
  return match(x, y);
}

template <RFactor T>
[[cpp::register]]
r_vec<r_sexp> test_factor2(T x){
  return make_vec<r_sexp>(x, x.levels(), x.value, r_factors(), r_factors(3), as<r_vec<r_str_view>>(x), as<r_vec<r_str_view>>(x));
}

template <RVector T>
[[cpp::register]]
int test_n_unique(T x){
  return n_unique(x);
}

[[cpp::register]]
SEXP test_copy(SEXP x){ 
  return deep_copy(r_sexp(x));
}

[[cpp::register]]
bool test_identical(SEXP x, SEXP y){
  return identical(x, y);
}

template <
RMathType T,
RMathType U
>
[[cpp::register]]
T test_multiline_template_add(T x, U y){
  return as<T>(x + y);
}

template
<
RMathType T,
RMathType U
>
[[cpp::register]]
T test_multiline_template_add2(T x, U y){
  return as<T>(x + y);
}

template <
RMathType T,
RMathType U
>
// [[cpp::register]]
T test_multiline_template_add3(T x, U y){
  return as<T>(x + y);
}



// template <
// RMathType T,
// RMathType U
// >
// // [[cpp::register]]
// T test_multiline_template_add2(T x, U y){
//   return as<T>(x + y);
// }
