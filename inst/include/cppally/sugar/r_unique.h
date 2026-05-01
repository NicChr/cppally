#ifndef CPPALLY_R_UNIQUE_H
#define CPPALLY_R_UNIQUE_H

#include <cppally/r_vec_methods.h>
#include <cppally/sugar/r_sort.h>
#include <cppally/sugar/r_groups.h>
#include <cppally/sugar/r_subset.h>

namespace cppally {

template <typename T>
requires (RComposite<T> || RSexpType<T>)
T unique(const T& x, bool sort = false) {
    groups group_info = make_groups(x, sort);
    auto starts = group_info.starts();
    return subset(x, starts);
}

template <RVector T>
r_vec<r_lgl> duplicated(const T& x, bool all = false){
  groups g = make_groups(x);
  if (all){
    r_vec<r_int> sizes = g.counts();
    r_vec<r_lgl> is_dup = sizes > r_int(1);
    return is_dup.subset(g.ids, false);
  } else {
    r_vec<r_lgl> out(x.length(), r_true);
    out.fill(g.starts(), r_vec<r_lgl>(1, r_false));
    return out;
  }
}

inline r_vec<r_lgl> duplicated(const r_factors& x, bool all = false){
    return duplicated(x.value);
}

inline r_vec<r_lgl> duplicated(const r_df& x, bool all = false){
    return duplicated(make_groups(x, false).ids, all);
}

inline r_vec<r_lgl> duplicated(const r_sexp& x, bool all = false){
    return CPPALLY_VIEW_AND_APPLY(x, /*return_type = */ r_vec<r_lgl>, /*fn = */ duplicated, /*rest of args = */ all);
}

template <RVal T>
r_factors::r_factors(const r_vec<T>& x) : r_factors(x, unique(x)) {}

}

#endif
