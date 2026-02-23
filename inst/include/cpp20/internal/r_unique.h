#ifndef CPP20_R_UNIQUE_H
#define CPP20_R_UNIQUE_H

#include <cpp20/internal/r_vec_math.h>
#include <cpp20/internal/r_sort.h>
#include <cpp20/internal/r_groups.h>

namespace cpp20 {

template <RVal T>
r_vec<T> unsorted_unique(const r_vec<T>& x) {
    auto group_info = make_groups(x);
    auto starts = group_starts(group_info);
    starts += 1;
    return x.subset(starts);
}

template <RVal T>
r_vec<T> sorted_unique(const r_vec<T>& x) {
    if constexpr (RSortable<T>){
        groups group_info = make_groups(x, true);
        auto starts = group_starts(group_info);
        starts += 1;
        return x.subset(starts);
    } else {
        return unsorted_unique(x); 
    }
}

template <RVal T>
r_vec<T> unique(const r_vec<T>& x, bool sort = false) {
    if (sort){
        return sorted_unique(x);
    } else {
        return unsorted_unique(x);
    }
}

}

#endif
