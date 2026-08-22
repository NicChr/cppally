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

namespace cppally {

// Relational operators

// Operators for r_str_view

inline r_lgl operator<(r_str_view lhs, r_str_view rhs) noexcept {
  if (internal::either_na(lhs, rhs)){
    return r_na;
  } else if (unwrap(lhs) == unwrap(rhs)){
    return r_false;
  } else {
    return r_lgl{std::strcmp(lhs.c_str(), rhs.c_str()) < 0};
  }
}
inline r_lgl operator<=(r_str_view lhs, r_str_view rhs) noexcept {
  if (internal::either_na(lhs, rhs)){
    return r_na;
  } else if (unwrap(lhs) == unwrap(rhs)){
    return r_true;
  } else {
    return r_lgl{std::strcmp(lhs.c_str(), rhs.c_str()) < 0};
  }
}
inline r_lgl operator>(r_str_view lhs, r_str_view rhs) noexcept {
  if (internal::either_na(lhs, rhs)){
    return r_na;
  } else if (unwrap(lhs) == unwrap(rhs)){
    return r_false;
  } else {
    return r_lgl{std::strcmp(lhs.c_str(), rhs.c_str()) > 0};
  }
}
inline r_lgl operator>=(r_str_view lhs, r_str_view rhs) noexcept {
  if (internal::either_na(lhs, rhs)){
    return r_na;
  } else if (unwrap(lhs) == unwrap(rhs)){
    return r_true;
  } else {
    return r_lgl{std::strcmp(lhs.c_str(), rhs.c_str()) > 0};
  }
}

inline r_lgl operator<(const r_str& lhs, const r_str& rhs) noexcept {
  return static_cast<r_str_view>(lhs) < static_cast<r_str_view>(rhs);
}
inline r_lgl operator<=(const r_str& lhs, const r_str& rhs) noexcept {
  return static_cast<r_str_view>(lhs) <= static_cast<r_str_view>(rhs);
}
inline r_lgl operator>(const r_str& lhs, const r_str& rhs) noexcept {
  return static_cast<r_str_view>(lhs) > static_cast<r_str_view>(rhs);
}
inline r_lgl operator>=(const r_str& lhs, const r_str& rhs) noexcept {
  return static_cast<r_str_view>(lhs) >= static_cast<r_str_view>(rhs);
}

template <RScalar T, RScalar U>
requires (requires (unwrap_t<T> a, unwrap_t<U> b) { a == b; })
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
requires (requires (unwrap_t<T> a, unwrap_t<U> b) { a != b; })
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
