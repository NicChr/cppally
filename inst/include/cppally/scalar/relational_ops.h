#ifndef CPPALLY_R_RELATIONAL_OPS_H
#define CPPALLY_R_RELATIONAL_OPS_H

// ------- Custom operators for cppally scalars -------
// NA handling mirrors R's NA handling.
// License: MIT License
// Author: Nick Christofides

// Relational operators: ==,!=,<=,<,>=,>

#include <cppally/r_setup.h>
#include <cppally/r_concepts.h>
#include <cppally/scalar/scalars.h>
#include <cppally/na.h>
#include <string_view>

namespace cppally {

// Relational operators

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
