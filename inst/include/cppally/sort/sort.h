#ifndef CPPALLY_R_SORT_H
#define CPPALLY_R_SORT_H

// ------- Hybrid sorting for R vectors -------
// All sorting is implemented by sorting NA values last (like `order(..., na.last = TRUE)`)
// Small vectors are sorted using a comparison sort via std::stable_sort.
// Large vectors of integers or doubles with no fractional part use a counting sort when the range is relatively small. 
// Wider ranges that still fit a uint32 offset (int, int64, whole number doubles): radix-sort on that narrower key instead of the full 64-bit key.
// Everything else falls back to a full-width stable multi-threaded LSD radix sort.
// Strings are sorted by radix sorting unique strings (if cardinality is low enough) and then re-ordering.

// References:
// - Parallel LSD (per-chunk histograms, prefix sum, scatter): Zagha & Blelloch (1991) doi.org/10.1145/125826.126164
// - One-pass histograms and skipping shared bytes: Terdiman, "Radix Sort Revisited" (codercorner.com/RadixSortRevisited.htm, see section "Reducing the complexity")
// - Order-preserving float keys (to_radix_key): Adapted from ska_sort's to_unsigned_or_bool (Malte Skarupke, Boost Software License 1.0)
// - Insertion sort for small radix sort inputs: inspired by R's iinsert in src/main/radixsort.c

#include <cppally/vector/r_vector.h>
#include <cppally/hash/hash.h>
#include <cppally/stats/range.h> // For range
#include <cppally/sort/is_sorted.h>
#include <cppally/sort/counting_order.h>
#include <cppally/group/groups.h>
#include <cstdint> // For uint32_t and similar
#include <vector> // For C++ vectors
#include <limits>
#include <memory> // For std::unique_ptr
#include <cmath>
#include <algorithm> // For std::min
#include <bit> // For std::bit_cast

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

// Insertion sort of idx by keys (for small inputs).
template <std::unsigned_integral key_t>
inline void insertion_sort(key_t* RESTRICT keys, int* RESTRICT idx, uint32_t n) noexcept {
    for (uint32_t i = 1; i < n; ++i) {

        key_t curr_key = keys[i];
        key_t prev_key = keys[i - 1];
        int curr_index = idx[i];
        int prev_index = idx[i - 1];

        uint32_t j = i;
        while (prev_key > curr_key){
            // Shift previous key and index up one slot
            keys[j] = prev_key;
            idx[j] = prev_index;

            if (--j == 0){
                break;
            }
            prev_key = keys[j - 1];
            prev_index = idx[j - 1];
        }
        // j is now the slot such that prev_key <= curr_key (or j == 0)
        keys[j] = curr_key;
        idx[j] = curr_index;
    }
}

// Byte b of key k as a base-256 digit
// R equivalent: (k %/% 256^b) %% 256
template <std::unsigned_integral key_t>
inline uint8_t byte_of(key_t k, int b) noexcept {
    return static_cast<uint8_t>(k >> (8 * b)); // shift byte to lowest position and keep 8 bits
}

