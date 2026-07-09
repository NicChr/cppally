#pragma once

#include <cppally.hpp>
using namespace cppally;

// Positional indices are supplied 1-based (like R) and shifted to cppally's
// 0-based convention here, so the R tests can compare against base `[`.
// Logical masks and names have no offset.
template <RVector T, RScalar U>
requires (any<U, r_int, r_lgl, r_str>)
[[cppally::register]]
T test_subset(T x, r_vec<U> i, bool invert){
    if constexpr (is<U, r_int>){
        i = i - 1;
    }
    return x.subset(i, invert, true);
}

template <RVal T, RVal U>
[[cppally::register]]
r_vec<r_int> test_find(r_vec<T> x, U y){
    return x.find(as<T>(y));
}
