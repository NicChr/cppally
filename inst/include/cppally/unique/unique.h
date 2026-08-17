#ifndef CPPALLY_R_UNIQUE_H
#define CPPALLY_R_UNIQUE_H

#include <cppally/length.h>
#include <cppally/factor/r_factors.h>
#include <cppally/group/groups.h>
#include <cppally/sort/sort.h>
#include <cppally/vector/vector_ops.h>
#include <cppally/sugar/subset.h>

namespace cppally {

template <RVector T>
T unique(const T& x, bool sort = false) {

  using data_t = typename T::data_type;

  r_size_t n = x.length();

  T out = x;

  uint64_t cardinality_est = internal::get_hash_map_reserve_size<T>(x.data(), n);

  // Try the dense int table first (for ints with a small range)
  std::vector<unwrap_t<data_t>> uniques;
  uniques.reserve(cardinality_est);

  bool done = internal::try_dense_int_map(x, uint8_t(0), [&uniques, &x, n](auto&& try_emplace, auto&&) {
    for (r_size_t i = 0; i < n; ++i) {
      auto val = x.view(i);
      if (try_emplace(val, uint8_t(1)).second) {
        uniques.push_back(unwrap(val));
      }
    }
  });

  if (done) {

    r_size_t n_unq = uniques.size();

    if (n_unq < n) {
      T res(n_unq);
      for (r_size_t i = 0; i < n_unq; ++i) {
        res.set(i, internal::unsafe_reconstruct_view<data_t>(uniques[i]));
      }
      out = std::move(res);
    }

  } else {

    ankerl::unordered_dense::map<
      unwrap_t<data_t>,
      bool,
      internal::r_hash_fn<data_t>,
      internal::r_hash_eq<data_t>
    > seen;

    seen.reserve(cardinality_est);

    for (r_size_t i = 0; i < n; ++i) {
      seen.try_emplace(x.view(i), false);
    }

    r_size_t n_unq = seen.size();

    if (n_unq < n) {
      const auto& vals = seen.values();
      T res(n_unq);
      for (r_size_t i = 0; i < n_unq; ++i) {
        res.set(i, internal::unsafe_reconstruct_view<data_t>(vals[i].first));
      }
      out = std::move(res);
    }
  }

  if constexpr (RSortableType<data_t>) {
    if (sort) {
      // std::move out so sort() sorts it in-place
      return cppally::sort(std::move(out));
    }
  }
  return out;
}

inline r_factors unique(const r_factors& x, bool sort = false) {
  return r_factors(unique(x.value, sort), x.levels(), false);
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

  bool done = internal::try_dense_int_map(x, uint8_t(0), [&n_unq, &x, n](auto&& try_emplace, auto&&) {
    for (r_size_t i = 0; i < n; ++i) {
      n_unq += try_emplace(x.view(i), uint8_t(1)).second;
    }
  });

  if (done) return n_unq;

  // Hash set for O(n) de-duplication
  ankerl::unordered_dense::map<
    unwrap_t<data_t>,
    bool,
    internal::r_hash_fn<data_t>,
    internal::r_hash_eq<data_t>
  > seen;

  uint64_t cardinality_est = internal::get_hash_map_reserve_size<T>(x.data(), n);
  seen.reserve(cardinality_est);

  for (r_size_t i = 0; i < n; ++i) {
    seen.try_emplace(x.view(i), false);
  }
  return seen.size();
}

inline r_size_t n_unique(const r_factors& x) {
  return n_unique(x.value);
}

}

#endif
