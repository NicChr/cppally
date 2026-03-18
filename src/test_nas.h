#pragma once

#include <cpp20.hpp>
using namespace cpp20;

template <RVector T>
[[cpp20::register]]
r_vec<r_sexp> test_nas(T const& x){
   return make_vec<r_sexp>(
    arg("is_na") = x.is_na(), 
    arg("na_count") = as<r_int>(x.na_count()),
    arg("any_na") = x.any_na(), 
    arg("all_na") = x.all_na()
);
}
