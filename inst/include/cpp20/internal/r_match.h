#ifndef CPP20_R_MATCH_H
#define CPP20_R_MATCH_H

#include <cpp20/internal/r_hash.h>

namespace cpp20 {

template <RVal T>
r_vec<r_int> match(const r_vec<T>& needles, const r_vec<T>& haystack) {

  r_size_t n_needles = needles.length();
  r_size_t n_haystack = haystack.length();

  if (n_haystack > r_limits<r_int>::max()){
    abort("Cannot match to a long vector, please use a short vector");
  }

  using key_t = unwrap_t<T>;

  // Build hash table
  ankerl::unordered_dense::map<key_t, int, internal::r_hash<T>, internal::r_hash_eq<T>> lookup;
  lookup.reserve(n_haystack);

  for (r_size_t i = 0; i < n_haystack; ++i) {
    lookup.try_emplace(unwrap(haystack.view(i)), static_cast<int>(i) + 1);
  }

  // Match needles
  auto out = r_vec<r_int>(n_needles);
  for (r_size_t i = 0; i < n_needles; ++i) {
    auto it = lookup.find(unwrap(needles.view(i)));
    out.set(i, it != lookup.end() ? r_int(it->second) : na<r_int>());
  }

  return out;
}

}

#endif
