#ifndef CPPALLY_R_SORT_H
#define CPPALLY_R_SORT_H

// ------- Hybrid sorting for R vectors -------
// All sorting is implemented by sorting NA values last (like `order(..., na.last = TRUE)`)
// ska_sort is used for radix sorting strings. Copyright Malte Skarupke 2016.
// Small vectors are sorted using a comparison sort via std::stable_sort.
// Large vectors of integers or doubles with no fractional part use a counting sort when the
// range is relatively small. 
// 64-bit types (int64, dates, date-times) with a wider range that still fits
// a uint32 offset radix-sort on that narrower key instead of the full 64-bit key. Everything
// else falls back to a full-width stable LSD radix sort.
// Strings are sorted by radix sorting unique strings (if cardinality is low enough) and then re-ordering.

#include <cppally/vector/r_vector.h>
#include <cppally/hash/hash.h>
#include <cppally/stats/range.h> // For range
#include <cppally/sort/is_sorted.h>
#include <cppally/sort/counting_order.h>
#include <array>
#include <cstdint> // For uint32_t and similar
#include <vector> // For C++ vectors
#include <limits>
#include <memory> // For std::unique_ptr
#include <cmath>
#include <algorithm> // For std::min
#include <ankerl/unordered_dense.h> // Hash maps for group IDs + unique + match
#include <ska_sort/ska_sort.hpp> // For radix sorting via ska_sort

