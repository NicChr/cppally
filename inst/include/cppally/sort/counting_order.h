#ifndef CPPALLY_COUNTING_ORDER_H
#define CPPALLY_COUNTING_ORDER_H

#include <cppally/r_setup.h>
#include <cstdint>
#include <vector>

namespace cppally {

namespace internal {

// Counts key(i) over [0, n) into 4 histograms, each of size n_keys (except for the 4th block which can be 1-3 elements longer).
// Blocks are non-overlapping, contiguous, and their ranges are: [0, n1), [n1, n2), [n2, n3), [n3, n).
// n1 = n / 4, n2 = n1 * 2, n3 = n1 * 3.
template <typename KeyFn>
inline void count_over_histograms(KeyFn key, int n, uint32_t n_keys, uint32_t* RESTRICT histograms) noexcept {
    
    uint32_t* RESTRICT hist1 = histograms;
    uint32_t* RESTRICT hist2 = hist1 + n_keys;
    uint32_t* RESTRICT hist3 = hist2 + n_keys;
    uint32_t* RESTRICT hist4 = hist3 + n_keys;
    
    // Implied that n0 = 0 but we don't write it out since 0 + j = j
    int n1 = n / 4;
    int n2 = n1 * 2;
    int n3 = n1 * 3;

    for (int j = 0; j < n1; ++j){
        ++hist1[key(j)];
        ++hist2[key(n1 + j)];
        ++hist3[key(n2 + j)];
        ++hist4[key(n3 + j)];
    }

    // If n is not wholly divisible by 4, then iterate through range: [floor(n / 4) * 4, n)
    for (int i = 4 * n1; i < n; ++i){
        ++hist4[key(i)];
    }
}

// Counting sort order permutation (stable)
// writes [0, n) to `out` ordered by key(i) in [0, n_keys)
template <typename KeyFn>
inline void counting_order(KeyFn key, int n, uint32_t n_keys, int* RESTRICT out){

    // When n keys <= 2048 and data is sufficiently large, use histograms
    if (n_keys <= 2048u && static_cast<uint64_t>(n) >= 16ull * n_keys){

        std::vector<uint32_t> histograms(4 * static_cast<std::size_t>(n_keys), uint32_t(0));
        count_over_histograms(key, n, n_keys, histograms.data());

        uint32_t* RESTRICT hist1 = histograms.data();
        uint32_t* RESTRICT hist2 = hist1 + n_keys;
        uint32_t* RESTRICT hist3 = hist2 + n_keys;
        uint32_t* RESTRICT hist4 = hist3 + n_keys;

        uint32_t total = 0;
        for (uint32_t k = 0; k < n_keys; ++k){
            uint32_t c1 = hist1[k], c2 = hist2[k], c3 = hist3[k], c4 = hist4[k];
            hist1[k] = total; total += c1;
            hist2[k] = total; total += c2;
            hist3[k] = total; total += c3;
            hist4[k] = total; total += c4;
        }

        // Implied that n0 = 0 but we don't write it out since 0 + j = j
        int n1 = n / 4;
        int n2 = n1 * 2;
        int n3 = n1 * 3;

        for (int j = 0; j < n1; ++j){
            out[hist1[key(j)]++] = j;
            out[hist2[key(n1 + j)]++] = n1 + j;
            out[hist3[key(n2 + j)]++] = n2 + j;
            out[hist4[key(n3 + j)]++] = n3 + j;
        }
        // If n is not wholly divisible by 4, then iterate through range: [floor(n / 4) * 4, n)
        for (int i = 4 * n1; i < n; ++i){
            out[hist4[key(i)]++] = i;
        }
        return;
    }

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
