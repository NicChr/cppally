#ifndef CPPALLY_R_INT64_H
#define CPPALLY_R_INT64_H

#include <cppally/r_concepts.h>
#include <cstdint> // For int64_t

namespace cppally {

// R integer64 (closely mimicking how bit64 defines it)
struct r_int64 {
    int64_t value;
    using value_type = int64_t;
    constexpr r_int64() noexcept : value{0} {}
    template <CppIntegerType T>
    requires (internal::lossless_numeric_cast<T, int64_t>())
    explicit constexpr r_int64(T x) noexcept : value{static_cast<int64_t>(x)} {}
    template <typename U> requires (is<U, int64_t>)
    constexpr operator U() const noexcept { return value; }

    static constexpr r_int64 na() noexcept {
        return r_int64(std::numeric_limits<int64_t>::min());
    }

    constexpr bool is_na() const noexcept {
        return value == na().value;
    }
};

namespace internal {
inline constexpr r_int64 na_int64 = r_int64::na();

}

}

#endif
