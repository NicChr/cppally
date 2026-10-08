#ifndef CPPALLY_R_SORT_H
#define CPPALLY_R_SORT_H

// ------- Hybrid sorting for R vectors -------
// All sorting is implemented by sorting NA values last (like `order(..., na.last = TRUE)`)
// ska_sort is used for radix sorting. Copyright Malte Skarupke 2016.
// Small vectors are sorted using a comparison sort via std::stable_sort.
// Large vectors of integers or doubles with no fractional part use a counting sort when the
// range is relatively small. 
// 64-bit types (int64, dates, date-times) with a wider range that still fits
// a uint32 offset radix-sort on that narrower key instead of the full 64-bit key. Everything
// else falls back to a full-width ska_sort.
// Strings are sorted by first de-duplicating (getting unique) strings, and then using a counting sort.

#include <cppally/vector/r_vector.h>
#include <cppally/hash/hash.h>
#include <cppally/stats/range.h> // For range
#include <cppally/sort/is_sorted.h>
#include <cppally/sort/counting_order.h>
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

struct key_of {
    template <typename key_t>
    key_t operator()(const key_index<key_t>& k) const noexcept { return k.key; }
};

// Radix sort of pre-materialised (key, index) pairs. Keys are co-located with
// the index so every pass is a sequential scan - no per-pass gather through
// the permutation index. NAs must already be mapped to the max key value.
template <typename key_t>
inline r_vec<r_int> order_radix(std::vector<key_index<key_t>>& pairs) {

    uint32_t n = static_cast<uint32_t>(pairs.size());

    // Where the sorted result ends up; usually `pairs`, but ska_sort_copy may
    // leave it in the scratch buffer.
    const key_index<key_t>* RESTRICT src = pairs.data();
    std::vector<key_index<key_t>> buffer;

    if constexpr (sizeof(key_t) == sizeof(int)) {
        // 32-bit key: LSD ska_sort_copy is stable by construction, so the stable
        // case sorts on the bare key (~4 flat passes) instead of widening to a
        // (key, index) pair.
        buffer.resize(n);
        bool in_buffer = ska_sort::ska_sort_copy(pairs.begin(), pairs.end(), buffer.begin(), key_of{});
        if (in_buffer) {
            src = buffer.data();
        }
    } else {
        // 64-bit key: ska_sort_copy degrades to unstable in-place at this width,
        // so stability still needs the (key, index) composite.
        ska_sort::ska_sort(pairs.begin(), pairs.end(),
        [](const key_index<key_t>& k) { return std::make_pair(k.key, k.index); });
    }

    r_vec<r_int> out(static_cast<r_size_t>(n));
    int* RESTRICT p_out = out.data();

    OMP_PARALLEL_FOR_SIMD(calc_threads(n))
    for (uint32_t i = 0; i < n; ++i) {
        p_out[i] = static_cast<int>(src[i].index);
    }
    return out;
}

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
        offsets_fit = dspan >= 0.0 && dspan < NA_KEY32 && lo >= -EXACT_LIMIT && hi <= EXACT_LIMIT;
        for (uint32_t i = 0; offsets_fit && i < n; ++i) {
            offsets_fit = is_na(p_x[i]) || numeric_cast_is_lossless<int>(p_x[i] - lo);
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
        std::vector<key_index<uint32_t>> pairs(n);
        for (uint32_t i = 0; i < n; ++i) {
            T v = p_x[i];
            uint32_t narrow_key = is_na(v) ? NA_KEY32 : static_cast<uint32_t>(v - lo);
            pairs[i] = { narrow_key, i };
        }
        return order_radix(pairs);
    }

    // ---- Full-width radix ----

    // NA owns the type minimum, so shifting real keys down by 1 keeps the max below the NA key
    constexpr bool shift_below_na = CppIntegerType<T> && is<T, unwrap_t<as_r_scalar_t<T>>> &&
        unwrap(na<as_r_scalar_t<T>>()) == std::numeric_limits<T>::min();
    constexpr radix_key_t NA_KEY = std::numeric_limits<radix_key_t>::max();

    std::vector<key_index<radix_key_t>> pairs(n);
    for (uint32_t i = 0; i < n; ++i) {
        radix_key_t radix_key = NA_KEY;
        if (!is_na(p_x[i])) {
            T v = p_x[i] + T(0); // + 0 normalises -0.0 to 0.0
            radix_key = ska_sort::detail::to_unsigned_or_bool(v);
            if constexpr (shift_below_na) {
                radix_key -= 1u;
            }
        }
        pairs[i] = { radix_key, i };
    }
    return order_radix(pairs);
}

}

// 0-indexed ordering permutation vector that represents in sequential order, 
// the indices of `x` elements that need to be chosen to return a sorted `x`
template <RSortableVector T>
inline r_vec<r_int> order(const T& x) {

    using data_t = typename T::data_type;

    uint32_t n = x.length();
    
    if (n < 200){
        return internal::order_cmp(x);
    }

    if constexpr (RNumericType<data_t>) {

        // ----------------------------------------------------------------------
        // Numeric data
        // ----------------------------------------------------------------------
    
        T rng = range(x, true);
        auto min_val = rng.get(0), max_val = rng.get(1);
    
        return internal::order_numeric_data(x.data(), n, unwrap(min_val), unwrap(max_val));

    } else if constexpr (RStringType<data_t>) {

        // ----------------------------------------------------------------------
        // Strings
        // ---------------------------------------------------------------------- 
    
        r_vec<r_int> out(n);
        auto* RESTRICT px = x.data();
        
        // Single Hash Map to assign group IDs and count frequencies
        ankerl::unordered_dense::map<SEXP, int, internal::r_hash_fn<data_t>, internal::r_hash_eq<data_t>> lookup;
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
            
            if (internal::ptrs_identical(str, unwrap(na<r_str_view>()))){
                group_ids.push_back(uint32_t(-1));
            }
            // Linear Scan Cache - identical strings have identical pointers
            else if (i > 0 && str == px[i - 1]) { 
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

        // Sort the unique group IDs by string content (C-locale byte order)
        std::vector<uint32_t> sorted_ids(n_uniques);
        std::iota(sorted_ids.begin(), sorted_ids.end(), 0u);

        // Low cardinality: strcmp sort is already trivial, and building keys would
        // be pure overhead. High cardinality: pack the first 8 bytes of each unique
        // big-endian so a plain uint64 compare reproduces strcmp's unsigned byte
        // order, letting most comparisons skip the strcmp call (strcmp breaks ties).
        std::vector<uint64_t> prefix;
        if (n_uniques >= 256) {
            prefix.resize(n_uniques);
            for (uint32_t id = 0; id < n_uniques; ++id) {
                const char* s = CHAR(uniques[id]);
                uint64_t k = 0;
                for (int b = 0; b < 8 && s[b]; ++b) {
                    k |= static_cast<uint64_t>(static_cast<unsigned char>(s[b])) << (56 - 8 * b);
                }
                prefix[id] = k;
            }
        }

        const uint64_t* pref = prefix.empty() ? nullptr : prefix.data();
        std::sort(sorted_ids.begin(), sorted_ids.end(), [&, pref](uint32_t a, uint32_t b) {
            if (pref && pref[a] != pref[b]) { return pref[a] < pref[b]; }
            return std::strcmp(CHAR(uniques[a]), CHAR(uniques[b])) < 0;
        });

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
        return internal::order_cmp(x);
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

namespace internal {

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
            internal::sort_in_place(x, std::move(o));
            return std::move(x);
        }
    }
    return pmap_parallel_simd([&](r_int a){ return x.view(unwrap(a));}, std::move(o));
}

}

#endif
