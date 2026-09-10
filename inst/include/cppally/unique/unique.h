#ifndef CPPALLY_R_UNIQUE_H
#define CPPALLY_R_UNIQUE_H

#include <cppally/factor/r_factors.h>
#include <cppally/group/dense_int_map.h>
#include <cppally/hash/hash.h>
#include <cppally/vector/vector_ops.h>
#include <ankerl/unordered_dense.h> // Hash maps for unique + duplicated

namespace cppally {

template <RVector T>
T unique(const T& x) {

  using data_t = typename T::data_type;

  const uint8_t zero(0);
  const uint8_t one(1);

  r_size_t n = x.length();

  T out = x;

  uint64_t cardinality_est = internal::get_hash_map_reserve_size<T>(x.data(), n);

  // Try the dense int table first (for ints with a small range)
  std::vector<unwrap_t<data_t>> uniques;
  uniques.reserve(cardinality_est);

  bool done = internal::try_dense_int_map(x, zero, [&uniques, &x, one, n](auto&& try_emplace, auto&&) {
    for (r_size_t i = 0; i < n; ++i) {
      auto val = x.view(i);
      if (try_emplace(val, one).second) {
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
      uint8_t,
      internal::r_hash_fn<data_t>,
      internal::r_hash_eq<data_t>
    > seen;

    seen.reserve(cardinality_est);

    for (r_size_t i = 0; i < n; ++i) {
      seen.try_emplace(x.view(i), zero);
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
  return out;
}

inline r_factors unique(const r_factors& x) {
  return r_factors(unique(x.value), x.levels(), false);
}

template <RVector T>
r_vec<r_lgl> duplicated(const T& x, bool all = false){

  using data_t = typename T::data_type;

  r_size_t n = x.length();
  r_vec<r_lgl> out(n, r_false);

  bool done = internal::try_dense_int_map(x, r_size_t(-1), [&out, &x, n, all](auto&& try_emplace, auto&&) {
    for (r_size_t i = 0; i < n; ++i) {
      auto [first, inserted] = try_emplace(x.view(i), i);
      if (!inserted) {
        out.set(i, r_true);
        if (all) {
          out.set(first, r_true);
        }
      }
    }
  });

  if (!done) {

    ankerl::unordered_dense::map<
      unwrap_t<data_t>,
      r_size_t,
      internal::r_hash_fn<data_t>,
      internal::r_hash_eq<data_t>
    > seen;

    seen.reserve(internal::get_hash_map_reserve_size<T>(x.data(), n));

    for (r_size_t i = 0; i < n; ++i) {
      auto [it, inserted] = seen.try_emplace(x.view(i), i);
      if (!inserted) {
        out.set(i, r_true);
        if (all) {
          out.set(it->second, r_true);
        }
      }
    }
  }

  return out;
}

inline r_vec<r_lgl> duplicated(const r_factors& x, bool all = false){
  return duplicated(x.value, all);
}

template <RVector T>
r_factors::r_factors(const T& x) : r_factors(x, unique(x)) {}

// Helper to calculate n unique values - can be useful for various algorithms
template <RVector T>
inline r_size_t n_unique(const T& x) {

  using data_t = typename T::data_type;

  // Writing these in-line apparently prevents compiler-inlining, strange..
  const uint8_t zero(0);
  const uint8_t one(1);

  r_size_t n = x.length();

  // Try the dense int table first (For int with small range)

  r_size_t n_unq = 0;

  bool done = internal::try_dense_int_map(x, zero, [&n_unq, &x, one, n](auto&& try_emplace, auto&&) {
    for (r_size_t i = 0; i < n; ++i) {
      n_unq += try_emplace(x.view(i), one).second;
    }
  });

  if (done) return n_unq;

  ankerl::unordered_dense::map<
    unwrap_t<data_t>,
    uint8_t,
    internal::r_hash_fn<data_t>,
    internal::r_hash_eq<data_t>
  > seen;

  uint64_t cardinality_est = internal::get_hash_map_reserve_size<T>(x.data(), n);
  seen.reserve(cardinality_est);

  for (r_size_t i = 0; i < n; ++i) {
    seen.try_emplace(x.view(i), zero);
  }
  return seen.size();
}

inline r_size_t n_unique(const r_factors& x) {
  return n_unique(x.value);
}

}

#endif
