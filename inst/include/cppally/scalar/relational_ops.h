#ifndef CPPALLY_R_RELATIONAL_OPS_H
#define CPPALLY_R_RELATIONAL_OPS_H

// ------- Custom operators for cppally scalars -------
// NA handling mirrors R's NA handling.
// License: MIT License
// Author: Nick Christofides

// Relational operators: ==,!=,<=,<,>=,>

#include <cppally/r_setup.h>
#include <cppally/r_concepts.h>
#include <cppally/utils.h>
#include <cppally/scalar/scalars.h>
#include <cppally/na.h>
#include <cstring> // For strcmp
#include <string_view>

namespace cppally {

// Relational operators

namespace internal {

namespace string {

// Can bytes be compared directly?
// true when either both are UTF-8 or both are bytes-encoded.
inline bool str_bytes_comparable(r_str_view x, r_str_view y) noexcept {
  return (x.is_utf8() && y.is_utf8()) || (x.encoding() == str_encoding::bytes && y.encoding() == str_encoding::bytes);
}

// Text orders before bytes strings. Text compares by its UTF-8 bytes (C-locale), bytes strings by their raw bytes
CPPALLY_NOINLINE inline int str_compare(r_str_view x, r_str_view y) {
  if (str_bytes_comparable(x, y)) {
    return std::strcmp(x.c_str(), y.c_str());
  }
  bool x_bytes = x.encoding() == str_encoding::bytes;
  bool y_bytes = y.encoding() == str_encoding::bytes;
  if (x_bytes != y_bytes) [[unlikely]] {
    return x_bytes ? 1 : -1;
  }
  return utf8_string(x).compare(utf8_string(y));
}
CPPALLY_NOINLINE inline int str_compare(r_str_view x, std::string_view y) {
  if (x.is_utf8()){
    // C++ strings are assumed to be UTF-8
    return x.cpp_str().compare(y);
  }
  // C++ strings are text, so bytes strings order after them
  if (x.encoding() == str_encoding::bytes) {
    return 1;
  }
  return utf8_string(x).compare(y);
}

// Assumes unwrap(x) != unwrap(y)
CPPALLY_NOINLINE inline bool str_equal_impl(r_str_view x, r_str_view y) {

  // Since ASCII is always unmarked and this fn assumes different pointers/addresses, then no match here
  if (x.is_ascii() || y.is_ascii()){
    return false;
  }

  // The same encoding at different addresses is different text
  if (x.encoding() == y.encoding()){
    return false;
  }
  return str_compare(x, y) == 0;
}

inline bool str_equal(r_str_view x, r_str_view y) {
  return r_addresses_equal(x, y) || str_equal_impl(x, y);
}

}

}

// Operators for r_str_view

inline r_lgl operator==(r_str_view lhs, r_str_view rhs) {
  return (lhs.is_na() || rhs.is_na()) ? r_na : r_lgl{internal::string::str_equal(lhs, rhs)};
}
inline r_lgl operator!=(r_str_view lhs, r_str_view rhs) {
  return (lhs.is_na() || rhs.is_na()) ? r_na : r_lgl{!internal::string::str_equal(lhs, rhs)};
}
inline r_lgl operator<(r_str_view lhs, r_str_view rhs) {
  if (lhs.is_na() || rhs.is_na()){
    return r_na;
  } else if (internal::r_addresses_equal(lhs, rhs)){
    return r_false;
  } else {
    return r_lgl{internal::string::str_compare(lhs, rhs) < 0};
  }
}
inline r_lgl operator<=(r_str_view lhs, r_str_view rhs) {
  if (lhs.is_na() || rhs.is_na()){
    return r_na;
  } else if (internal::r_addresses_equal(lhs, rhs)){
    return r_true;
  } else {
    return r_lgl{internal::string::str_compare(lhs, rhs) <= 0};
  }
}
inline r_lgl operator>(r_str_view lhs, r_str_view rhs) {
  if (lhs.is_na() || rhs.is_na()){
    return r_na;
  } else if (internal::r_addresses_equal(lhs, rhs)){
    return r_false;
  } else {
    return r_lgl{internal::string::str_compare(lhs, rhs) > 0};
  }
}
inline r_lgl operator>=(r_str_view lhs, r_str_view rhs) {
  if (lhs.is_na() || rhs.is_na()){
    return r_na;
  } else if (internal::r_addresses_equal(lhs, rhs)){
    return r_true;
  } else {
    return r_lgl{internal::string::str_compare(lhs, rhs) >= 0};
  }
}

inline r_lgl operator==(const r_str& lhs, const r_str& rhs) {
  return static_cast<r_str_view>(lhs) == static_cast<r_str_view>(rhs);
}
inline r_lgl operator!=(const r_str& lhs, const r_str& rhs) {
  return static_cast<r_str_view>(lhs) != static_cast<r_str_view>(rhs);
}
inline r_lgl operator<(const r_str& lhs, const r_str& rhs) {
  return static_cast<r_str_view>(lhs) < static_cast<r_str_view>(rhs);
}
inline r_lgl operator<=(const r_str& lhs, const r_str& rhs) {
  return static_cast<r_str_view>(lhs) <= static_cast<r_str_view>(rhs);
}
inline r_lgl operator>(const r_str& lhs, const r_str& rhs) {
  return static_cast<r_str_view>(lhs) > static_cast<r_str_view>(rhs);
}
inline r_lgl operator>=(const r_str& lhs, const r_str& rhs) {
  return static_cast<r_str_view>(lhs) >= static_cast<r_str_view>(rhs);
}

// r_str/C++ string operators

inline r_lgl operator==(r_str_view lhs, std::string_view rhs) {
  return lhs.is_na() ? r_lgl::na() : r_lgl(internal::string::str_compare(lhs, rhs) == 0);
}
inline r_lgl operator!=(r_str_view lhs, std::string_view rhs) {
  return lhs.is_na() ? r_lgl::na() : r_lgl(internal::string::str_compare(lhs, rhs) != 0);
}
inline r_lgl operator<(r_str_view lhs, std::string_view rhs) {
  return lhs.is_na() ? r_lgl::na() : r_lgl(internal::string::str_compare(lhs, rhs) < 0);
}
inline r_lgl operator<=(r_str_view lhs, std::string_view rhs) {
  return lhs.is_na() ? r_lgl::na() : r_lgl(internal::string::str_compare(lhs, rhs) <= 0);
}
inline r_lgl operator>(r_str_view lhs, std::string_view rhs) {
  return lhs.is_na() ? r_lgl::na() : r_lgl(internal::string::str_compare(lhs, rhs) > 0);
}
inline r_lgl operator>=(r_str_view lhs, std::string_view rhs) {
  return lhs.is_na() ? r_lgl::na() : r_lgl(internal::string::str_compare(lhs, rhs) >= 0);
}

inline r_lgl operator==(std::string_view lhs, r_str_view rhs) {
  return rhs == lhs;
}
inline r_lgl operator!=(std::string_view lhs, r_str_view rhs) {
  return rhs != lhs;
}
inline r_lgl operator<(std::string_view lhs, r_str_view rhs) {
  return rhs > lhs;
}
inline r_lgl operator<=(std::string_view lhs, r_str_view rhs) {
  return rhs >= lhs;
}
inline r_lgl operator>(std::string_view lhs, r_str_view rhs) {
  return rhs < lhs;
}
inline r_lgl operator>=(std::string_view lhs, r_str_view rhs) {
  return rhs <= lhs;
}

template <RScalar T, RScalar U>
requires (!(RStringType<T> && RStringType<U>) && requires (unwrap_t<T> a, unwrap_t<U> b) { a == b; })
inline constexpr r_lgl operator==(const T& lhs, const U& rhs) noexcept {
  return (internal::either_na(lhs, rhs)) ? r_na : r_lgl{unwrap(lhs) == unwrap(rhs)};
}

template <RScalar T, CppScalar U>
requires (requires (unwrap_t<T> a, unwrap_t<U> b) { a == b; })
inline constexpr r_lgl operator==(const T& lhs, const U& rhs) noexcept {
  return (internal::either_na(lhs, rhs)) ? r_na : r_lgl{unwrap(lhs) == unwrap(rhs)};
}

template <CppScalar T, RScalar U>
requires (requires (unwrap_t<T> a, unwrap_t<U> b) { a == b; })
inline constexpr r_lgl operator==(const T& lhs, const U& rhs) noexcept {
  return (internal::either_na(lhs, rhs)) ? r_na : r_lgl{unwrap(lhs) == unwrap(rhs)};
}

// Need to have 3 overloads otherwise compiler complains about lhs != rhs

template <RScalar T, RScalar U>
requires (!(RStringType<T> && RStringType<U>) && requires (unwrap_t<T> a, unwrap_t<U> b) { a != b; })
inline constexpr r_lgl operator!=(const T& lhs, const U& rhs) noexcept {
  return (internal::either_na(lhs, rhs)) ? r_na : r_lgl{unwrap(lhs) != unwrap(rhs)};
}

template <RScalar T, CppScalar U>
requires (requires (unwrap_t<T> a, unwrap_t<U> b) { a != b; })
inline constexpr r_lgl operator!=(const T& lhs, const U& rhs) noexcept {
  return (internal::either_na(lhs, rhs)) ? r_na : r_lgl{unwrap(lhs) != unwrap(rhs)};
}

template <CppScalar T, RScalar U>
requires (requires (unwrap_t<T> a, unwrap_t<U> b) { a != b; })
inline constexpr r_lgl operator!=(const T& lhs, const U& rhs) noexcept {
  return (internal::either_na(lhs, rhs)) ? r_na : r_lgl{unwrap(lhs) != unwrap(rhs)};
}

template <NumericType T, NumericType U>
requires (RNumericType<T> || RNumericType<U>)
inline constexpr r_lgl operator<(T lhs, U rhs) noexcept {
  return (internal::either_na(lhs, rhs)) ? r_na : r_lgl{unwrap(lhs) < unwrap(rhs)};
}
template <NumericType T, NumericType U>
requires (RNumericType<T> || RNumericType<U>)
inline constexpr r_lgl operator<=(T lhs, U rhs) noexcept {
  return (internal::either_na(lhs, rhs)) ? r_na : r_lgl{unwrap(lhs) <= unwrap(rhs)};
}
template <NumericType T, NumericType U>
requires (RNumericType<T> || RNumericType<U>)
inline constexpr r_lgl operator>(T lhs, U rhs) noexcept {
  return (internal::either_na(lhs, rhs)) ? r_na : r_lgl{unwrap(lhs) > unwrap(rhs)};
}
template <NumericType T, NumericType U>
requires (RNumericType<T> || RNumericType<U>)
inline constexpr r_lgl operator>=(T lhs, U rhs) noexcept {
  return (internal::either_na(lhs, rhs)) ? r_na : r_lgl{unwrap(lhs) >= unwrap(rhs)};
}

}

#endif
