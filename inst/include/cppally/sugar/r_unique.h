#ifndef CPPALLY_R_UNIQUE_H
#define CPPALLY_R_UNIQUE_H

#include <cppally/r_length.h>
#include <cppally/sugar/r_vec_ops.h>
#include <cppally/sugar/r_groups.h>
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

}

#endif
