#ifndef CPP20_R_UNIQUE_H
#define CPP20_R_UNIQUE_H

#include <cpp20/internal/r_vec_math.h>
#include <cpp20/internal/r_groups.h>

namespace cpp20 {

template <RVal T>
r_vec<T> unique(const r_vec<T>& x) {
    auto group_info = make_groups(x);
    auto starts = group_starts(group_info);
    starts += 1;
    return x.subset(starts);
}

}

#endif
