#pragma once

#include <cppally.hpp>
using namespace cppally;

template <RVector T>
r_vec<r_lgl> vec_is_na(const T& x){
    r_size_t n = x.length();
    r_vec<r_lgl> out(n);
    for (r_size_t i = 0; i < n; ++i){
        out.set(i, is_na(x.get(i)));
    }
    return out;
}

template <RVector T>
[[cppally::register]]
r_vec<r_sexp> test_nas(T const& x){
   return make_vec<r_sexp>(
    arg("is_na") = vec_is_na(x), 
    arg("na_count") = as<r_int>(x.count(na<typename T::data_type>())),
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
