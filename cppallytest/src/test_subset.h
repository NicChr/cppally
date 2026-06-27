#pragma once

#include <cppally.hpp>
using namespace cppally;

template <RVector T, typename U>
requires (any<U, r_int, r_lgl, r_str, r_str_view>)
[[cppally::register]]
T test_subset(T x, r_vec<U> i, bool invert){
    return x.subset(i, true, invert);
}

template <RVal T, RVal U>
[[cppally::register]]
r_vec<r_int> test_find(r_vec<T> x, U y){
    return x.find(as<T>(y));
}

