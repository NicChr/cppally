#ifndef CPPALLY_R_UNIQUE_H
#define CPPALLY_R_UNIQUE_H

#include <cppally/sugar/r_vec_methods.h>
#include <cppally/sugar/r_sort.h>
#include <cppally/sugar/r_groups.h>
#include <cppally/sugar/r_subset.h>

namespace cppally {

namespace internal {

template <RVector T>
T unsorted_unique(const T& x) {
    auto group_info = make_groups(x);
    auto starts = group_info.starts();
    return x.subset(starts);
}

template <RVector T>
T sorted_unique(const T& x) {
    if constexpr (RSortableType<T>){
        groups group_info = make_groups(x, true);
        auto starts = group_info.starts();
        return x.subset(starts);
    } else {
        return unsorted_unique(x); 
    }
}

}

template <RVector T>
T unique(const T& x, bool sort = false) {
    if (sort){
        return internal::sorted_unique(x);
    } else {
        return internal::unsorted_unique(x);
    }
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

template <RVal T>
r_factors::r_factors(const r_vec<T>& x) : r_factors(x, unique(x)) {}

}

#endif
