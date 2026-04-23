#pragma once

#include <cppally.hpp>
using namespace cppally;

template <typename T>
requires ((RVector<T> && RSortableType<typename T::data_type>) || RFactor<T>)
[[cpp::register]]
r_vec<r_int> test_order(T x, bool preserve_ties){
    if constexpr (RFactor<T>){
        return order(x.value, preserve_ties);
    } else {
        return order(x, preserve_ties);
    }
}


template <typename T>
requires ((RVector<T> && RSortableType<typename T::data_type>) || RFactor<T>)
[[cpp::register]]
T test_sort(T x, bool preserve_ties){
  auto o = test_order(x, preserve_ties);
  return x.subset(o);
}

