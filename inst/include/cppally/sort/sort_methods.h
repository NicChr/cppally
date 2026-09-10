#ifndef CPPALLY_SORT_METHODS_H
#define CPPALLY_SORT_METHODS_H

// A header for methods that rely on sort.h

#include <cppally/sort/sort.h>
#include <cppally/unique/unique.h>
#include <cppally/group/groups.h>

namespace cppally {

template <RVector T>
T unique(const T& x, bool sort) {

    T out = unique(x);

    if constexpr (RSortableVector<T>) {
        if (sort) {
            // std::move out so sort() can sort it in-place
            return cppally::sort(std::move(out));
        }
    }
    return out;
}

inline r_factors unique(const r_factors& x, bool sort) {
    return r_factors(unique(x.value, sort), x.levels(), false);
}

template <RVector T>
inline groups make_groups(const T& x, bool ordered) {
    if constexpr (RSortableType<typename T::data_type>){
        if (ordered){
            if (x.is_long()) [[unlikely]] {
                abort("Cannot group a long-vector");
            }
            return internal::make_groups_from_order(x, order(x, /*preserve_ties = */ false));
        }
    }
    return make_groups(x);
}

inline groups make_groups(const r_factors& x, bool ordered) {
    return make_groups(x.value, ordered);
}

}

#endif
