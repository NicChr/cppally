#pragma once

#include <cppally.hpp>
using namespace cppally;

[[cppally::register]]
auto test_optional_add(r_dbl x, std::optional<r_int> y){
    if (y.has_value()){
        return x + *y;
    } else {
        return x + r_int(0);
    }
}


[[cppally::register]]
std::optional<r_dbl> test_optional_return(r_dbl x){
    if (x == 0){
        return std::nullopt;
    } else {
        return x;
    }
}

template <RNumber T, RNumber U>
[[cppally::register]]
auto test_optional_add_template(T x, std::optional<U> y){
    if (y.has_value()){
        return x + *y;
    } else {
        return x + U(0);
    }
}

