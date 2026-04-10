#pragma once

#include <cpp20.hpp>
using namespace cpp20;

// What type is deduced by dispatch?
template <typename T>
[[cpp20::register]]
T scalar_init(T ptype){
	return T();
}

[[cpp20::register]]
r_int64 as_int64(r_int x){
	return as<r_int64>(x);
}

[[cpp20::register]]
r_size_t as_r_size_t(r_int x){
	return as<r_size_t>(x);
}

template <RVector T>
[[cpp20::register]]
int cpp_length(T vec){
	return vec.length();
}

[[cpp20::register]]
r_dbl add_half(r_int x){
  return x + 0.5;
}
