#ifndef CPPALLY_R_EQUAL_H
#define CPPALLY_R_EQUAL_H

#include <cppally/r_visit.h>
#include <cppally/r_pmap.h>
#include <cppally/r_length.h>
#include <cppally/r_identical.h>
#include <cppally/sugar/r_vec_ops.h>
#include <cppally/sugar/r_df_methods.h>
#include <cppally/sugar/r_recycle.h>
#include <cppally/sugar/r_sexp_methods.h>

namespace cppally {

template <typename T, typename U>
requires (
  ( (RVector<T> || RFactor<T>) && (RVector<U> || RFactor<U>)) && 
  (!RAtomicVector<T> || !RAtomicVector<U>)
)
inline r_vec<r_lgl> operator==(const T& lhs, const U& rhs){
  using common_t = common_r_t<T, U>;

  common_t a = as<common_t>(lhs);
  common_t b = as<common_t>(rhs);

  r_size_t n = common_length(a, b);

  r_size_t lhsn = length(lhs);
  r_size_t rhsn = length(rhs);

  r_vec<r_lgl> out(n);

  if constexpr (RListVector<common_t>){
    for (r_size_t i = 0, li = 0, ri = 0; i < n;
      recycle_index(li, lhsn),
      recycle_index(ri, rhsn), ++i){
      out.set(i, r_lgl(identical(lhs.view(li), rhs.view(ri))));
    }
  } else {
    for (r_size_t i = 0, li = 0, ri = 0; i < n;
      recycle_index(li, lhsn),
      recycle_index(ri, rhsn), ++i){
      out.set(i, lhs.view(li) == rhs.view(ri));
    }
  }
  return out;
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
  requires (const T& a, const U& b) { a == b; } &&
  is<decltype(std::declval<const T&>() == std::declval<const U&>()), r_vec<r_lgl>>
)
inline r_vec<r_lgl> operator!=(const T& lhs, const U& rhs) {
  return !(lhs == rhs);
}

}

#endif
