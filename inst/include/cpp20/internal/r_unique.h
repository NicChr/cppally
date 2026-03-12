#ifndef CPP20_R_UNIQUE_H
#define CPP20_R_UNIQUE_H

#include <cpp20/internal/r_vec_math.h>
#include <cpp20/internal/r_sort.h>
#include <cpp20/internal/r_groups.h>

namespace cpp20 {

namespace internal {

template <RVector T>
T unsorted_unique(const T& x) {
    auto group_info = make_groups(x);
    auto starts = group_info.starts();
    starts += 1;
    return x.subset(starts);
}

template <RVector T>
T sorted_unique(const T& x) {
    if constexpr (RSortableType<T>){
        groups group_info = make_groups(x, true);
        auto starts = group_info.starts();
        starts += 1;
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

}

#endif
