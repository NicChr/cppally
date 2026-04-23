#pragma once

#include <cppally.hpp>
using namespace cppally;

template <RSortableVector T>
[[cpp::register]]
T test_range(T x, bool na_rm){
  return range(x, na_rm);
}

template <RMathType T>
[[cpp::register]]
r_dbl test_sum(r_vec<T> x, bool na_rm){
  return sum(x, na_rm);
}

template <RMathType T>
[[cpp::register]]
r_dbl test_mean(r_vec<T> x, bool na_rm){
    return mean(x, na_rm);
}

template <RMathType T>
[[cpp::register]] 
r_dbl test_var(r_vec<T> x, bool na_rm){
  return var(x, na_rm);
}
