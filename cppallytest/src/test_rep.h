#pragma once

#include <cppally.hpp>
using namespace cppally;

template <RVector T>
[[cppally::register]]
T test_rep_len(T x, int n){
    return rep_len(x, n);
}

template <RVector T>
[[cppally::register]]
T test_rep(T x, r_vec<r_int> times){
    return rep(x, times);
}

template <RVector T>
[[cppally::register]]
T test_rep_each(T x, r_vec<r_int> each){
    return rep_each(x, each);
}
