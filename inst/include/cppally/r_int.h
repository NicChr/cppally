#ifndef CPPALLY_R_INT_H
#define CPPALLY_R_INT_H

#include <cppally/r_concepts.h>

namespace cppally {

// R integer
struct r_int {
    int value;
    using value_type = int;
    constexpr r_int() noexcept : value{0} {}
    template <CppMathType T>
    requires (internal::can_definitely_be_int<T>())
    explicit constexpr r_int(T x) noexcept : value{static_cast<int>(x)} {}
    template <typename U> requires (is<U, int>)
    constexpr operator U() const noexcept { return value; }

    constexpr bool is_na() const noexcept {
        return value == std::numeric_limits<int>::min();
    }
};

}

#endif
