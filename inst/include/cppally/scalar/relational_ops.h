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
// We only compare bytes for UTF-8 strings.
inline bool str_bytes_comparable(r_str_view x, r_str_view y) noexcept {
  return x.is_utf8() && y.is_utf8();
}

// Byte-wise (C-locale) comparison of the UTF-8 text, strings marked as bytes are compared as-is
CPPALLY_NOINLINE inline int str_compare(r_str_view x, r_str_view y) {
  int cmp = str_bytes_comparable(x, y) ? std::strcmp(x.c_str(), y.c_str()) : utf8_string(x).compare(utf8_string(y));
  return cmp == 0 ? static_cast<int>(x.encoding() == str_encoding::bytes) - static_cast<int>(y.encoding() == str_encoding::bytes) : cmp;
}
CPPALLY_NOINLINE inline int str_compare(r_str_view x, std::string_view y) {
  if (x.is_utf8()){
    // C++ strings are assumed to be UTF-8
    return x.cpp_str().compare(y);
  }
  int cmp = utf8_string(x).compare(y);
  // Bytes (non-text) and text should not be marked equal
  // We are deciding to always order bytes AFTER text
  return (cmp == 0 && x.encoding() == str_encoding::bytes) ? 1 : cmp;
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
