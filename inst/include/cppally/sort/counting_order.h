#ifndef CPPALLY_COUNTING_ORDER_H
#define CPPALLY_COUNTING_ORDER_H

#include <cppally/r_setup.h>
#include <cppally/utils.h> // For calc_threads
#include <cstdint>
#include <vector>

namespace cppally {

namespace internal {

namespace sort {

// Minimum average elements per (chunk, key) histogram cell for multi-threaded counting sort
inline constexpr uint64_t min_elements_per_chunk_key = 8;

// Runs body(c, start, end) over n_chunks of contiguous ranges of [0, n).
template <typename F>
inline void for_each_chunk(uint32_t n, int n_chunks, F&& body) {
    static_assert(noexcept(body(0, 0u, 0u)), "chunk bodies may run inside an OMP region and must not throw");
    if (n_chunks > 1) {
        OMP_PARALLEL_FOR(n_chunks)
        for (int c = 0; c < n_chunks; ++c) {
            const uint32_t start = static_cast<uint32_t>(static_cast<uint64_t>(n) * c / n_chunks);
            const uint32_t end = static_cast<uint32_t>(static_cast<uint64_t>(n) * (c + 1) / n_chunks);
            body(c, start, end);
        }
    } else {
        body(0, 0u, n);
    }
}

// Counts key(i) over [start, end) into 4 histograms, each of size n_keys.
// Blocks are non-overlapping, contiguous, and their ranges are: [start, n1), [n1, n2), [n2, n3), [n3, end).
// len = (end - start) / 4, n1 = start + len, n2 = n1 + len, n3 = n2 + len. The 4th block also takes the 0-3 leftover elements.
template <typename KeyFn>
inline void count_over_histograms(KeyFn key, int start, int end, uint32_t n_keys, uint32_t* RESTRICT histograms) noexcept {

    uint32_t* RESTRICT hist1 = histograms;
    uint32_t* RESTRICT hist2 = hist1 + n_keys;
    uint32_t* RESTRICT hist3 = hist2 + n_keys;
    uint32_t* RESTRICT hist4 = hist3 + n_keys;

    int len = (end - start) / 4;
    int n1 = start + len;
    int n2 = n1 + len;
    int n3 = n2 + len;

    for (int j = 0; j < len; ++j){
        ++hist1[key(start + j)];
        ++hist2[key(n1 + j)];
        ++hist3[key(n2 + j)];
        ++hist4[key(n3 + j)];
    }

    // If (end - start) is not wholly divisible by 4, then iterate through range: [n3 + len, end)
    for (int i = n3 + len; i < end; ++i){
        ++hist4[key(i)];
    }
}

// Scatters [start, end) into `out` using the 4 offset histograms of count_over_histograms() (same blocks)
template <typename KeyFn>
inline void scatter_over_histograms(KeyFn key, int start, int end, uint32_t n_keys, uint32_t* RESTRICT histograms, int* RESTRICT out) noexcept {

    uint32_t* RESTRICT hist1 = histograms;
    uint32_t* RESTRICT hist2 = hist1 + n_keys;
    uint32_t* RESTRICT hist3 = hist2 + n_keys;
    uint32_t* RESTRICT hist4 = hist3 + n_keys;

    int len = (end - start) / 4;
    int n1 = start + len;
    int n2 = n1 + len;
    int n3 = n2 + len;

    for (int j = 0; j < len; ++j){
        out[hist1[key(start + j)]++] = start + j;
        out[hist2[key(n1 + j)]++] = n1 + j;
        out[hist3[key(n2 + j)]++] = n2 + j;
        out[hist4[key(n3 + j)]++] = n3 + j;
    }

    // If (end - start) is not wholly divisible by 4, then iterate through range: [n3 + len, end)
    for (int i = n3 + len; i < end; ++i){
        out[hist4[key(i)]++] = i;
    }
}

// Stable multi-threaded counting sort order permutation
// writes [0, n) to `out` ordered by key(i) in [0, n_keys)
template <typename KeyFn>
CPPALLY_NOINLINE void counting_order(KeyFn key, int n, uint32_t n_keys, int* RESTRICT out){

    static_assert(noexcept(key(0)), "`key` must be noexcept as it may run inside an OMP region");

    int n_chunks = calc_threads(n);

    // Every chunk pays for n_keys counters, so only split when chunks have enough elements per key
    if (static_cast<uint64_t>(n_keys) * n_chunks * min_elements_per_chunk_key > static_cast<uint64_t>(n)){
        n_chunks = 1;
    }

    // 4 interleaved histograms per chunk when few keys repeat a lot
    const bool split = n_keys <= 2048u && static_cast<uint64_t>(n / n_chunks) >= 16ull * n_keys;
    const uint32_t ways = split ? 4 : 1;

    constexpr std::size_t gap = 1024 / sizeof(uint32_t); // 1KB gap to avoid false sharing
    const std::size_t stride = static_cast<std::size_t>(ways) * n_keys + gap;
    std::vector<uint32_t> histograms(static_cast<std::size_t>(n_chunks) * stride);
    uint32_t* p_hists = histograms.data();

    for_each_chunk(static_cast<uint32_t>(n), n_chunks, [&](int c, uint32_t start, uint32_t end) noexcept {
        uint32_t* RESTRICT hist = p_hists + static_cast<std::size_t>(c) * stride;
        if (split){
            count_over_histograms(key, static_cast<int>(start), static_cast<int>(end), n_keys, hist);
        } else {
            KeyFn chunk_key = key;
            for (int i = static_cast<int>(start); i < static_cast<int>(end); ++i){
                ++hist[chunk_key(i)];
            }
        }
    });

    // Counts -> first output slots. Keys outer, then chunks, then each chunk's blocks (stable)
    uint32_t total = 0;
    for (uint32_t k = 0; k < n_keys; ++k){
        for (int c = 0; c < n_chunks; ++c){
            uint32_t* cell = p_hists + static_cast<std::size_t>(c) * stride + k;
            for (uint32_t w = 0; w < ways; ++w){
                const uint32_t count = cell[w * n_keys];
                cell[w * n_keys] = total;
                total += count;
            }
        }
    }

    for_each_chunk(static_cast<uint32_t>(n), n_chunks, [&](int c, uint32_t start, uint32_t end) noexcept {
        uint32_t* RESTRICT next_slot = p_hists + static_cast<std::size_t>(c) * stride;
        int* RESTRICT chunk_out = out;
        if (split){
            scatter_over_histograms(key, static_cast<int>(start), static_cast<int>(end), n_keys, next_slot, chunk_out);
        } else {
            KeyFn chunk_key = key;
            for (int i = static_cast<int>(start); i < static_cast<int>(end); ++i){
                chunk_out[next_slot[chunk_key(i)]++] = i;
            }
        }
    });
}

}

}

}

#endif
