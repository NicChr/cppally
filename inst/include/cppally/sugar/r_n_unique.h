#ifndef CPPALLY_R_N_UNIQUE_H
#define CPPALLY_R_N_UNIQUE_H

#include <cppally/r_setup.h>
#include <cppally/r_concepts.h>
#include <cppally/r_factor.h>
#include <ankerl/unordered_dense.h> // Hash maps for group IDs + unique + match

namespace cppally {

// Useful helper to calculate n unique values - can be useful for various algorithms
template <RVal T>
inline r_size_t n_unique(const r_vec<T>& x) {
    
    r_size_t n = x.length();
  
    // Hash set for O(n) de-duplication
    ankerl::unordered_dense::set<
      unwrap_t<T>, 
      internal::r_hash<T>, 
      internal::r_hash_eq<T>
    > seen;

    seen.reserve(internal::get_hash_map_reserve_size<T>(n));

    auto* RESTRICT p_x = x.data(); 
  
    for (r_size_t i = 0; i < n; ++i) {
      seen.insert(p_x[i]);
      // Since r_lgl can be either true, false or NA, we can safely return early if n_unique == 3
      if constexpr (is<T, r_lgl>){
        if (seen.size() == 3) return 3;
      }
    }
    return seen.size();
}

}

#endif
