#pragma once

#include <cppally.hpp>
using namespace cppally;

template <RVector T>
[[cpp::register]]
r_vec<r_sexp> test_nas(T const& x){
   return make_vec<r_sexp>(
    arg("is_na") = x.is_na(), 
    arg("na_count") = as<r_int>(x.na_count()),
    arg("any_na") = x.any_na(), 
    arg("all_na") = x.all_na()
);
}

[[cpp::register]]
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
