#ifndef CPPALLY_SORT_METHODS_H
#define CPPALLY_SORT_METHODS_H

// A header for methods that rely on sort.h

#include <cppally/sort/sort.h>
#include <cppally/unique/unique.h>
#include <cppally/group/groups.h>
#include <cppally/scalar/r_limits.h>
#include <vector>

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
    
    using data_t = typename std::remove_cvref_t<T>::data_type;

    if constexpr (RSortableType<data_t>){
        if (ordered){

            r_size_t n = x.length();

            if (n > r_limits<r_int>::max()) [[unlikely]] {
                abort("Cannot group a long-vector");
            }

            uint64_t cardinality_estimate = internal::unique_count_estimate<unwrap_t<data_t>, int, internal::r_hash_fn<data_t>, internal::r_hash_eq<data_t>>(x.data(), n);

            // If cardinality is low, use unordered groups + unique + order + rank method
            // This ratio of 1/4 is based on real measurements across a variety of data and data types
            if (cardinality_estimate < (static_cast<uint64_t>(n) / 4)){

                groups g = make_groups(x, /*ordered = */ false);
                int n_groups = g.n_groups;

                r_vec<r_int> starts = g.starts(); // Group start indices
                
                T uniques(n_groups);
                for (int k = 0; k < n_groups; ++k) {
                    uniques.set(k, x.view(unwrap(starts.get(k))));
                }
                r_vec<r_int> o = order(uniques);
                const int* RESTRICT p_o = o.data();
                
                std::vector<int> rank(n_groups);
                for (int k = 0; k < n_groups; ++k) {
                    rank[p_o[k]] = k;
                }

                r_vec<r_int> ids = g.ids;
                int* RESTRICT p_id = ids.data();
                for (r_size_t i = 0; i < n; ++i) {
                    p_id[i] = rank[p_id[i]];
                }
                return groups(ids, n_groups, /*ordered=*/ true);
            } else {
                return internal::make_groups_from_order(x, order(x));
            }
        }
    }
    return make_groups(x);
}

inline groups make_groups(const r_factors& x, bool ordered) {
    return make_groups(x.value, ordered);
}

}

#endif
