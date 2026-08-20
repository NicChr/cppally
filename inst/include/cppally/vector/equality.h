#ifndef CPPALLY_R_EQUAL_H
#define CPPALLY_R_EQUAL_H

#include <cppally/functional/pmap.h>
#include <cppally/length.h>
#include <cppally/identical.h>
#include <cppally/coerce.h>
#include <cppally/vector/vector_ops.h>

// Vectorised `==` operator for R vectors.
// `operator==` has already been defined when both lhs and rhs satisfy RAtomicVector in vector_ops.h

namespace cppally {

inline r_vec<r_lgl> operator==(const r_vec<r_sexp>& lhs, const r_vec<r_sexp>& rhs) {
  return pmap([](const r_sexp& a, const r_sexp& b) {
    return r_lgl(identical(a, b));
  }, 
  lhs, rhs);
}

inline r_vec<r_lgl> operator==(const r_factors& lhs, const r_factors& rhs) {
  // Position of each of rhs's levels within lhs's levels; -1 = absent from lhs
  r_vec<r_int> remap = pmap(
    /*fn = */ [&lhs](const auto& lvl){
      return lhs.get_code(lvl, /*no_match = */ r_int(-1)); 
    },
    rhs.levels()
  );

  r_size_t n = rhs.length();
  r_vec<r_int> comparable(n);
  for (r_size_t i = 0; i < n; ++i){
    r_int c = rhs.value.get(i);
    // Genuine NA stays NA; a level absent from lhs is known-unequal, not unknown
    comparable.set(i, is_na(c) ? na<r_int>() : remap.get(unwrap(c) - 1));
  }
  return lhs.value == comparable;
}

template <typename T, typename U>
requires (
  ( (RVector<T> || RFactor<T>) && (RVector<U> || RFactor<U>)) && 
  (!RAtomicVector<T> || !RAtomicVector<U>)
)
inline r_vec<r_lgl> operator==(const T& lhs, const U& rhs){
  
  using common_t = common_r_t<T, U>;

  common_t a = as<common_t>(lhs);
  common_t b = as<common_t>(rhs);

  return a == b;
}

template <typename T, typename U>
requires (
  requires (const T& a, const U& b) { a == b; } &&
  is<decltype(std::declval<const T&>() == std::declval<const U&>()), r_vec<r_lgl>>
)
inline r_vec<r_lgl> operator!=(const T& lhs, const U& rhs) {
  return !(lhs == rhs);
}

}

#endif
