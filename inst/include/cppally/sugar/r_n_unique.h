#ifndef CPPALLY_R_N_UNIQUE_H
#define CPPALLY_R_N_UNIQUE_H

#include <cppally/r_setup.h>
#include <cppally/r_concepts.h>
#include <cppally/r_factor.h>
#include <cppally/groups/groups.h>
#include <cppally/sugar/r_dense_int_map.h>
#include <ankerl/unordered_dense.h> // Hash maps for group IDs + unique + match

namespace cppally {

// Useful helper to calculate n unique values - can be useful for various algorithms
template <RVector T>
inline r_size_t n_unique(const T& x) {

  using data_t = typename T::data_type;

  r_size_t n = x.length();

  // Try the dense int table first (For int with small range)

  r_size_t n_unq = 0;

  bool done = internal::try_dense_int_map(x, 0, [&n_unq, &x, n](auto&& try_emplace, auto&&) {
    for (r_size_t i = 0; i < n; ++i) {
      n_unq += try_emplace(x.view(i), 1).second;
    }
  });

  if (done) return n_unq;

  // Hash set for O(n) de-duplication
  ankerl::unordered_dense::map<
    unwrap_t<data_t>,
    int,
    internal::r_hash<data_t>,
    internal::r_hash_eq<data_t>
  > seen;

  seen.reserve(internal::get_hash_map_reserve_size<T>(x.data(), n));

  for (r_size_t i = 0; i < n; ++i) {
    seen.try_emplace(x.view(i), 0);
  }
  return seen.size();
}

inline r_size_t n_unique(const r_factors& x) {
  return n_unique(x.value);
}

}

#endif
