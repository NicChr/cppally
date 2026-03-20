#pragma once

#include <cpp20.hpp>
using namespace cpp20;

template <typename T>
requires ((RVector<T> && RSortableType<typename T::data_type>) || RFactor<T>)
[[cpp20::register]]
r_vec<r_int> test_order(T x, bool preserve_ties){
    if constexpr (RFactor<T>){
        return order(x.value, preserve_ties);
    } else {
        return order(x, preserve_ties);
    }
}


template <typename T>
requires ((RVector<T> && RSortableType<typename T::data_type>) || RFactor<T>)
[[cpp20::register]]
T test_sort(T x, bool preserve_ties){
  auto o = test_order(x, preserve_ties);
  o += 1;
  return x.subset(o);
}

