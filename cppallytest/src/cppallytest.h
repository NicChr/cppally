#pragma once

#include <cppally_light.hpp>
using namespace cppally;

template <CastableToRScalar T, CastableToRScalar U>
void expect_identical(const T& x, const U& target){
    bool test_passed = identical(x, target);
    if (!test_passed){
        abort("expect_identical: `x` is not identical to `target`");
    }
}

template <CastableToRScalar T, CastableToRScalar U>
void expect_identical(T x, U target, const char* what){
    if (!identical(x, target)){
        abort("test failure for: %s", what);
    }
}
