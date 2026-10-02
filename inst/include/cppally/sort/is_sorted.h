#ifndef CPPALLY_IS_SORTED_H
#define CPPALLY_IS_SORTED_H

#include <cppally/vector/r_vector.h>

namespace cppally {

template <RNumericType T>
bool is_sorted(const r_vec<T>& x){

    r_size_t n = x.length();

    for (r_size_t i = 1; i < n; ++i) {

        if (is_na(x.get(i))){
            
            // Since x[i] is NA, x is sorted IFF the rest of the values are also NA
            for (r_size_t j = i + 1; j < n; ++j) {
                if (!is_na(x.get(j))){
                    return false;
                }
            }
        }

        bool is_increasing = (x.get(i) >= x.get(i - 1)).is_true();

        if (!is_increasing){
            return false;
        }
    }

    return true;
}

}

#endif
