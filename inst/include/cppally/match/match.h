#ifndef CPPALLY_R_MATCH_H
#define CPPALLY_R_MATCH_H

#include <cppally/coerce.h>
#include <cppally/hash/hash.h>
#include <cppally/stats/stats.h>
#include <cppally/group/dense_int_map.h>
#include <cppally/functional/pmap.h>
#include <ankerl/unordered_dense.h> // Hash maps for group IDs + unique + match
#include <vector>

namespace cppally {

// match locations
template <internal::RNumericSubscript U = r_int, RVal T>
r_vec<U> match(const r_vec<T>& needles, const r_vec<T>& haystack, U no_match = na<U>()) {

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
        out.set(0, U(static_cast<unwrap_t<U>>(i)));
        return out;
      }
    }
    out.set(0, no_match);
    return out;
  }

  auto* RESTRICT p_needles = needles.data();
  auto* RESTRICT p_haystack = haystack.data();
  auto* RESTRICT p_out = out.data();

  // Try the dense int table first (small-range int haystack)
  if constexpr (is<U, r_int>) {
    bool done = internal::try_dense_int_map(haystack, -1, [&, p_needles, p_haystack, p_out](auto&& try_emplace, auto&& find_or) {

      // Build table: first occurrence wins
      for (r_size_t i = 0; i < n_haystack; ++i) {
        try_emplace(p_haystack[i], static_cast<int>(i));
      }
      // Match needles (NA needles match the first NA in the haystack)
      for (r_size_t i = 0; i < n_needles; ++i) {
        p_out[i] = find_or(p_needles[i], unwrap(no_match));
      }
    });
    if (done) return out;
  }

  // Build hash table
  ankerl::unordered_dense::map<key_t, int_t, internal::r_hash_fn<T>, internal::r_hash_eq<T>> lookup;
  lookup.reserve(internal::get_hash_map_reserve_size<r_vec<T>>(p_haystack, n_haystack));

  for (r_size_t i = 0; i < n_haystack; ++i) {
    lookup.try_emplace(p_haystack[i], int_t(i));
  }

  // Match needles
  for (r_size_t i = 0; i < n_needles; ++i) {
    auto it = lookup.find(p_needles[i]);
    p_out[i] = (it != lookup.end() ? it->second : unwrap(no_match));
  }

  return out;
}

inline r_vec<r_int> match(const r_factors& needles, const r_factors& haystack, r_int no_match = na<r_int>()) {
  if (identical(needles.levels(), haystack.levels())){
    return match(needles.value, haystack.value, no_match);
  } else {

    // Can't use `refactor()` because invalid codes become NA which then match onto haystack's NA values. 
    // We don't want them matching so we use an always invalid unmatchable code `r_int(-1)`. 

    // Empty codes — we only need the temp's levels for lookup
    r_factors new_lvls_fct(r_vec<r_int>(), haystack.levels());

    // For each of this factor's levels, find its position in new_levels
    r_vec<r_int> remap = pmap(
      /*fn = */ [&new_lvls_fct](const auto& lvl){
        return new_lvls_fct.get_code(lvl, /*no_match*/ r_int(-1));
      },
      needles.levels()
    );

    r_size_t n = needles.length();
    r_vec<r_int> remapped_needles(n);

    for (r_size_t i = 0; i < n; ++i){
      r_int c = needles.value.get(i);
      remapped_needles.set(i, is_na(c) ? na<r_int>() : remap.get(unwrap(c) - 1));
    }

    return match(remapped_needles, haystack.value, no_match);
  }
}

template <RVector T>
requires (!RStringType<typename T::data_type>)
r_factors::r_factors(const T& x, const T& levels) : value(match(x, levels)){

  // Need to turn 0-indexed matches into 1-indexed
  value.apply([](r_int a) { return a + r_int(1); });

  r_size_t n = levels.length();
  r_vec<r_str_view> str_levels(n);
  for (r_size_t i = 0; i < n; ++i) {
      str_levels.set(i, as<r_str_view>(levels.view(i)));
  }
  init_factor(str_levels, false);
}

namespace internal {

struct in_tag {};

template <typename T>
requires (requires (const T& obj) { match(obj, obj); })
struct in_lhs {
  const T& needles;
};

// x IN table  expands to  x < in_tag{} > table, parsed as (x < in_tag{}) > table
template <typename T>
requires (requires (const T& obj) { match(obj, obj); })
in_lhs<T> operator<(const T& needles, in_tag) {
  return in_lhs<T>{ needles };
}

template <typename T>
requires (requires (const T& obj) { match(obj, obj); })
r_vec<r_lgl> operator>(in_lhs<T> lhs, const T& table) {
  auto matches = match(lhs.needles, table);
  return pmap_parallel_simd([](auto a) noexcept { return r_lgl(!is_na(a)); }, matches);
}

}

// Named infix operator
#define IS_IN < cppally::internal::in_tag{} >

}

#endif
