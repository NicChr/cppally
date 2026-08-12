#ifndef CPPALLY_R_SORT_H
#define CPPALLY_R_SORT_H

// ------- Hybrid sorting for R vectors -------
// All sorting is implemented by sorting NA values last (like `order(..., na.last = TRUE)`)
// ska_sort is used for radix sorting. Copyright Malte Skarupke 2016.
// Small vectors are sorted using a comparison sort via std::sort/std::stable_sort.
// Large vectors of integers or doubles with no fractional part use a counting sort when the
// range is relatively small. 
// 64-bit types (int64, dates, date-times) with a wider range that still fits
// a uint32 offset radix-sort on that narrower key instead of the full 64-bit key. Everything
// else falls back to a full-width ska_sort.
// Strings are sorted by first de-duplicating (getting unique) strings, and then using a counting sort.

#include <cppally/r_vec.h>
#include <cppally/hash/hash.h>
#include <cppally/sugar/r_stats.h>
#include <cstdint> // For uint32_t and similar
#include <cstring> // For strcmp
#include <vector> // For C++ vectors
#include <numeric>
#include <limits>
#include <cmath>
#include <algorithm> // For std::min
#include <ankerl/unordered_dense.h> // Hash maps for group IDs + unique + match
#include <ska_sort/ska_sort.hpp> // For radix sorting via ska_sort

