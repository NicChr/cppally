#ifndef CPPALLY_R_INT_H
#define CPPALLY_R_INT_H

#include <cppally/r_concepts.h>

namespace cppally {

// R integer
struct r_int {
    int value;
    using value_type = int;
    r_int() : value{0} {}
    template <CppMathType T>
    requires (internal::can_definitely_be_int<T>())
    explicit constexpr r_int(T x) : value{static_cast<int>(x)} {}
    constexpr operator int() const { return value; }
};

}

#endif
