#ifndef CPPALLY_COUNTING_ORDER_H
#define CPPALLY_COUNTING_ORDER_H

#include <cppally/r_setup.h>
#include <cstdint>
#include <vector>

namespace cppally {

namespace internal {

// Counting sort order permutation (stable)
// writes [0, n) to `out` ordered by key(i) in [0, n_keys)
template <typename KeyFn>
inline void counting_order(KeyFn key, int n, uint32_t n_keys, int* RESTRICT out){

    std::vector<uint32_t> offsets(n_keys, uint32_t(0));
    for (int i = 0; i < n; ++i){
        ++offsets[key(i)];
    }

    uint32_t total = 0;
    for (uint32_t& offset : offsets){
        uint32_t key_count = offset;
        offset = total;
        total += key_count;
    }

    for (int i = 0; i < n; ++i){
        out[offsets[key(i)]++] = i;
    }
}

}

}

#endif
