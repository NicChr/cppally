#pragma once

#include <cpp20.hpp>
using namespace cpp20;

template <RVector T>
[[cpp20::register]]
T test_subset(T x, r_vec<r_int> i){
    return x.subset(i);
}
