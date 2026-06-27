#ifndef CPPALLY_R_REPLACE_AT_H
#define CPPALLY_R_REPLACE_AT_H

#include <cppally/r_vec.h>
#include <cppally/r_coerce.h>
#include <cppally/sugar/r_subset.h>

namespace cppally {

// Replace values at specific locations
// Equivalent to R's x[where] <- with
// COPY-ON-MODIFY is enforced when CPPALLY_COPY_ON_MODIFY flag is defined
template <RVal T, internal::RSubscript U>
void replace_at(r_vec<T>& x, const r_vec<U>& where, const r_vec<T>& with) {

  if (x.is_null()) return;

  r_size_t with_size = with.length();
  r_size_t withi = 0;

  if (with_size == 0){
    return;
  }

  if (x.is_long()){
    // Clean where vector
    r_vec<r_int64> where_clean = internal::clean_locs<r_int64>(where, x);
    r_size_t where_size = where_clean.length();
  
    for (r_size_t i = 0; i < where_size; recycle_index(withi, with_size), ++i){
      x.set(unwrap(where_clean.get(i)), with.get(withi));
    }
  } else {
    // Clean where vector
    r_vec<r_int> where_clean = internal::clean_locs<r_int>(where, x);
    r_size_t where_size = where_clean.length();
  
    for (r_size_t i = 0; i < where_size; recycle_index(withi, with_size), ++i){
      x.set(unwrap(where_clean.get(i)), with.get(withi));
    }
  }
}

}

#endif
