#pragma once

#include <cppally.hpp>
using namespace cppally;

template <typename T>
requires ((RVector<T> && RSortableType<typename T::data_type>) || RFactor<T>)
[[cppally::register]]
r_vec<r_int> test_order(T x){
    if constexpr (RFactor<T>){
        return order(x.value);
    } else {
        return order(x);
    }
}


template <typename T>
requires ((RVector<T> && RSortableType<typename T::data_type>) || RFactor<T>)
[[cppally::register]]
T test_sort(T x){
  auto o = test_order(x);
  return subset(x, o);
}

