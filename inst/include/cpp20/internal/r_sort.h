#ifndef CPP20_R_SORT_H
#define CPP20_R_SORT_H

#include <cpp20/internal/r_vec.h>
#include <cpp20/internal/r_stats.h>

namespace cpp20 {

namespace internal {

// general order vector that sorts `x`
// NAs are ordered last
// Internal function to be used for low overhead sorting small vectors (n<500)
template <RSortableType T>
r_vec<r_int> cpp_order(const r_vec<T>& x) {
    int n = x.size();
    r_vec<r_int> p(n);
    OMP_SIMD
    for (r_size_t i = 0; i < n; ++i) p.set(i, i);

    auto *p_x = x.data();

    if constexpr (RNumericType<T>){
        std::sort(p.begin(), p.end(), [&](int i, int j) {
            if (is_na(x.view(i))) return false;
            if (is_na(x.view(j))) return true;
            return p_x[i] < p_x[j];
        });
    } else {
        // Below works on strings
        std::sort(p.begin(), p.end(), [&](int i, int j) {
            auto res = x.view(i) < x.view(j);
            if (is_na(res)){
                if (is_na(x.view(i))){
                    return false;
                } else {
                    return true;
                }
            }
            return static_cast<bool>(unwrap(res));
        });
    }
    return p;
}

template <RSortableType T>
r_vec<r_int> cpp_stable_order(const r_vec<T>& x) {
    int n = x.size();
    r_vec<r_int> p(n);
    OMP_SIMD
    for (r_size_t i = 0; i < n; ++i) p.set(i, i);

    auto *p_x = x.data();

    if constexpr (RNumericType<T>){
        std::stable_sort(p.begin(), p.end(), [&](int i, int j) {
            if (is_na(x.view(i))) return false;
            if (is_na(x.view(j))) return true;
            return p_x[i] < p_x[j];
        });
    } else {
        // Below works on strings
        std::stable_sort(p.begin(), p.end(), [&](int i, int j) {
            auto res = x.view(i) < x.view(j);
            if (is_na(res)){
                if (is_na(x.view(i))){
                    return false;
                } else {
                    return true;
                }
            }
            return static_cast<bool>(unwrap(res));
        });
    }
    return p;
}

}

// Stable order that preserves order of ties
template <RSortableType T>
r_vec<r_int> stable_order(const r_vec<T>& x) {

    using base_t = unwrap_t<T>;

    uint32_t n = x.size();

    if (n < 500){
        return internal::cpp_stable_order(x);
    }

    // ----------------------------------------------------------------------
    // Numeric types
    // ----------------------------------------------------------------------

    if constexpr (RNumericType<T>){

        using unsigned_t = decltype(detail::to_unsigned_or_bool(std::declval<base_t>()));

        r_vec<r_int> out(n);
        auto* RESTRICT p_x = x.data();

        struct key_index {
            unsigned_t key;
            uint32_t index;
        };
        std::vector<key_index> pairs;
        pairs.reserve(n);

        OMP_SIMD
        for (uint32_t i = 0; i < n; ++i) {
            unsigned_t key = is_na(p_x[i]) ? std::numeric_limits<unsigned_t>::max() : detail::to_unsigned_or_bool(p_x[i]);
            pairs.push_back({key, i});
        }

        ska_sort(pairs.begin(), pairs.end(),
        [](const key_index& k) { 
            return std::make_pair(k.key, k.index); 
        });

        int* RESTRICT p_out = out.data();
        OMP_SIMD
        for (uint32_t i = 0; i < n; ++i) {
            p_out[i] = static_cast<int>(pairs[i].index);
        }
        return out;

        // r_vec<r_int> out(n);
        // auto* RESTRICT p_x = x.data();

        // std::vector<uint64_t> pairs;
        // pairs.reserve(n);

        // for (uint32_t i = 0; i < n; ++i) {
        //     auto val = p_x[i];
        //     uint32_t key = is_na(val) ? std::numeric_limits<uint32_t>::max() : detail::to_unsigned_or_bool(val);
        //     // Pack: Key High, Index Low
        //     pairs.push_back((static_cast<uint64_t>(key) << 32) | i);
        // }

        // // Fast & Stable (because index is part of the unique value)
        // ska_sort(pairs.begin(), pairs.end());

        // int* RESTRICT p_out = out.data();
        // for (uint32_t i = 0; i < n; ++i) {
        //     p_out[i] = static_cast<int>(pairs[i] & static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()));
        // }
        // return out;
    } else if constexpr (RStringType<T>) {

        r_vec<r_int> out(n);
        
        // Use pair for stable sorting: (string, index)
        std::vector<std::pair<std::string_view, uint32_t>> non_na_pairs;
        std::vector<uint32_t> na_indices;
        
        non_na_pairs.reserve(n);
        na_indices.reserve(n / 3);
        
        for (uint32_t i = 0; i < n; ++i) {
            if (is_na(x.view(i))) {
                na_indices.push_back(i);
            } else {
                non_na_pairs.emplace_back(static_cast<std::string_view>(x.view(i).c_str()), i);
            }
        }
        
        // sorts by string first, then index for ties
        ska_sort(non_na_pairs.begin(), non_na_pairs.end());
        
        // Write results: non-NA first, then NAs
        int* RESTRICT p_out = out.data();
        uint32_t pos = 0;
        
        for (const auto& pair : non_na_pairs) {
            p_out[pos++] = static_cast<int>(pair.second);
        }
        
        for (uint32_t na_idx : na_indices) {
            p_out[pos++] = static_cast<int>(na_idx);
        }
        
        return out;
    } else {
        return internal::cpp_stable_order(x);
    }
}

// order function (doesn't preserve order of ties)
template <RSortableType T>
inline r_vec<r_int> order(const r_vec<T>& x) {

    using base_t = unwrap_t<T>;

    uint32_t n = x.size();
    if (n < 500) return internal::cpp_order(x);

    
    // ----------------------------------------------------------------------
    // Types with numeric storage
    // ----------------------------------------------------------------------
    
    if constexpr (RIntegerType<T>) {

        if (n >= 100000){

        r_vec<r_int> out(n);

        auto rng = range(x, true);

        auto* RESTRICT px = x.data();
        
        // Find min/max and check for NAs
        auto min_val = rng.get(0), max_val = rng.get(1);
        r_int64 delta = internal::as_r<r_int64>(max_val) - internal::as_r<r_int64>(min_val);
        bool all_nas = is_na(delta);
        
        // All NAs - just return sequential indices
        if (all_nas) {
            OMP_SIMD
            for (uint32_t i = 0; i < n; ++i) out.set(i, static_cast<int>(i));
            return out;
        }

        constexpr int64_t COUNTING_SORT_THRESHOLD = 10000000;
        
        // Use counting sort for small range (O(n + range))
        if ((delta >= 0 && delta < COUNTING_SORT_THRESHOLD).is_true()){
            std::vector<uint32_t> counts(unwrap(delta) + 1, 0);
            
            // First pass: count occurrences (ignore NAs)
            bool has_nas = false;
            for (uint32_t i = 0; i < n; ++i) {
                if (!is_na(px[i])) {
                    size_t idx = static_cast<size_t>(unwrap(px[i]) - min_val);
                    counts[idx]++;
                } else {
                    has_nas = true;
                }
            }
            
            // Prefix sum: counts[i] becomes the starting position for value i
            uint32_t total = 0;
            uint32_t n_counts = counts.size();
            for (size_t i = 0; i < n_counts; ++i) {
                uint32_t old_count = counts[i];
                counts[i] = total;
                total += old_count;
            }
            
            // Second pass: write indices in sorted order (stable)
            int* RESTRICT p_out = out.data();
            
            // For each element, place it at counts[value], then increment
            for (uint32_t i = 0; i < n; ++i) {
                if (!is_na(px[i])) {
                    size_t idx = static_cast<size_t>(unwrap(px[i]) - min_val);
                    p_out[counts[idx]++] = static_cast<int>(i);
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
    }
}

    if constexpr (RNumericType<T>) {

        using unsigned_t = decltype(detail::to_unsigned_or_bool(std::declval<base_t>()));
        
        r_vec<r_int> out(n);
        auto* RESTRICT px = x.data();
        
        struct key_index {
            unsigned_t key;
            uint32_t index;
        };
        
        std::vector<key_index> pairs;
        pairs.reserve(n);

        for (uint32_t i = 0; i < n; ++i) {
            unsigned_t key = is_na(px[i]) ? std::numeric_limits<unsigned_t>::max() : detail::to_unsigned_or_bool(px[i]);
            pairs.push_back({key, i});
        }
        ska_sort(pairs.begin(), pairs.end(), 
            [](const key_index& k){ return k.key; });

        int* RESTRICT p_out = out.data();
        OMP_SIMD
        for (uint32_t i = 0; i < n; ++i) {
            p_out[i] = static_cast<int>(pairs[i].index);
        }
        return out;
    }

    // ----------------------------------------------------------------------
    // Strings
    // ---------------------------------------------------------------------- 

    else if constexpr (RStringType<T>) {
        
        r_vec<r_int> out(n);

        r_vec<r_str_view> str_vec = r_vec<r_str_view>(unwrap(x), internal::view_tag{});
        
        struct key_index {
            std::string_view str;
            uint32_t index;
            bool is_na;
        };
        std::vector<key_index> pairs;
        pairs.reserve(n);
    
        for (uint32_t i = 0; i < n; ++i) {
            if (is_na(str_vec.view(i))){
                // Use empty string for NA
                pairs.push_back({"", i, true});
            } else {
                pairs.push_back({str_vec.view(i).cpp_str(), i, false});
            }
        }
    
        // Partition: non-NA first, NAs at end
        auto na_start = std::partition(pairs.begin(), pairs.end(),
            [](const key_index& p) { return !p.is_na; });
    
        // Sort non-NA strings
        if (na_start != pairs.begin()) {
            ska_sort(pairs.begin(), na_start, 
                [](const key_index& s) -> const std::string_view& { 
                    return s.str; 
                });
        }
    
        // Unpack indices
        int* RESTRICT p_out = out.data();
        OMP_SIMD
        for (uint32_t i = 0; i < n; ++i) {
            p_out[i] = static_cast<int>(pairs[i].index);
        }
        return out; 
    } else {
        return internal::cpp_order(x);
    }
}


// Is x in a sorted order? i.e is x increasing but not necessarily monotonically?
// To retrieve a bool result, use the `is_true` member function
template <RSortableType T>
inline r_lgl is_sorted(const r_vec<T>& x) {
    
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

}

#endif
