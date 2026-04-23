#pragma once

#include <cppally.hpp>
using namespace cppally;

template <RVector T, typename U>
requires (any<U, r_int, r_lgl, r_str, r_str_view>)
[[cpp::register]]
T test_subset(T x, r_vec<U> i, bool invert){
    return x.subset(i, true, invert);
}

template <RVal T, RVal U>
[[cpp::register]]
r_vec<T> test_fill(r_vec<T> x, r_vec<r_int> where, r_vec<U> with){
    r_vec<T> out = deep_copy(x);
    out.fill(where, as<r_vec<T>>(with));
    return out;
}

template <RVal T, RVal U>
[[cpp::register]]
inline r_int test_counts(r_vec<T> x, r_vec<U> y){
   return as<r_int>(x.count(as<r_vec<T>>(y)));
}

template <RVal T, RVal U>
[[cpp::register]]
inline r_vec<T> test_remove(r_vec<T> x, r_vec<U> y){
   return x.remove(as<r_vec<T>>(y));
}

template <RVal T, RVal U>
[[cpp::register]]
r_vec<r_int> test_find(r_vec<T> x, r_vec<U> y){
    return x.find(as<r_vec<T>>(y));
}

template <RVal T, RVal U>
[[cpp::register]]
r_vec<T> test_replace(r_vec<T> x, r_vec<U> y, r_vec<U> z){
    r_vec<T> out = deep_copy(x);
    out.replace(as<r_vec<T>>(y), as<r_vec<T>>(z));
    return out;
}
