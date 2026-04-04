#ifndef CPP20_R_UNIQUE_H
#define CPP20_R_UNIQUE_H

#include <cpp20/r_vec_methods.h>
#include <cpp20/sugar/r_sort.h>
#include <cpp20/sugar/r_groups.h>
#include <cpp20/sugar/r_subset.h>

namespace cpp20 {

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

template <RVal T>
r_factors::r_factors(const r_vec<T>& x) : r_factors(x, unique(x)) {}

}

#endif
