#pragma once

#include <cppally.hpp>
using namespace cppally;

// replace_at() mutates in place, so operate on a private copy and return it.
// This keeps the test functional (comparable to base R's `replace()`) and
// correct under both the default and CPPALLY_COPY_ON_MODIFY builds.
//
// cppally uses 0-based positional indices; base R is 1-based. We shift here so
// the R side can compare directly against `replace(x, where, with)`. The shift
// applies only to positional integer subscripts -- logical masks and names have
// no offset.
template <RVector T, typename U>
requires (any<U, r_int, r_lgl, r_str>)
[[cppally::register]]
T test_replace_at(T x, r_vec<U> where, T with){
    T out = shallow_copy(x);
    if constexpr (is<U, r_int>){
        where = where - 1;
    }
    replace_at(out, where, with);
    return out;
}