// Stable multi-threaded LSD radix sort, one byte per pass, permuting `idx` along with `keys`.
// Elements are split into contiguous chunks, with each chunk owning its own histograms,
// so every chunk scatters into output slots no other chunk writes to.
// Both keys and idx are sorted in-place (based on keys)
// struct {key, index} would be cleaner but slower for 64-bit keys due to padding
template <std::unsigned_integral key_t>
inline CPPALLY_NOINLINE void radix_sort(key_t* keys, int* idx, uint32_t n) {

    // Use insertion sort for small vectors
    if (n < 200) {
        insertion_sort(keys, idx, n);
        return;
    }

    constexpr int n_bytes = sizeof(key_t);
    int n_chunks = calc_threads(n);

    // Each chunk's histograms
    constexpr std::size_t gap = 1024 / sizeof(uint32_t); // 1KB gap to avoid false sharing
    // The gap avoids false sharing by keeping counters on separate cache lines
    constexpr std::size_t stride = n_bytes * 256 + gap;
    
    // Single vector containing all histograms
    // Each thread gets its own histogram (chunk)
    // Each histogram is composed of counts of each digit AND the gap as described above
    // E.g if key_t is uint32_t, then n_bytes is 4,
    // assuming 4 threads:
    // histogram0 (chunk0) = {byte0[256], byte1[256], byte2[256], byte3[256], gap[256]}
    // ...
    // histogram3 (chunk3) = ----------------------------------------------------------
    //
    // So for byte0[256], it holds counts for byte 0 (number of keys that have digit 0, 1, ..., 255)
    // And similarly for byte1[256], etc..
    std::vector<uint32_t> histograms(static_cast<std::size_t>(n_chunks) * stride); // Counts initialised to 0
    std::unique_ptr<key_t[]> keys_buf(new key_t[n]);
    std::unique_ptr<int[]> idx_buf(new int[n]);

    // Chunk c's histogram of byte b
    uint32_t* p_hists = histograms.data();
    auto hist_of = [p_hists](int c, int b) noexcept {
        return p_hists + static_cast<std::size_t>(c) * stride + b * 256;
    };

    // ----- Fill up histogram counts -----

    // Each byte is a base-256 digit of the key, with byte 0 being least significant.
    // Count how many keys have each digit (0-255) at each byte position
    for_each_chunk(n, n_chunks, [&](int c, uint32_t start, uint32_t end) noexcept {
        uint32_t* RESTRICT hist = hist_of(c, 0);
        const key_t* RESTRICT chunk_keys = keys;
        for (uint32_t i = start; i < end; ++i) {
            const key_t k = chunk_keys[i];
            for (int b = 0; b < n_bytes; ++b) {
                ++hist[b * 256 + byte_of(k, b)];
            }
        }
    });

    // ----- Terdiman's suggestion for reducing radix sort complexity -----
    // Terdiman, "Radix Sort Revisited" (codercorner.com/RadixSortRevisited.htm)
    // Do all keys share the same digit at byte `b`?

    // Avoid pass for bytes that are shared by every key, because it can't change the order.
    // If the first key's digit at byte `b` is held by all `n` keys, every key shares the same value at this byte
    // We can check this quickly by checking the count of byte `b` for that digit and check if it equals n keys.
    // For example: keys = {1, 10, 20} all fit in byte 0, so bytes 1-3 are 0 for each key and only byte 0 is sorted
    int passes[n_bytes];
    int n_passes = 0;
    for (int b = 0; b < n_bytes; ++b) {
        const uint8_t first_byte = byte_of(keys[0], b);
        uint32_t first_byte_count = 0;
        for (int c = 0; c < n_chunks; ++c) {
            first_byte_count += hist_of(c, b)[first_byte];
        }
        if (first_byte_count != n) {
            passes[n_passes++] = b;
        }
    }

    key_t* k_src = keys;
    key_t* k_dst = keys_buf.get();
    int* i_src = idx;
    int* i_dst = idx_buf.get();

    // ----- Counting sort per byte -----
    // Starting from least significant digit.
    // Keys with same byte keep their order from previous iteration, which already sorted them by less significant bytes
    for (int p = 0; p < n_passes; ++p) {
        const int b = passes[p];

        // Chunk counts depend on element order, so later passes re-count.
        // A single chunk's counts are the totals (unchanging).
        if (p > 0 && n_chunks > 1) {
            for_each_chunk(n, n_chunks, [&](int c, uint32_t start, uint32_t end) noexcept {
                uint32_t* RESTRICT hist = hist_of(c, b);
                const key_t* RESTRICT chunk_keys = k_src;
                const int chunk_b = b; // Local copy, so the loop doesn't reload b through the closure
                std::fill(hist, hist + 256, 0u);
                for (uint32_t i = start; i < end; ++i) {
                    ++hist[byte_of(chunk_keys[i], chunk_b)];
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
            const int chunk_b = b;
            for (uint32_t i = start; i < end; ++i) {
                const key_t k = chunk_k_src[i];
                const uint32_t pos = next_slot[byte_of(k, chunk_b)]++;
                chunk_k_dst[pos] = k;
                chunk_i_dst[pos] = chunk_i_src[i];
            }
        });

        // Current output becomes next iteration's input
        std::swap(k_src, k_dst);
        std::swap(i_src, i_dst);
    }

    // Each iteration flips between the inputs and the buffers,
    // after an odd number of passes, the result is in the buffers
    if (i_src != idx) {
        const key_t* sorted_keys = k_src;
        const int* sorted_idx = i_src;
        for_each_chunk(n, n_chunks, [&](int, uint32_t start, uint32_t end) noexcept {
            std::copy(sorted_idx + start, sorted_idx + end, idx + start);
            std::copy(sorted_keys + start, sorted_keys + end, keys + start); // Sorted keys back into keys
        });
    }
}

// Using keys, return order permutation [0, n - 1]
// NAs must be mapped to max key.
// Note: `keys` become sorted in-place
template <std::unsigned_integral key_t>
inline r_vec<r_int> order_radix(key_t* keys, uint32_t n) {
    r_vec<r_int> out(static_cast<r_size_t>(n));
    out.iota();
    radix_sort(keys, out.data(), n);
    return out;
}

// Maps a number to an unsigned key with the same ordering
template <typename T>
inline auto to_radix_key(T x) noexcept {
    if constexpr (std::unsigned_integral<T>) {
        return x;
    } else if constexpr (std::floating_point<T>) {

        // Adapted from ska_sort's:
        // std::uint64_t to_unsigned_or_bool(double f){
        //     union { double d; uint64_t u; } as_union = { f };
        //     uint64_t sign_bit = -int64_t(as_union.u >> 63);
        //     return as_union.u ^ (sign_bit | 0x8000000000000000);
        // }
        
        using key_t = std::conditional_t<sizeof(T) == 8, uint64_t, uint32_t>;
        const key_t u = std::bit_cast<key_t>(x);

        constexpr int sign_shift = 8 * sizeof(T) - 1; // Position of sign bit

        // (u >> sign_shift) - shift by sign_shift places to the right so that only sign bit survives, e.g. 0000....1 if x < 0, otherwise, 00000...0
        // negating the result then converts the 0000....1 into all ones: 111111, and leaves 0000...0 as all zeroes: 0000...0
        const key_t mask = -(u >> sign_shift);
        // (1 << sign_shift) shifts by sign_shift places to the left this time - gives 1000...0
        // the bitwise or (mask | 1000...0) - returns 1111...1 if x < 0, otherwise 1000...0
        // the (u ^ result) flips the bits of u for every 1 in result. So for x < 0, all bits are flipped, otherwise only top bit is flipped
        // So for x < 0, we always end up with 0XXX...X, otherwise, 1XXX...X
        // We flip bits so that it reverses the order of negatives, so that negatives with larger size are ordered before smaller ones
        // Rationale: we want negatives to sort first, so their top bit is now 0, which is less than 1, so the unsigned result of negatives sort before non-negatives
        return static_cast<key_t>(u ^ (mask | (key_t(1) << sign_shift)));
    } else if constexpr (std::signed_integral<T>) {
        
        // Adapted from ska_sort's:
        // unsigned int to_unsigned_or_bool(int i){
        //     return static_cast<unsigned int>(i) + static_cast<unsigned int>(1 << (sizeof(int) * 8 - 1));
        // }

        using key_t = std::make_unsigned_t<T>;
        const key_t u = static_cast<key_t>(x);

        constexpr int sign_shift = 8 * sizeof(T) - 1;

        // Same equation as in floating-point branch with mask = 0
        // Two's complement ensures negatives will order correctly, so only flip the sign bit
        return static_cast<key_t>(u ^ (key_t(1) << sign_shift));
    } else {
        static_assert(always_false<T>, "Don't know how to convert this type to a radix key");
    }
}

template <CppNumber T>
inline CPPALLY_NOINLINE r_vec<r_int> order_numeric_data(const T* RESTRICT p_x, uint32_t n, T lo, T hi) {

    using radix_key_t = decltype(to_radix_key(std::declval<T>()));
    constexpr uint32_t NA_KEY32 = std::numeric_limits<uint32_t>::max();

    // ---- Can every value be keyed as a whole-number uint32 offset from lo? ----

    uint64_t span = 0;
    bool offsets_fit = false;

    if constexpr (CppIntegerNumber<T>) {
        span = static_cast<uint64_t>(hi) - static_cast<uint64_t>(lo);
        offsets_fit = span < NA_KEY32;
    } else if constexpr (CppFloatType<T>) {
        double dspan = static_cast<double>(hi) - static_cast<double>(lo);
        offsets_fit = dspan < NA_KEY32;

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

    // Counts capped at 32MB and a multiple of n
    constexpr uint64_t MAX_COUNTS = (32ull << 20) / sizeof(uint32_t);
    constexpr uint64_t COUNTS_PER_ELEMENT = 4;
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

    if (offsets_fit) {
        using offset_t = typename std::conditional_t<CppIntegerNumber<T>, std::make_unsigned<T>, std::type_identity<T>>::type;
        std::unique_ptr<uint32_t[]> keys(new uint32_t[n]);
        uint32_t* p_keys = keys.get();
        for_each_chunk(n, calc_threads(n), [&](int, uint32_t start, uint32_t end) noexcept {
            const T* RESTRICT chunk_x = p_x;
            uint32_t* RESTRICT chunk_keys = p_keys;
            const offset_t chunk_lo = lo;
            OMP_SIMD
            for (uint32_t i = start; i < end; ++i) {
                const T v = chunk_x[i];
                chunk_keys[i] = is_na(v) ? NA_KEY32 : static_cast<uint32_t>(static_cast<offset_t>(v) - chunk_lo);
            }
        });
        return order_radix(p_keys, n);
    }

    // ---- Full-width radix ----

    // NA owns the type minimum, so shifting real keys down by 1 keeps the max below the NA key
    constexpr bool shift_below_na = std::signed_integral<T>;
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
                radix_key = to_radix_key(v);
                if constexpr (shift_below_na) {
                    // NA must be the min of signed integer type
                    static_assert(unwrap(na<as_r_scalar_t<T>>()) == std::numeric_limits<T>::min());
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

// Stable sort of string keys and their indices into `strs`, caller has to have built the keys (first 8 bytes only)
inline CPPALLY_NOINLINE void string_radix_sort(const r_str_view* strs, uint64_t* RESTRICT keys, int* RESTRICT idx, uint32_t n) {

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

        uint64_t* run_keys = keys + r.begin;
        int* run_idx = idx + r.begin;
        uint32_t run_n = r.end - r.begin;

        if (r.depth > 0) {
            for (uint32_t j = 0; j < run_n; ++j) {
                run_keys[j] = string_radix_key(strs[run_idx[j]].c_str(), r.depth);
            }
        }

        // A shared prefix leaves the whole run on one window, nothing to sort at this depth
        bool all_equal = std::all_of(run_keys, run_keys + run_n, [w = run_keys[0]](uint64_t k) { return k == w; });
        if (!all_equal) {
            radix_sort(run_keys, run_idx, run_n);
        }

        for (uint32_t j = r.begin; j < r.end;) {
            uint32_t k = j + 1;
            while (k < r.end && keys[k] == keys[j]) {
                ++k;
            }
            // Non-zero last byte means the strings continue past this window
            if (k - j > 1 && (keys[j] & 0xff) != 0) {
                runs.push_back({j, k, r.depth + 8});
            }
            j = k;
        }
    }
}

// Order strings by radix sort, NAs last
inline CPPALLY_NOINLINE void order_strings(const r_str_view* strs, uint32_t n, int* RESTRICT p_out) {

    // ----- Build keys -----
    // Non-NA strings go to the front, NAs in the back
    std::unique_ptr<uint64_t[]> keys(new uint64_t[n]);
    uint32_t n_real = 0, back = n;
    for (uint32_t i = 0; i < n; ++i) {
        if (strs[i].is_na()) {
            p_out[--back] = static_cast<int>(i);
        } else {
            keys[n_real] = string_radix_key(strs[i].c_str(), 0);
            p_out[n_real++] = static_cast<int>(i);
        }
    }
    // NAs were written in reverse order, so reverse that segment to get the correct order
    std::reverse(p_out + n_real, p_out + n);
    // Only sort the non-NAs
    string_radix_sort(strs, keys.get(), p_out, n_real);
}

inline CPPALLY_NOINLINE r_vec<r_int> order_string_data(const r_vec<r_str>& x) {

    uint32_t n = x.length();
    r_vec<r_int> out(n);

    static_assert(sizeof(r_str_view) == sizeof(SEXP) && std::is_standard_layout_v<r_str_view> && std::is_trivially_copyable_v<r_str_view>);
    uint64_t estimate = internal::unique_count_estimate<SEXP, int, internal::r_hash_fn<r_str>, internal::r_hash_eq<r_str>>(x.data(), n);

    const r_str_view* strs = reinterpret_cast<const r_str_view*>(x.data());
    
    // High cardinality -> direct sort
    if (estimate >= n / 4) {
        internal::sort::order_strings(strs, n, out.data());
        return out;
    }

    // Low cardinality -> sort unique strings (NA last), then counting sort by group rank
    groups g = make_groups(x);
    uint32_t n_groups = static_cast<uint32_t>(g.n_groups);
    r_vec<r_int> group_start_locations = g.starts();
    const int* RESTRICT p_starts = group_start_locations.data();
    const int* RESTRICT p_ids = g.ids.data();

    std::vector<r_str_view> unique_strs;
    unique_strs.reserve(n_groups);
    for (uint32_t i = 0; i < n_groups; ++i){
        unique_strs.push_back(strs[p_starts[i]]);
    }
    std::unique_ptr<int[]> sorted_ids(new int[n_groups]);
    internal::sort::order_strings(unique_strs.data(), n_groups, sorted_ids.get());

    std::unique_ptr<uint32_t[]> rank(new uint32_t[n_groups]);
    uint32_t* RESTRICT p_rank = rank.get();
    for (uint32_t r = 0; r < n_groups; ++r) {
        p_rank[sorted_ids[r]] = r;
    }

    internal::sort::counting_order(
        [p_ids, p_rank](int i) noexcept { return p_rank[p_ids[i]]; },
        static_cast<int>(n), n_groups, out.data()
    );
    return out;
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

    if (n > std::numeric_limits<int>::max()) [[unlikely]] {
        abort("Cannot sort long-vectors");
    }
    
    if (n < 200){
        return internal::sort::order_cmp(x);
    }

    if constexpr (RNumericType<data_t>) {

        // ----------------------------------------------------------------------
        // Numeric data
        // ----------------------------------------------------------------------
    
        T rng = range(x, true);
        auto min_val = rng.get(0), max_val = rng.get(1);

        // All NA means order is [0, n)
        if (is_na(min_val) || is_na(max_val)) {
            r_vec<r_int> out(static_cast<r_size_t>(n));
            out.iota();
            return out;
        }
    
        return internal::sort::order_numeric_data(x.data(), n, unwrap(min_val), unwrap(max_val));

    } else if constexpr (RStringType<data_t>) {

        // ----------------------------------------------------------------------
        // Strings
        // ---------------------------------------------------------------------- 

        return internal::sort::order_string_data(r_vec<r_str>(x));
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
