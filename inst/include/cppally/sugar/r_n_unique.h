#ifndef CPPALLY_R_N_UNIQUE_H
#define CPPALLY_R_N_UNIQUE_H

#include <cppally/r_setup.h>
#include <cppally/r_concepts.h>
#include <cppally/r_factor.h>
#include <cppally/sugar/r_groups.h>
#include <cppally/sugar/r_dense_int_map.h>
#include <ankerl/unordered_dense.h> // Hash maps for group IDs + unique + match

namespace cppally {

// Useful helper to calculate n unique values - can be useful for various algorithms
template <RVector T>
inline r_size_t n_unique(const T& x) {

  using data_t = typename T::data_type;

  r_size_t n = x.length();

  auto* RESTRICT p_x = x.data();

  // Try the dense int table first (For int with small range)
  if constexpr (is<data_t, r_int>) {

    r_size_t n_unq = 0;

    bool done = internal::try_dense_int_map(x, 0, [&, p_x](auto&& try_emplace, auto&&) {
      for (r_size_t i = 0; i < n; ++i) {
        n_unq += try_emplace(p_x[i], 1).second;
      }
    });

    if (done) return n_unq;
  }

  // Hash map for O(n) de-duplication
  // A map with a discarded int payload instead of a set: it shares its
  // instantiation with the maps in make_groups()/match()
  ankerl::unordered_dense::map<
    unwrap_t<data_t>,
    int,
    internal::r_hash<data_t>,
    internal::r_hash_eq<data_t>
  > seen;

  seen.reserve(internal::get_hash_map_reserve_size<data_t>(n));

  for (r_size_t i = 0; i < n; ++i) {
    seen.try_emplace(p_x[i], 0);
    // Since r_lgl can be either true, false or NA, we can safely return early if n_unique == 3
    if constexpr (is<data_t, r_lgl>){
      if (seen.size() == 3) return 3;
    }
  }
  return seen.size();
}

// template <RVector T>
// inline r_size_t n_unique(const T& x) {

//   using data_t = typename T::data_type;

//   r_size_t n = x.length();

//   auto* RESTRICT p_x = x.data();

//   return internal::with_value_map<int>(x, 0, [&](auto&& try_emplace, auto&&) -> r_size_t {
//     r_size_t n_unq = 0;
//     for (r_size_t i = 0; i < n; ++i) {
//       n_unq += try_emplace(p_x[i], 1).second;
//       // Since r_lgl can be either true, false or NA, we can safely return early if n_unique == 3
//       if constexpr (is<data_t, r_lgl>){
//         if (n_unq == 3) return 3;
//       }
//     }
//     return n_unq;
//   });
// }

inline r_size_t n_unique(const r_factors& x) {
  return n_unique(x.value);
}

inline r_size_t n_unique(const r_df& x) {
  return make_groups(x, false).n_groups;
}

inline r_size_t n_unique(const r_sexp& x);

}

#endif