namespace cppally {

namespace internal {

// general order vector that sorts `x`
// NAs are ordered last
// Internal function to be used for low overhead sorting small vectors
template <RSortableVector T>
r_vec<r_int> order_cmp(const T& x, bool stable = true) {
    int n = x.length();
    r_vec<r_int> pv(n);
    pv.iota();
    auto* RESTRICT p = pv.data();

    if (stable){
        std::stable_sort(p, p + n, [&x](int i, int j) noexcept {
            return is_na(x.view(i)) ? false : !((x.view(i) < x.view(j)).is_false());
        });
    } else {
        std::sort(p, p + n, [&x](int i, int j) noexcept {
            return is_na(x.view(i)) ? false : !((x.view(i) < x.view(j)).is_false());
        });
    }
    return pv;
}

template <typename key_t>
struct key_index {
    key_t key;
    uint32_t index;
};

// Radix sort of pre-materialised (key, index) pairs. Keys are co-located with
// the index so every pass is a sequential scan - no per-pass gather through
// the permutation index. NAs must already be mapped to the max key value.
template <typename key_t>
inline r_vec<r_int> order_radix(std::vector<key_index<key_t>>& pairs, bool stable) {

    uint32_t n = static_cast<uint32_t>(pairs.size());

    // Where the sorted result ends up; usually `pairs`, but ska_sort_copy may
    // leave it in the scratch buffer.
    const key_index<key_t>* RESTRICT src = pairs.data();
    std::vector<key_index<key_t>> buffer; // only allocated for the 32-bit stable path

    if constexpr (sizeof(key_t) == sizeof(int)) {
        // 32-bit key: LSD ska_sort_copy is stable by construction, so the stable
        // case sorts on the bare key (~4 flat passes) instead of widening to a
        // (key, index) pair. Unstable sorts in place - no scratch buffer.
        if (stable) {
            buffer.resize(n);
            bool in_buffer = ska_sort::ska_sort_copy(pairs.begin(), pairs.end(), buffer.begin(),
                [](const key_index<key_t>& k) { return k.key; });
            if (in_buffer) {
                src = buffer.data();
            }
        } else {
            ska_sort::ska_sort(pairs.begin(), pairs.end(),
                [](const key_index<key_t>& k) { return k.key; });
        }
    } else {
        // 64-bit key: ska_sort_copy degrades to unstable in-place at this width,
        // so stability still needs the (key, index) composite.
        if (stable) {
            ska_sort::ska_sort(pairs.begin(), pairs.end(),
                [](const key_index<key_t>& k) { return std::make_pair(k.key, k.index); });
        } else {
            ska_sort::ska_sort(pairs.begin(), pairs.end(),
                [](const key_index<key_t>& k) { return k.key; });
        }
    }

    r_vec<r_int> out(static_cast<r_size_t>(n));
    int* RESTRICT p_out = out.data();

    OMP_SIMD
    for (uint32_t i = 0; i < n; ++i) {
        p_out[i] = static_cast<int>(src[i].index);
    }
    return out;
}

}

// 0-indexed ordering permutation vector that represents in sequential order, 
// the indices of `x` elements that need to be chosen to return a sorted `x`
template <RSortableVector T>
inline r_vec<r_int> order(const T& x, bool preserve_ties = true) {

    using data_t = typename T::data_type;
    using base_t = unwrap_t<data_t>;

    uint32_t n = x.length();
    
    if (n < 200){
        return internal::order_cmp(x, preserve_ties);
    }

    if constexpr (RNumericType<data_t>) {

    // ----------------------------------------------------------------------
    // Integers or whole numbers with relatively small range optimisation
    // ----------------------------------------------------------------------

    auto rng = range(x, true);
    auto min_val = rng.get(0), max_val = rng.get(1);

    // Range is NA only when every value is NA -> sequential indices
    if (is_na(min_val) || is_na(max_val)) {
        r_vec<r_int> out(static_cast<r_size_t>(n));
        out.iota();
        return out;
    }

    base_t lo = unwrap(min_val);
    base_t hi = unwrap(max_val);

    using unsigned_t = decltype(ska_sort::detail::to_unsigned_or_bool(std::declval<base_t>()));

    // counts costs O(range) to zero and prefix-sum whatever n is, and is probed
    // once per element in both the count and scatter passes, so cap it in bytes
    // as well as relative to n. 64-bit keys take the wider ratio: their radix
    // fallback pairs into 16 bytes rather than 8, and when stable sorts on
    // key+index rather than the key alone.
    constexpr uint64_t MAX_COUNTS_BYTES = 32ull << 20;
    constexpr uint64_t MAX_RANGE = std::min<uint64_t>(
        MAX_COUNTS_BYTES / sizeof(uint32_t),
        std::numeric_limits<int>::max()
    ); // integer branch computes v - lo in int
    constexpr uint64_t RANGE_RATIO = sizeof(unsigned_t) > sizeof(int) ? 16 : 4;
    const uint64_t range_cap = std::min(MAX_RANGE, static_cast<uint64_t>(n) * RANGE_RATIO);

    // 64-bit types whose values are whole-number offsets from lo spanning less
    // than a uint32 window can radix-sort on uint32 keys through the cheap
    // 32-bit path instead of the 64-bit composite. The strict limit keeps the
    // max real key below UINT32_MAX, which is reserved for the NA sentinel
    constexpr uint64_t NARROW_LIMIT = std::numeric_limits<uint32_t>::max();

    std::size_t range_size = 0;
    bool int_count_usable = false;
    bool narrow = false;

    const auto* RESTRICT p_x = x.data();

    if constexpr (CppIntegerType<base_t>){
        uint64_t span = static_cast<uint64_t>(hi) - static_cast<uint64_t>(lo);
        int_count_usable = span < range_cap;
        if (int_count_usable) {
            range_size = static_cast<std::size_t>(span) + 1;
        } else if constexpr (sizeof(base_t) > sizeof(int)) {
            narrow = span < NARROW_LIMIT;
        }
    } else if constexpr (CppFloatType<base_t>) {
        double span = static_cast<double>(hi) - static_cast<double>(lo);
        // whole = every value is an exact whole-number offset from lo
        bool whole = span >= 0.0 && span < static_cast<double>(NARROW_LIMIT);
        if (whole) {
            constexpr base_t EXACT_LIMIT =
                static_cast<base_t>(uint64_t(1) << (std::numeric_limits<base_t>::digits - 1)) * 2;
            if (lo < -EXACT_LIMIT || hi > EXACT_LIMIT) {
                whole = false;
            } else {
                for (uint32_t i = 0; i < n; ++i) {
                    base_t v = p_x[i];
                    if (!is_na(v) && !internal::double_is_int_like(v - lo)) {
                        whole = false;
                        break;
                    }
                }
            }
        }
        
        int_count_usable = whole && span < static_cast<double>(range_cap);

        if (int_count_usable) {
            range_size = static_cast<std::size_t>(span) + 1; // span is whole here
        } else {
            narrow = whole;
        }
    }

    // Use counting sort for small range
    if (int_count_usable) {

        std::vector<uint32_t> counts(range_size, 0);

        // First pass: count occurrences (ignore NAs)
        for (uint32_t i = 0; i < n; ++i) {
            base_t v = p_x[i];
            if (!is_na(v)) {
                counts[static_cast<std::size_t>(v - lo)]++;
            }
        }

        // Prefix sum: counts[i] becomes the starting position for value i
        uint32_t total = 0;
        for (std::size_t i = 0; i < range_size; ++i) {
            uint32_t old_count = counts[i];
            counts[i] = total;
            total += old_count;
        }

        // Write indices in sorted order (stable)
        r_vec<r_int> out(static_cast<r_size_t>(n));
        int* RESTRICT p_out = out.data();
        for (uint32_t i = 0; i < n; ++i) {
            base_t v = p_x[i];
            if (is_na(v)) {
                // Append NAs at end (preserving input order)
                p_out[total++] = static_cast<int>(i);
            } else {
                p_out[counts[static_cast<std::size_t>(v - lo)]++] = static_cast<int>(i);
            }
        }
        return out;
    }

    // Narrow window: keys are the uint32 offsets from lo (only reachable for
    // 64-bit base types)
    if (narrow) {
        std::vector<internal::key_index<uint32_t>> pairs(n);
        for (uint32_t i = 0; i < n; ++i) {
            base_t v = p_x[i];
            uint32_t key = is_na(v)
                ? std::numeric_limits<uint32_t>::max()
                : static_cast<uint32_t>(v - lo);
            pairs[i] = { key, i };
        }
        return internal::order_radix(pairs, preserve_ties);
    }

    std::vector<internal::key_index<unsigned_t>> pairs(n);
    for (uint32_t i = 0; i < n; ++i) {
        unsigned_t key;
        if (is_na(p_x[i])) {
            key = std::numeric_limits<unsigned_t>::max();
        } else {
            key = ska_sort::detail::to_unsigned_or_bool(p_x[i]);
            if constexpr (RIntegerType<data_t> && (unwrap(na<data_t>()) == std::numeric_limits<base_t>::min())){
                key -= 1u; // keep max real value below the NA sentinel
            }
        }
        pairs[i] = { key, i };
    }
    return internal::order_radix(pairs, preserve_ties);
    }

    // ----------------------------------------------------------------------
    // Strings
    // ---------------------------------------------------------------------- 

    else if constexpr (RStringType<data_t>) {
    
        r_size_t n = x.length();
        r_vec<r_int> out(n);
        auto* RESTRICT px = x.data();
        
        // Single Hash Map to assign group IDs and count frequencies
        ankerl::unordered_dense::map<SEXP, uint32_t, internal::r_hash_fn<data_t>, internal::r_hash_eq<data_t>> lookup;
        auto n_uniques_guess = internal::get_hash_map_reserve_size<T>(px, n);
        lookup.reserve(n_uniques_guess);
        
        std::vector<SEXP> uniques;
        uniques.reserve(n_uniques_guess);
        std::vector<uint32_t> counts;
        counts.reserve(n_uniques_guess);
        std::vector<uint32_t> group_ids; // Caches the ID for each element
        group_ids.reserve(n);
        
        uint32_t last_id = uint32_t(-1);
        
        for (uint32_t i = 0; i < n; ++i) {
            SEXP str = px[i];
            
            if (internal::ptrs_identical(str, na<r_str_view>())){
                group_ids.push_back(uint32_t(-1));
                last_id = uint32_t(-1); // Break linear cache
            } 
            // Linear Scan Cache - identical strings have identical pointers
            else if (i > 0 && str == px[i - 1]) { 
                group_ids.push_back(last_id);
                counts[last_id]++;
            } 
            else {
                auto [it, inserted] = lookup.try_emplace(str, uniques.size());
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

        // Sort the unique group IDs by string content (C-locale byte order)
        std::vector<uint32_t> sorted_ids(n_uniques);
        std::iota(sorted_ids.begin(), sorted_ids.end(), 0u);

        // Low cardinality: strcmp sort is already trivial, and building keys would
        // be pure overhead. High cardinality: pack the first 8 bytes of each unique
        // big-endian so a plain uint64 compare reproduces strcmp's unsigned byte
        // order, letting most comparisons skip the strcmp call (strcmp breaks ties)
        if (n_uniques < 256) {
            std::sort(sorted_ids.begin(), sorted_ids.end(), [&](uint32_t a, uint32_t b) {
                return std::strcmp(CHAR(uniques[a]), CHAR(uniques[b])) < 0;
            });
        } else {
            std::vector<uint64_t> prefix(n_uniques);
            for (uint32_t id = 0; id < n_uniques; ++id) {
                const char* s = CHAR(uniques[id]);
                uint64_t k = 0;
                for (int b = 0; b < 8 && s[b]; ++b) {
                    k |= static_cast<uint64_t>(static_cast<unsigned char>(s[b])) << (56 - 8 * b);
                }
                prefix[id] = k;
            }
            std::sort(sorted_ids.begin(), sorted_ids.end(), [&](uint32_t a, uint32_t b) {
                if (prefix[a] != prefix[b]) { return prefix[a] < prefix[b]; }
                return std::strcmp(CHAR(uniques[a]), CHAR(uniques[b])) < 0;
            });
        }
        
        // Prefix Sums: calculate the starting write offset for each group
        std::vector<uint32_t> offsets(n_uniques);
        uint32_t current_offset = 0;
        
        for (uint32_t id : sorted_ids) {
            offsets[id] = current_offset;
            current_offset += counts[id];
        }
        uint32_t na_offset = current_offset; // NAs go at the very end
        
        // Distribute indices (Counting Sort)
        int* RESTRICT p_out = out.data();
        
        for (uint32_t i = 0; i < n; ++i) {
            uint32_t id = group_ids[i];
            if (id == uint32_t(-1)) {
                p_out[na_offset++] = i;
            } else {
                p_out[offsets[id]++] = i;
            }
        }
        
        return out;
    } else {
        return internal::order_cmp(x, preserve_ties);
    }
}

inline r_vec<r_int> order(const r_factors& x, bool preserve_ties = true) {
    return order(x.value, preserve_ties);
}

// Sorting

namespace internal {

// In-place sort
template <typename T>
requires requires (const T& v, r_size_t i) { v.get(i);}
void sort_in_place(T& x, r_vec<r_int>&& order){

    int n = static_cast<int>(x.length());

    if (n != order.length()) [[unlikely]] {
        abort("%s: `x` and `order` must have the same length", __func__);
    }

    // Since we are overwriting order, ensure it is not overwriting user data
    order.ensure_exclusive();
    
    // Apply the permutation to x via cycle-following: no extra buffer,
    // o doubles as the visited marker (o[j] = j once that slot is final).
    for (int i = 0; i < n; ++i){
        if (unwrap(order.get(i)) == i) continue;
    
        int j = i;
        auto temp = x.view(i);
        while (unwrap(order.view(j)) != i){
            int next = unwrap(order.view(j));
            x.set(j, x.view(next));
            order.set(j, j);
            j = next;
        }
        x.set(j, temp);
        order.set(j, j);
    }
}

}

template <typename T>
requires requires (T&& v, r_size_t i) { order(v); v.get(i);}
std::remove_cvref_t<T> sort(T&& x){
    
    if constexpr (RVector<T> && RNumericType<typename std::remove_cvref_t<T>::data_type>){

        r_size_t n = x.length();
        bool is_sorted = true;

        for (r_size_t i = 1; i < n; ++i) {

            if (is_na(x.view(i))){
                
                // Since x[i] is NA, x is sorted IFF the rest of the values are also NA
                for (r_size_t j = i + 1; j < n; ++j) {
                    if (!is_na(x.view(j))){
                        is_sorted = false;
                        break;
                    }
                }
                break;
            }

            r_lgl is_increasing = x.view(i) >= x.view(i - 1);
    
            // If NA return NA, if false return false
            if (!is_increasing.is_true()){
                is_sorted = false;
                break;
            }
        }

        if (is_sorted){
            if constexpr (std::is_same_v<T, std::remove_cvref_t<T>>){
                return std::move(x);
            }
            return x;
        }
    }

    r_vec<r_int> o = order(x, /*preserve_ties = */ false);

    if constexpr (std::is_same_v<T, std::remove_cvref_t<T>>){
        if (x.is_exclusive()){
            internal::sort_in_place(x, std::move(o));
            return std::move(x);
        }
    }
    return pmap_parallel_simd([&](r_int a){ return x.get(unwrap(a));}, std::move(o));
}

}

#endif
