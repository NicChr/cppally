#ifndef CPP20_R_MATCH_H
#define CPP20_R_MATCH_H

#include <cpp20/sugar/r_hash.h>
#include <ankerl/unordered_dense.h> // Hash maps for group IDs + unique + match

namespace cpp20 {

// 1-indexed match locations
template <typename U = r_int, RVal T>
requires (is<U, r_int> || is<U, r_int64>)
r_vec<U> match(const r_vec<T>& needles, const r_vec<T>& haystack) {

  r_size_t n_needles = needles.length();
  r_size_t n_haystack = haystack.length();

  if constexpr (is<U, r_int>){
    if (n_haystack > r_limits<r_int>::max()){
      abort("Cannot match to a long vector, please use match<r_int64> instead");
    }
  }

  using key_t = unwrap_t<T>;
  using int_t = unwrap_t<U>;

  r_vec<U> out(n_needles);

  if (n_needles == 0){
    return out;
  }

  if (n_needles == 1){
    auto val = needles.view(0);
    for (r_size_t i = 0; i < n_haystack; ++i){
      if (identical(val, haystack.view(i))){
        out.set(0, i + 1);
        return out;
      }
    }
    out.set(0, na<U>());
    return out;
  }

  auto* RESTRICT p_needles = needles.data();
  auto* RESTRICT p_haystack = haystack.data();
  auto* RESTRICT p_out = out.data();

  // Integer optimization: use table lookup for small integer ranges
  if constexpr (is<U, r_int> && is<key_t, int>) {
    r_vec<T> rng = range(haystack, /*na_rm=*/true);
    
    int min_val = unwrap(rng.get(0));
    int max_val = unwrap(rng.get(1));
    
    bool all_nas = is_na(min_val) && is_na(max_val);
    int64_t range_span = all_nas ? 0 : static_cast<int64_t>(max_val) - static_cast<int64_t>(min_val);
    constexpr int64_t MAX_TABLE_SIZE = 20000000; 

    if (!all_nas && range_span < MAX_TABLE_SIZE) {
      // Table maps (value - min_val) -> position, init with NA
      r_vec<r_int> table(range_span + 1, na<r_int>());
      
      auto* RESTRICT p_table = table.data();
      
      // Build table: first occurrence wins
      for (r_size_t i = 0; i < n_haystack; ++i) {
        int val = p_haystack[i];
        if (!is_na(val)){
          size_t idx = static_cast<size_t>(static_cast<int64_t>(val) - min_val);
          if (is_na(p_table[idx])){
            p_table[idx] = static_cast<int>(i) + 1;
          }
        }
      }
      
      // Find first NA position in haystack (if any)
      int na_pos = na<r_int>();
      for (r_size_t i = 0; i < n_haystack; ++i) {
        if (is_na(p_haystack[i])){
          na_pos = static_cast<int>(i) + 1;
          break;
        }
      }
      
      // Match needles
      for (r_size_t i = 0; i < n_needles; ++i) {
        int val = p_needles[i];
        if (is_na(val)) {
          p_out[i] = na_pos;
        } else if (val >= min_val && val <= max_val) {
          size_t idx = static_cast<size_t>(static_cast<int64_t>(val) - min_val);
          p_out[i] = p_table[idx];
        } else {
          p_out[i] = unwrap(na<r_int>());
        }
      }
      
      return out;
    }
  }

  // Build hash table
  ankerl::unordered_dense::map<key_t, int_t, internal::r_hash<T>, internal::r_hash_eq<T>> lookup;
  lookup.reserve(internal::get_hash_map_reserve_size<T>(n_haystack));

  for (r_size_t i = 0; i < n_haystack; ++i) {
    lookup.try_emplace(p_haystack[i], int_t(i) + int_t(1));
  }

  // Match needles
  for (r_size_t i = 0; i < n_needles; ++i) {
    auto it = lookup.find(p_needles[i]);
    p_out[i] = (it != lookup.end() ? it->second : unwrap(na<U>()));
  }

  return out;
}

template <RVal T>
r_factors::r_factors(const r_vec<T>& x, const r_vec<T>& levels) : value(match(x, levels)){
  r_vec<r_str_view> str_levels;
  if constexpr (RStringType<T>) {
      str_levels = r_vec<r_str_view>(levels);
  } else {
      r_size_t n = levels.length();
      str_levels = r_vec<r_str_view>(n);
      for (r_size_t i = 0; i < n; ++i) {
          str_levels.set(i, internal::as_r<r_str_view>(levels.view(i)));
      }
  }
  init_factor(str_levels, false);
}

}

#endif
