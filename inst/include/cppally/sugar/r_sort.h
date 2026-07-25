#ifndef CPPALLY_R_SORT_H
#define CPPALLY_R_SORT_H

#include <cppally/r_vec.h>
#include <cppally/sugar/r_hash.h>
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
            return is_na(x.view(i)) ? false : !(x.view(i) < x.view(j)).is_false();
        });
    } else {
        std::sort(p, p + n, [&x](int i, int j) noexcept {
            return is_na(x.view(i)) ? false : !(x.view(i) < x.view(j)).is_false();
        });
    }
    return pv;
}

// Exact whole-number test
inline bool is_exact_whole(double x) noexcept {
    return (std::trunc(x) == x) && !std::isinf(x);
}

}

// 0-indexed ordering permutation vector that represents in sequential order, 
// the indices of `x` elements that need to be chosen to return a sorted `x`
template <RSortableVector T>
inline r_vec<r_int> order(const T& x, bool preserve_ties = true) {

    using data_t = typename T::data_type;
    using base_t = unwrap_t<data_t>;

    uint32_t n = x.length();
    if (n < 1000){
        return internal::order_cmp(x, preserve_ties);
    }

    if constexpr (RNumericType<data_t>) {

    // ----------------------------------------------------------------------
    // Integers or whole numbers with relatively small range optimisation
    // ----------------------------------------------------------------------

    auto rng = range(x, true);
    auto* RESTRICT px = x.data();
    auto min_val = rng.get(0), max_val = rng.get(1);

    // Range is NA only when every value is NA -> sequential indices
    if (is_na(min_val) || is_na(max_val)) {
        r_vec<r_int> out(static_cast<r_size_t>(n));
        out.iota();
        return out;
    }

    base_t lo = unwrap(min_val);
    base_t hi = unwrap(max_val);

    // Absolute cap bounds the counts allocation and prefix-sum work; the
    // relative cap stops small-n/wide-range inputs paying a range-sized scan
    constexpr uint64_t MAX_RANGE = 10000000;
    const uint64_t range_cap = std::min(MAX_RANGE, static_cast<uint64_t>(n) * 32);

    std::size_t range_size = 0;
    bool usable;
    if constexpr (CppFloatType<base_t>) {
        double span = static_cast<double>(hi) - static_cast<double>(lo);
        usable = span >= 0.0 && span < static_cast<double>(range_cap);
        if (usable) {
            constexpr base_t EXACT_LIMIT =
                static_cast<base_t>(uint64_t(1) << std::numeric_limits<base_t>::digits);
            if (lo < -EXACT_LIMIT || hi > EXACT_LIMIT) {
                usable = false;
            } else {
                for (uint32_t i = 0; i < n; ++i) {
                    base_t v = px[i];
                    if (!is_na(v) && !internal::is_exact_whole(v - lo)) {
                        usable = false;
                        break;
                    }
                }
            }
            if (usable) {
                range_size = static_cast<std::size_t>(span) + 1; // span is whole here
            }
        }
    } else {
        uint64_t span = static_cast<uint64_t>(hi) - static_cast<uint64_t>(lo);
        usable = span < range_cap;
        if (usable) {
            range_size = static_cast<std::size_t>(span) + 1;
        }
    }

    // Use counting sort for small range
    if (usable) {

        std::vector<uint32_t> counts(range_size, 0);

        // First pass: count occurrences (ignore NAs)
        bool has_nas = false;
        for (uint32_t i = 0; i < n; ++i) {
            base_t v = px[i];
            if (!is_na(v)) {
                counts[static_cast<std::size_t>(v - lo)]++;
            } else {
                has_nas = true;
            }
        }

        // Prefix sum: counts[i] becomes the starting position for value i
        uint32_t total = 0;
        for (std::size_t i = 0; i < range_size; ++i) {
            uint32_t old_count = counts[i];
            counts[i] = total;
            total += old_count;
        }

        // Second pass: write indices in sorted order (stable)
        r_vec<r_int> out(static_cast<r_size_t>(n));
        int* RESTRICT p_out = out.data();
        for (uint32_t i = 0; i < n; ++i) {
            base_t v = px[i];
            if (!is_na(v)) {
                p_out[counts[static_cast<std::size_t>(v - lo)]++] = static_cast<int>(i);
            }
        }

        // Append NAs at end (preserving input order)
        if (has_nas) {
            for (uint32_t i = 0; i < n; ++i) {
                if (is_na(px[i])) {
                    p_out[total++] = static_cast<int>(i);
                }
            }
        }
        return out;
    }

    r_vec<r_int> out(static_cast<r_size_t>(n));
    int* RESTRICT p_out = out.data();
    const auto* RESTRICT p_x = x.data();

    using unsigned_t = decltype(ska_sort::detail::to_unsigned_or_bool(std::declval<base_t>()));

    // Keys materialised once, co-located with the index, so every radix pass is
    // a sequential scan — no per-pass gather through the permutation index.
    struct key_index {
        unsigned_t key;
        uint32_t index;
    };

    std::vector<key_index> pairs(n);
    for (uint32_t i = 0; i < n; ++i) {
        unsigned_t key;
        if (is_na(p_x[i])) {
            key = std::numeric_limits<unsigned_t>::max();
        } else {
            key = ska_sort::detail::to_unsigned_or_bool(p_x[i]);
            if constexpr (sizeof(unsigned_t) == sizeof(int)) {
                key -= 1u; // keep max real value below the NA sentinel
            }
        }
        pairs[i] = { key, i };
    }

    // Where the sorted result ends up; usually `pairs`, but ska_sort_copy may
    // leave it in the scratch buffer.
    const key_index* RESTRICT src = pairs.data();
    std::vector<key_index> buffer; // only allocated for the 32-bit stable path

    if constexpr (sizeof(unsigned_t) == sizeof(int)) {
        // 32-bit key: LSD ska_sort_copy is stable by construction, so the stable
        // case sorts on the bare key (~4 flat passes) instead of widening to a
        // (key, index) pair. Unstable sorts in place — no scratch buffer.
        if (preserve_ties) {
            buffer.resize(n);
            bool in_buffer = ska_sort::ska_sort_copy(pairs.begin(), pairs.end(), buffer.begin(),
                [](const key_index& k) { return k.key; });
            if (in_buffer) {
                src = buffer.data();
            }
        } else {
            ska_sort::ska_sort(pairs.begin(), pairs.end(),
                [](const key_index& k) { return k.key; });
        }
    } else {
        // 64-bit key: ska_sort_copy degrades to unstable in-place at this width,
        // so stability still needs the (key, index) composite.
        if (preserve_ties) {
            ska_sort::ska_sort(pairs.begin(), pairs.end(),
                [](const key_index& k) { return std::make_pair(k.key, k.index); });
        } else {
            ska_sort::ska_sort(pairs.begin(), pairs.end(),
                [](const key_index& k) { return k.key; });
        }
    }

    OMP_SIMD
    for (uint32_t i = 0; i < n; ++i) {
        p_out[i] = static_cast<int>(src[i].index);
    }
    return out;
    }

    // ----------------------------------------------------------------------
    // Strings
    // ---------------------------------------------------------------------- 

    else if constexpr (RStringType<data_t>) {
    
        r_size_t n = x.length();
        r_vec<r_int> out(n);
        auto* RESTRICT px = x.data();
        
        // Single Hash Map to assign group IDs and count frequencies
        ankerl::unordered_dense::map<SEXP, uint32_t, internal::r_hash<data_t>, internal::r_hash_eq<data_t>> lookup;
        auto n_uniques_guess = internal::get_hash_map_reserve_size<data_t>(n);
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
    return order(x.value);
}

inline r_vec<r_int> order(const r_sexp& x, bool preserve_ties = true);

// Lexicographic order across all columns of a data frame
//
// Segmented multi-key sort:
//   1. Sort all rows by col 0 using the fast single-column order() (ska_sort /
//      counting sort / string hash-sort depending on type)
//   2. Walk the result and find contiguous runs of equal col-0 values (tied
//      segments). Any segment of length > 1 is pushed onto a work stack with
//      col=1
//   3. Pop a frame {start, end, col}: stable_sort out[start..end) by that
//      column, then find ties within the sorted subrange and push survivors
//      with col+1
inline r_vec<r_int> order(const r_df& x, bool preserve_ties = true) {
    int nrow = x.nrow();
    int ncol = x.ncol();

    r_vec<r_int> out(nrow);
    out.iota();

    if (nrow <= 1 || ncol == 0){
        return out;
    }

    int* p_out = out.data();

    // Each frame: sort out[start..end) by columns starting at `col`
    struct frame { int start; int end; int col; };
    std::vector<frame> stack;
    stack.push_back({0, nrow, 0});

    while (!stack.empty()) {
        frame f = stack.back();
        stack.pop_back();
        if (f.col >= ncol) continue;

        internal::view_sexp(x.value.view(f.col), [&]<typename ColT>(const ColT& col) {
            if constexpr (requires (const ColT& c, int i) {
                order(c, false);
                identical(c.view(i), c.view(i));
            }) {
                if (f.start == 0 && f.end == nrow) {
                    // Full-range sort: use the fast single-column order().
                    // Must be stable when preserve_ties=true so rows fully tied
                    // across all columns keep input-order through the chain.
                    r_vec<r_int> o = order(col, preserve_ties);
                    std::memcpy(p_out, o.data(), sizeof(int) * nrow);
                } else {
                    // Subsegment: stable_sort with single-column comparator
                    std::stable_sort(p_out + f.start, p_out + f.end, [&](int a, int b) {
                        if (is_na(col.view(a))) return false;
                        if (is_na(col.view(b))) return true;
                        auto lt = col.view(a) < col.view(b);
                        return static_cast<bool>(unwrap(lt));
                    });
                }

                // Push tied subsegments (size > 1) for next column
                if (f.col + 1 < ncol) {
                    int seg_start = f.start;
                    for (int i = f.start + 1; i < f.end; ++i) {
                        if (!identical(col.view(p_out[i]), col.view(p_out[i - 1]))) {
                            if (i - seg_start > 1) {
                                stack.push_back({seg_start, i, f.col + 1});
                            }
                            seg_start = i;
                        }
                    }
                    if (f.end - seg_start > 1) {
                        stack.push_back({seg_start, f.end, f.col + 1});
                    }
                }
            } else {
                abort("order(r_df): column does not support ordering");
            }
        });
    }

    return out;
}


inline r_vec<r_int> order(const r_sexp& x, bool preserve_ties);

// Is x in a sorted order? i.e is x increasing but not necessarily monotonically?
// To retrieve a bool result, use the `is_true` member function
template <typename T>
requires requires (const T& v, r_size_t i){ v.view(i) >= v.view(i); }
inline r_lgl is_sorted(const T& x) {
    r_size_t n = x.length();
    for (r_size_t i = 1; i < n; ++i) {
        r_lgl is_increasing = x.view(i) >= x.view(i - 1);

        // If NA return NA, if false return false
        if (!is_increasing.is_true()){
            return is_increasing;
        }
    }
    return r_true;
}

// Sorting

namespace internal {

// In-place sort
template <typename T>
requires requires (const T& v, r_size_t i) { order(v); v.get(i);}
void sort_in_place(T& x){

    r_vec<r_int> o = order(x);
    int n = static_cast<int>(x.length());
    
    // Apply the permutation to x via cycle-following: no extra buffer,
    // o doubles as the visited marker (o[j] = j once that slot is final).
    for (int i = 0; i < n; ++i){
        if (unwrap(o.get(i)) == i) continue;
    
        int j = i;
        auto temp = x.view(i);
        while (unwrap(o.view(j)) != i){
            int next = unwrap(o.view(j));
            x.set(j, x.view(next));
            o.set(j, j);
            j = next;
        }
        x.set(j, temp);
        o.set(j, j);
    }
}

}

template <typename T>
requires requires (T&& v, r_size_t i) { order(v); v.get(i);}
std::remove_cvref_t<T> sort(T&& x){
    if constexpr (std::is_same_v<T, std::remove_cvref_t<T>>){
        if (x.is_exclusive()){
            internal::sort_in_place(x);
            return std::move(x);
        }
    }
    return pmap_parallel_simd([&](r_int a){ return x.get(a);}, order(x));
}

}

#endif
