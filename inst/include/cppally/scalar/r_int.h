#ifndef CPPALLY_R_INT_H
#define CPPALLY_R_INT_H

#include <cppally/r_concepts.h>
#include <limits>

namespace cppally {

// R integer
struct r_int {
    int value;
    using value_type = int;
    constexpr r_int() noexcept : value{0} {}
    template <CppIntegerType T>
    requires (internal::lossless_numeric_cast<T, int>())
    explicit constexpr r_int(T x) noexcept : value{static_cast<int>(x)} {}
    template <typename U> requires (is<U, int>) // Implicit r_int -> int
    constexpr operator U() const noexcept { return value; }

    template <typename U>
    // Explicit r_int -> integer with same width as int (e.g. long on windows)
    requires (!is<U, int> && std::signed_integral<std::remove_cvref_t<U>> && std::numeric_limits<std::remove_cvref_t<U>>::digits == std::numeric_limits<int>::digits)
    explicit constexpr operator U() const noexcept { return value; }

    static constexpr r_int na() noexcept {
        return r_int(std::numeric_limits<int>::min());
    }

    constexpr bool is_na() const noexcept {
        return value == na().value;
    }
};

}

#endif
