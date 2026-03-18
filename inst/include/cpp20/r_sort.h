#ifndef CPP20_R_SORT_H
#define CPP20_R_SORT_H

#include <cpp20/r_vec.h>
#include <cpp20/r_hash.h>
#include <cpp20/r_stats.h>

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

        // Use this instead of above for stable sorting
        // ska_sort(pairs.begin(), pairs.end(), 
        //     [](const key_index& k){ return std::make_tuple(k.key, k.index); });

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
    
        r_size_t n = x.length();
        r_vec<r_int> out(n);
        auto* RESTRICT px = x.data();
        
        // Single Hash Map to assign group IDs and count frequencies
        ankerl::unordered_dense::map<SEXP, uint32_t, internal::r_hash<T>, internal::r_hash_eq<T>> lookup;
        lookup.reserve(internal::get_hash_map_reserve_size<T>(n));
        
        std::vector<SEXP> uniques;
        std::vector<uint32_t> counts;
        std::vector<uint32_t> group_ids(n); // Caches the ID for each element
        
        uint32_t na_count = 0;
        uint32_t last_id = uint32_t(-1);
        
        for (uint32_t i = 0; i < n; ++i) {
            SEXP str = px[i];
            
            if (str == NA_STRING) {
                group_ids[i] = uint32_t(-1);
                last_id = uint32_t(-1); // Break linear cache
                na_count++;
            } 
            // Linear Scan Cache - identical strings have identical pointers
            else if (i > 0 && str == px[i - 1]) { 
                group_ids[i] = last_id;
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
                group_ids[i] = last_id;
            }
        }

        uint32_t n_uniques = uniques.size();

        // Sort the unique group IDs
        std::vector<uint32_t> sorted_ids(n_uniques);
        OMP_SIMD
        for (uint32_t i = 0; i < sorted_ids.size(); ++i) sorted_ids[i] = i;
        
        std::sort(sorted_ids.begin(), sorted_ids.end(), [&](uint32_t a, uint32_t b) {
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
