#ifndef CPPALLY_R_UNIQUE_H
#define CPPALLY_R_UNIQUE_H

#include <cppally/r_length.h>
#include <cppally/r_factor.h>
#include <cppally/group/groups.h>
#include <cppally/sugar/r_vec_ops.h>
#include <cppally/sugar/r_subset.h>

namespace cppally {

template <typename T>
requires requires (const T& vec) { make_groups(vec); }
T unique(const T& x, bool sort = false) {
    groups group_info = make_groups(x, sort);
    if (group_info.n_groups == length(x)){
      return x;
    } else {
      return subset(x, group_info.starts(), false, false);
    }
}

template <typename T>
requires requires (const T& vec) { make_groups(vec); }
r_vec<r_lgl> duplicated(const T& x, bool all = false){
  
  groups g = make_groups(x);

  if (all){
    return subset(g.counts() > r_int(1), g.ids, /*invert=*/ false, /*check=*/ false);
  } else {
    r_vec<r_lgl> out(length(x), r_true);
    auto starts = g.starts();
    r_size_t n_groups = g.n_groups;

    // out[starts] = r_false
    for (r_size_t i = 0; i < n_groups; ++i){
      out.set(static_cast<r_size_t>(unwrap(starts.get(i))), r_false);
    }
    return out;
  }

}

template <RVector T>
r_factors::r_factors(const T& x) : r_factors(x, unique(x)) {}

// Helper to calculate n unique values - can be useful for various algorithms
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
    internal::r_hash_fn<data_t>,
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
