#ifndef CPPALLY_STD_COMPAT_H
#define CPPALLY_STD_COMPAT_H

// Fallbacks for C++20 library features missing from older libc++ (e.g. CRAN's macOS 11 SDK)

#include <version>
#include <utility>
#include <bit>
#include <type_traits>

namespace cppally {

namespace internal {

#ifdef __cpp_lib_integer_comparison_functions
using std::cmp_less;
using std::cmp_less_equal;
using std::cmp_greater_equal;
#else
template <typename T, typename U>
constexpr bool cmp_less(T t, U u) noexcept {
  if constexpr (std::is_signed_v<T> == std::is_signed_v<U>){
    return t < u;
  } else if constexpr (std::is_signed_v<T>){
    return t < 0 || std::make_unsigned_t<T>(t) < u;
  } else {
    return u >= 0 && t < std::make_unsigned_t<U>(u);
  }
}

template <typename T, typename U>
constexpr bool cmp_less_equal(T t, U u) noexcept {
  return !cmp_less(u, t);
}

template <typename T, typename U>
constexpr bool cmp_greater_equal(T t, U u) noexcept {
  return !cmp_less(t, u);
}
#endif

#ifdef __cpp_lib_bit_cast
using std::bit_cast;
#else
template <typename To, typename From>
constexpr To bit_cast(const From& from) noexcept {
  return __builtin_bit_cast(To, from);
}
#endif

}

}

#endif
