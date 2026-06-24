#pragma once

#include <cppally.hpp>
using namespace cppally;

template <RVector T>
[[cppally::register]]
r_vec<r_lgl> vec_is_na(const T& x){
    return pmap([](auto v) -> r_lgl { return r_lgl(is_na(v)); }, x);
}

template <RVector T>
[[cppally::register]]
r_vec<r_sexp> test_nas(T const& x){
   return make_vec<r_sexp>(
    arg("is_na") = vec_is_na(x), 
    arg("na_count") = as<r_int>(x.na_count()),
    arg("any_na") = x.any_val(na<typename T::data_type>()),
    arg("all_na") = x.all_val(na<typename T::data_type>()));
}

[[cppally::register]]
inline r_vec<r_sexp> test_na_types(){
   return make_vec<r_sexp>(
    na<r_lgl>(), 
    na<r_int>(),
    na<r_dbl>(),
    as<r_vec<r_str>>(na<r_str>()),
    as<r_vec<r_str_view>>(na<r_str_view>()),
    na<r_date>(),
    na<r_psxct>()
);
}