namespace cppally {

namespace internal {

namespace sort {

// general order vector that sorts `x`
// NAs are ordered last
// Internal function to be used for low overhead sorting small vectors
template <RSortableVector T>
r_vec<r_int> order_cmp(const T& x) {

    int n = x.length();
    r_vec<r_int> pv(n);
    pv.iota();

    auto* RESTRICT p = pv.data();
    auto cmp = [&x](int i, int j) noexcept {
        return is_na(x.view(i)) ? false : !((x.view(i) < x.view(j)).is_false());
    };

    std::stable_sort(p, p + n, cmp);

    return pv;
}

template <typename key_t>
struct key_index {
    key_t key;
    uint32_t index;
};

// Sort key-index pairs
template <typename key_t>
inline void sort_key_index(key_index<key_t>* first, key_index<key_t>* last) {
    ska_sort::ska_sort(first, last, [](const key_index<key_t>& k) { return std::make_pair(k.key, k.index); });
}

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

// Stable multi-threaded LSD radix sort, one byte per pass, returning the original positions in key order.
// Elements are split into contiguous chunks, with each chunk owning its own histograms,
// so every chunk scatters into output slots no other chunk writes to.
// NAs must be mapped to max key.
// Note: `keys` data is overwritten.
template <typename key_t>
inline r_vec<r_int> order_radix(key_t* keys, uint32_t n) {

    static_assert(std::is_unsigned_v<key_t>, "radix keys must be unsigned");

    constexpr int n_bytes = sizeof(key_t);
    int n_chunks = calc_threads(n);

    r_vec<r_int> out(static_cast<r_size_t>(n));
    int* p_out = out.data();

    if (n == 0) {
        return out;
    }

    // Each chunk's histograms
    constexpr std::size_t gap = 1024 / sizeof(uint32_t); // 1KB gap to avoid false sharing
    constexpr std::size_t stride = n_bytes * 256 + gap;
    std::vector<uint32_t> histograms(static_cast<std::size_t>(n_chunks) * stride);
    std::unique_ptr<key_t[]> keys_buf(new key_t[n]);
    std::unique_ptr<int[]> idx_buf(new int[n]);

    // Chunk c's histogram of byte b
    uint32_t* p_hists = histograms.data();
    auto hist_of = [p_hists](int c, int b) noexcept {
        return p_hists + static_cast<std::size_t>(c) * stride + b * 256;
    };

    // Each byte is a base-256 digit of the key, with byte 0 being least significant.
    // Count how many keys have each digit (0-255) at each byte position
    for_each_chunk(n, n_chunks, [&](int c, uint32_t start, uint32_t end) noexcept {
        uint32_t* RESTRICT hist = hist_of(c, 0);
        const key_t* RESTRICT chunk_keys = keys;
        for (uint32_t i = start; i < end; ++i) {
            const key_t k = chunk_keys[i];
            for (int b = 0; b < n_bytes; ++b) {
                // Shift byte to lowest position and keep 8 bits
                const uint8_t byte = static_cast<uint8_t>(k >> (8 * b));
                ++hist[b * 256 + byte];
            }
        }
    });

    // Avoid pass for bytes that are shared by every key, because it can't change the order.
    // If key 0's digit is held by all n keys, every key shares the same value at this byte
    int passes[n_bytes];
    int n_passes = 0;
    for (int b = 0; b < n_bytes; ++b) {
        const uint8_t first_byte = static_cast<uint8_t>(keys[0] >> (8 * b));
        uint32_t first_byte_count = 0;
        for (int c = 0; c < n_chunks; ++c) {
            first_byte_count += hist_of(c, b)[first_byte];
        }
        if (first_byte_count != n) {
            passes[n_passes++] = b;
        }
    }

    // Original positions 0, ..., n - 1, which move with their keys
    out.iota();

    key_t* k_src = keys;
    key_t* k_dst = keys_buf.get();
    int* i_src = p_out;
    int* i_dst = idx_buf.get();

    // Stable counting sort per byte, least significant first.
    // Keys with same byte keep their order from previous iteration, which already sorted them by less significant bytes
    for (int p = 0; p < n_passes; ++p) {
        const int b = passes[p];

        // Chunk counts depend on element order, so later passes re-count.
        // A single chunk's counts are the totals (unchanging).
        if (p > 0 && n_chunks > 1) {
            for_each_chunk(n, n_chunks, [&](int c, uint32_t start, uint32_t end) noexcept {
                uint32_t* RESTRICT hist = hist_of(c, b);
                const key_t* RESTRICT chunk_keys = k_src;
                const int shift = 8 * b;
                std::fill(hist, hist + 256, 0u);
                for (uint32_t i = start; i < end; ++i) {
                    ++hist[static_cast<uint8_t>(chunk_keys[i] >> shift)];
                }
            });
        }

        // Counts -> buckets' first output slot.
        // Digits outer, chunks inner: chunk c's keys with digit d land after chunk c - 1's
        uint32_t total = 0;
        for (int d = 0; d < 256; ++d) {
            for (int c = 0; c < n_chunks; ++c) {
                uint32_t& slot = hist_of(c, b)[d];
                const uint32_t count = slot;
                slot = total;
                total += count;
            }
        }

        for_each_chunk(n, n_chunks, [&](int c, uint32_t start, uint32_t end) noexcept {
            uint32_t* RESTRICT next_slot = hist_of(c, b);
            const key_t* RESTRICT chunk_k_src = k_src;
            key_t* RESTRICT chunk_k_dst = k_dst;
            const int* RESTRICT chunk_i_src = i_src;
            int* RESTRICT chunk_i_dst = i_dst;
            const int shift = 8 * b;
            for (uint32_t i = start; i < end; ++i) {
                const key_t k = chunk_k_src[i];
                const uint8_t byte = static_cast<uint8_t>(k >> shift);
                const uint32_t pos = next_slot[byte]++;
                chunk_k_dst[pos] = k;
                chunk_i_dst[pos] = chunk_i_src[i];
            }
        });

        // Current output becomes next iteration's input
        std::swap(k_src, k_dst);
        std::swap(i_src, i_dst);
    }

    // Each iteration flips between out and idx_buf
    // after an odd number of passes, the result is in idx_buf
    if (i_src != p_out) {
        const int* result = i_src;
        for_each_chunk(n, n_chunks, [&](int, uint32_t start, uint32_t end) noexcept {
            std::copy(result + start, result + end, p_out + start);
        });
    }
    return out;
}

// struct key_of {
//     template <typename key_t>
//     key_t operator()(const key_index<key_t>& k) const noexcept { return k.key; }
// };

// Stable LSD radix sort using ska_sort internals
// template <typename key_t>
// inline r_vec<r_int> order_radix(std::vector<key_index<key_t>>& pairs) {

//     uint32_t n = static_cast<uint32_t>(pairs.size());

//     // Where the sorted result ends up; usually `pairs`, but the LSD sort may
//     // leave it in the scratch buffer.
//     key_index<key_t>* RESTRICT src = pairs.data();
//     std::vector<key_index<key_t>> buffer(n);

//     // Use LSD copy sort as it's stable by default.
//     bool in_buffer = ska_sort::detail::RadixSorter<key_t>::sort(src, src + n, buffer.data(), key_of{});
//     if (in_buffer) {
//         src = buffer.data();
//     }

//     r_vec<r_int> out(static_cast<r_size_t>(n));
//     int* RESTRICT p_out = out.data();

//     OMP_PARALLEL_FOR_SIMD(calc_threads(n))
//     for (uint32_t i = 0; i < n; ++i) {
//         p_out[i] = static_cast<int>(src[i].index);
//     }
//     return out;
// }

template <CppNumber T>
inline r_vec<r_int> order_numeric_data(const T* RESTRICT p_x, uint32_t n, T lo, T hi) {

    // All NA means order is [0, n)
    if (is_na(lo) || is_na(hi)) {
        r_vec<r_int> out(static_cast<r_size_t>(n));
        out.iota();
        return out;
    }

    using radix_key_t = decltype(ska_sort::detail::to_unsigned_or_bool(std::declval<T>()));
    constexpr bool wide_key = sizeof(radix_key_t) > sizeof(int);
    constexpr uint32_t NA_KEY32 = std::numeric_limits<uint32_t>::max();

    // ---- Can every value be keyed as a whole-number uint32 offset from lo? ----

    uint64_t span = 0;
    bool offsets_fit = false;

    if constexpr (CppIntegerType<T>) {
        span = static_cast<uint64_t>(hi) - static_cast<uint64_t>(lo);
        offsets_fit = span < NA_KEY32;
    } else if constexpr (CppFloatType<T>) {
        // Above 2^digits, not every whole number is representable
        constexpr T EXACT_LIMIT = static_cast<T>(uint64_t(1) << (std::numeric_limits<T>::digits - 1)) * 2;
        double dspan = static_cast<double>(hi) - static_cast<double>(lo);
        offsets_fit = dspan < NA_KEY32 && lo >= -EXACT_LIMIT && hi <= EXACT_LIMIT;

        for (uint32_t i = 0; offsets_fit && i < n; ++i) {
            const T v = p_x[i];
            const double offset = v - lo;
            offsets_fit = is_na(v) || static_cast<double>(static_cast<int64_t>(offset)) == offset;
        }

        if (offsets_fit) {
            span = static_cast<uint64_t>(dspan);
        }
    }

    // ---- Counting sort: small span, offsets index straight into counts ----

    // Counts capped at 32MB, int-sized offsets, and a multiple of n
    constexpr uint64_t MAX_COUNTS = std::min<uint64_t>((32ull << 20) / sizeof(uint32_t), std::numeric_limits<int>::max());
    constexpr uint64_t COUNTS_PER_ELEMENT = wide_key ? 16 : 4;
    const uint64_t counts_cap = std::min(MAX_COUNTS, static_cast<uint64_t>(n) * COUNTS_PER_ELEMENT);

    if (offsets_fit && span < counts_cap) {
        uint32_t na_key = static_cast<uint32_t>(span) + 1; // NAs go in the bucket after the last offset
        r_vec<r_int> out(static_cast<r_size_t>(n));
        counting_order(
            [p_x, lo, na_key](int i) noexcept {
                T v = p_x[i];
                uint32_t key = is_na(v) ? na_key : static_cast<uint32_t>(v - lo);
                return key;
            },
            static_cast<int>(n), na_key + 1, out.data()
        );
        return out;
    }

    // ---- Narrow radix: uint32 offset keys instead of the full-width key ----

    if (offsets_fit && (wide_key || CppFloatType<T>)) {
        std::unique_ptr<uint32_t[]> keys(new uint32_t[n]);
        uint32_t* p_keys = keys.get();
        for_each_chunk(n, calc_threads(n), [&](int, uint32_t start, uint32_t end) noexcept {
            const T* RESTRICT chunk_x = p_x;
            uint32_t* RESTRICT chunk_keys = p_keys;
            const T chunk_lo = lo;
            OMP_SIMD
            for (uint32_t i = start; i < end; ++i) {
                const T v = chunk_x[i];
                chunk_keys[i] = is_na(v) ? NA_KEY32 : static_cast<uint32_t>(v - chunk_lo);
            }
        });
        return order_radix(p_keys, n);
    }

    // ---- Full-width radix ----

    // NA owns the type minimum, so shifting real keys down by 1 keeps the max below the NA key
    constexpr bool shift_below_na = CppIntegerType<T> && is<T, unwrap_t<as_r_scalar_t<T>>> &&
        unwrap(na<as_r_scalar_t<T>>()) == std::numeric_limits<T>::min();
    constexpr radix_key_t NA_KEY = std::numeric_limits<radix_key_t>::max();

    std::unique_ptr<radix_key_t[]> keys(new radix_key_t[n]);
    radix_key_t* p_keys = keys.get();
    for_each_chunk(n, calc_threads(n), [&](int, uint32_t start, uint32_t end) noexcept {
        const T* RESTRICT chunk_x = p_x;
        radix_key_t* RESTRICT chunk_keys = p_keys;
        OMP_SIMD
        for (uint32_t i = start; i < end; ++i) {
            radix_key_t radix_key = NA_KEY;
            if (!is_na(chunk_x[i])) {
                T v = chunk_x[i] + T(0); // + 0 normalises -0.0 to 0.0
                radix_key = ska_sort::detail::to_unsigned_or_bool(v);
                if constexpr (shift_below_na) {
                    radix_key -= 1u;
                }
            }
            chunk_keys[i] = radix_key;
        }
    });
    return order_radix(p_keys, n);
}

// Pack 8 bytes of string into uint64_t (pad zeros at the end)
inline uint64_t string_radix_key(const char* s, uint32_t depth) noexcept {
    s += depth;
    uint64_t k = 0;
    for (int b = 0; b < 8 && s[b]; ++b) {
        k |= static_cast<uint64_t>(static_cast<unsigned char>(s[b])) << (56 - 8 * b);
    }
    return k;
}

// Sorts radix key-index pairs, caller has to have built radix key from string (first 8 bytes only)
// Index is used for ties to keep sort stable
inline void sort_string_keys(const r_str_view* strs, std::vector<key_index<uint64_t>>& keys) {

    uint32_t n = keys.size();

    if (n < 2) {
        return;
    }

    // Ties are re-keyed 8 bytes deeper
    struct run {
        uint32_t begin;
        uint32_t end;
        uint32_t depth; 
    };

    std::vector<run> runs = { {0, n, 0} };

    while (!runs.empty()) {
        run r = runs.back();
        runs.pop_back();

        key_index<uint64_t>* first = keys.data() + r.begin;
        key_index<uint64_t>* last = keys.data() + r.end;

        if (r.depth > 0) {
            for (auto* p = first; p != last; ++p) {
                p->key = string_radix_key(strs[p->index].c_str(), r.depth);
            }
        }

        // A shared prefix leaves the whole run on one window, nothing to sort at this depth
        bool all_equal = std::all_of(first, last, [w = first->key](const key_index<uint64_t>& k) { return k.key == w; });
        if (!all_equal) {
            sort_key_index(first, last);
        }

        for (uint32_t j = r.begin; j < r.end;) {
            uint32_t k = j + 1;
            while (k < r.end && keys[k].key == keys[j].key) {
                ++k;
            }
            // Non-zero last byte means the strings continue past this window
            if (k - j > 1 && (keys[j].key & 0xff) != 0) {
                runs.push_back({j, k, r.depth + 8});
            }
            j = k;
        }
    }
}

// Ids [0, n - 1] of the unique strings, in C-locale order
inline std::vector<uint32_t> sort_unique_strings(const std::vector<r_str_view>& uniques) {

    uint32_t n = uniques.size();
    std::vector<uint32_t> ids(n);

    std::vector<key_index<uint64_t>> keys(n);
    for (uint32_t id = 0; id < n; ++id) {
        keys[id] = { string_radix_key(uniques[id].c_str(), 0), id };
    }

    sort_string_keys(uniques.data(), keys);

    for (uint32_t j = 0; j < n; ++j) {
        ids[j] = keys[j].index;
    }
    return ids;
}

// High-cardinality string vector: skip deduplication and scatter
inline void order_strings_direct(const r_str_view* strs, uint32_t n, int* RESTRICT p_out) {

    std::vector<key_index<uint64_t>> keys;
    keys.reserve(n);
    for (uint32_t i = 0; i < n; ++i) {
        if (!strs[i].is_na()) {
            keys.push_back({ string_radix_key(strs[i].c_str(), 0), i });
        }
    }

    sort_string_keys(strs, keys);

    uint32_t n_real = keys.size();
    for (uint32_t j = 0; j < n_real; ++j) {
        p_out[j] = static_cast<int>(keys[j].index);
    }

    // NAs last, in element order
    uint32_t j = n_real;
    for (uint32_t i = 0; i < n && j < n; ++i) {
        if (strs[i].is_na()) {
            p_out[j++] = static_cast<int>(i);
        }
    }
}

// In-place sort
template <typename T>
requires requires (const T& v, r_size_t i) { v.get(i);}
void sort_in_place(T& x, const r_vec<r_int>& order){

    int n = static_cast<int>(x.length());

    if (n != order.length()) [[unlikely]] {
        abort("`sort_in_place()`: `x` and `order` must have the same length");
    }

    using data_t = typename std::remove_cvref_t<T>::data_type;
    std::vector<unwrap_t<data_t>> buf;
    buf.reserve(n);
    for (int i = 0; i < n; ++i){
        buf.push_back(unwrap(x.view(i)));
    }

    const int* RESTRICT o = order.data();
    for (int i = 0; i < n; ++i){
        x.set(i, internal::unsafe_reconstruct_view<data_t>(buf[o[i]]));
    }
}

// Inverse of sort_in_place
// assuming x is already sorted by `order`, this restores x in its original order
template <typename T>
requires requires (const T& v, r_size_t i) { v.get(i);}
void unsort_in_place(T& x, const r_vec<r_int>& order){

    int n = static_cast<int>(x.length());

    if (n != order.length()) [[unlikely]] {
        abort("`unsort_in_place()`: `x` and `order` must have the same length");
    }

    using data_t = typename std::remove_cvref_t<T>::data_type;
    std::vector<unwrap_t<data_t>> buf;
    buf.reserve(n);
    for (int i = 0; i < n; ++i){
        buf.push_back(unwrap(x.view(i)));
    }

    const int* RESTRICT o = order.data();
    for (int i = 0; i < n; ++i){
        x.set(o[i], internal::unsafe_reconstruct_view<data_t>(buf[i]));
    }
}

}

}

// 0-indexed ordering permutation vector that represents in sequential order, 
// the indices of `x` elements that need to be chosen to return a sorted `x`
template <RSortableVector T>
inline r_vec<r_int> order(const T& x) {

    using data_t = typename T::data_type;

    uint32_t n = x.length();
    
    if (n < 200){
        return internal::sort::order_cmp(x);
    }

    if constexpr (RNumericType<data_t>) {

        // ----------------------------------------------------------------------
        // Numeric data
        // ----------------------------------------------------------------------
    
        T rng = range(x, true);
        auto min_val = rng.get(0), max_val = rng.get(1);
    
        return internal::sort::order_numeric_data(x.data(), n, unwrap(min_val), unwrap(max_val));

    } else if constexpr (RStringType<data_t>) {

        // ----------------------------------------------------------------------
        // Strings
        // ---------------------------------------------------------------------- 
    
        r_vec<r_int> out(n);
        auto* RESTRICT px = x.data();

        uint64_t estimate = internal::unique_count_estimate<SEXP, int, internal::r_hash_fn<data_t>, internal::r_hash_eq<data_t>>(px, n);
        // High cardinality -> direct sort
        if (estimate >= n / 2) {
            static_assert(sizeof(r_str_view) == sizeof(SEXP) && std::is_standard_layout_v<r_str_view> && std::is_trivially_copyable_v<r_str_view>);
            internal::sort::order_strings_direct(reinterpret_cast<const r_str_view*>(px), n, out.data());
            return out;
        }
        // Low cardinality -> sort uniques
        auto n_uniques_guess = internal::get_hash_map_reserve_size<T>(px, n);

        // Single Hash Map to assign group IDs and count frequencies
        ankerl::unordered_dense::map<SEXP, int, internal::r_hash_fn<data_t>, internal::r_hash_eq<data_t>> lookup;
        lookup.reserve(n_uniques_guess);
        
        std::vector<r_str_view> uniques;
        uniques.reserve(n_uniques_guess);
        std::vector<uint32_t> counts;
        counts.reserve(n_uniques_guess);
        std::vector<uint32_t> group_ids; // Caches the ID for each element
        group_ids.reserve(n);
        
        uint32_t last_id = uint32_t(-1);
        
        for (uint32_t i = 0; i < n; ++i) {
            r_str_view str = r_str_view(px[i], internal::no_checks_tag{});
            
            if (str.is_na()){
                group_ids.push_back(uint32_t(-1));
            }
            // Linear Scan Cache - identical strings have identical pointers
            else if (i > 0 && unwrap(str) == px[i - 1]) { 
                group_ids.push_back(last_id);
                counts[last_id]++;
            } 
            else {
                auto [it, inserted] = lookup.try_emplace(str, static_cast<int>(uniques.size()));
                if (inserted) {
                    last_id = uniques.size();
                    uniques.push_back(str);
                    counts.push_back(1);
                } else {
                    last_id = it->second;
                    counts[last_id]++;
                }
                group_ids.push_back(last_id);
            }
        }

        uint32_t n_uniques = uniques.size();
        // Sort the unique group IDs by string content
        std::vector<uint32_t> sorted_ids = internal::sort::sort_unique_strings(uniques);

        // Prefix Sums: calculate the starting write offset for each group
        std::vector<uint32_t> offsets(n_uniques + 1);
        uint32_t current_offset = 0;

        for (uint32_t id : sorted_ids) {
            offsets[id] = current_offset;
            current_offset += counts[id];
        }

        offsets[n_uniques] = current_offset; // NA bucket after all real groups
    
        // Distribute indices (Counting Sort)
        int* RESTRICT p_out = out.data();
        for (uint32_t i = 0; i < n; ++i) {
            p_out[offsets[std::min(group_ids[i], n_uniques)]++] = i;
        }
        
        return out;
    } else {
        return internal::sort::order_cmp(x);
    }
}

inline r_vec<r_int> order(const r_factors& x) {
    return order(x.value);
}

template <typename T>
requires (requires (const T&x) { order(x); })
[[deprecated("order(): preserve_ties is ignored")]]
inline r_vec<r_int> order(const T& x, bool) {
    return order(x);
}

// Sorting

template <typename T>
requires requires (T&& v, r_size_t i) { order(v); v.get(i);}
std::remove_cvref_t<T> sort(T&& x){
    
    if constexpr (requires (T&& vec){ is_sorted(vec); }){

        if (is_sorted(x)){
            if constexpr (std::is_same_v<T, std::remove_cvref_t<T>>){
                return std::move(x);
            }
            return x;
        }
    }

    r_vec<r_int> o = order(x);

    if constexpr (std::is_same_v<T, std::remove_cvref_t<T>>){
        if (x.is_exclusive()){
            internal::sort::sort_in_place(x, std::move(o));
            return std::move(x);
        }
    }
    return pmap_parallel_simd([&](r_int a){ return x.view(unwrap(a));}, std::move(o));
}

}

#endif
