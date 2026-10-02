#ifndef CPPALLY_IS_SORTED_H
#define CPPALLY_IS_SORTED_H

#include <cppally/vector/r_vector.h>

namespace cppally {

// Is vector sorted in ascending order?
// When NAs are present, x is sorted if and only if all NAs are found at the end of the vector with no non-NA values in between. This matches R's `na.last = TRUE` convention.
// Only defined for numeric vectors, as comparison is expensive for character vectors.
template <RNumericType T>
bool is_sorted(const r_vec<T>& x){

    r_size_t n = x.length();

    T prev;

    for (r_size_t i = 0; i < n; ++i) {

        T curr = x.get(i);

        if (is_na(curr)) [[unlikely]] {
            
            // Since x[i] is NA, x is sorted IFF the rest of the values are also NA
            for (r_size_t j = i + 1; j < n; ++j) {
                if (!is_na(x.get(j))){
                    return false;
                }
            }
            return true;
        }


        if (i > 0 && unwrap(curr) < unwrap(prev)){
            return false;
        }

        prev = curr;

    }

    return true;
}

}

#endif
