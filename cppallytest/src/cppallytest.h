#pragma once

#include <cppally_light.hpp>
using namespace cppally;

template <typename T, typename U>
void expect_identical(const T& x, const U& target){
    bool test_passed = identical(x, target);
    if (!test_passed){
        abort("expect_identical: `x` is not identical to `target`");
    }
}
