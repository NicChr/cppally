#ifndef CPP20_R_SORT_H
#define CPP20_R_SORT_H

#include <cpp20/internal/r_vec.h>

namespace cpp20 {

namespace internal {

// general order vector that sorts `x`
// NAs are ordered last
// Internal function to be used for low overhead sorting small vectors (n<500)
template <RSortable T>
r_vec<r_int> cpp_order(const r_vec<T>& x) {
    int n = x.size();
    r_vec<r_int> p(n);
    OMP_SIMD
    for (r_size_t i = 0; i < n; ++i) p.set(i, i);

    auto *p_x = x.data();

    if constexpr (RMathType<T>){
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

template <RSortable T>
r_vec<r_int> cpp_stable_order(const r_vec<T>& x) {
    int n = x.size();
    r_vec<r_int> p(n);
    OMP_SIMD
    for (r_size_t i = 0; i < n; ++i) p.set(i, i);

    auto *p_x = x.data();

    if constexpr (RMathType<T>){
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
template <RSortable T>
r_vec<r_int> stable_order(const r_vec<T>& x) {
    int n = x.size();

    if (n < 500){
        return internal::cpp_stable_order(x);
    }

    // ----------------------------------------------------------------------
    // Integers (Stable Radix Sort on uint64_t)
    // ----------------------------------------------------------------------

    if constexpr (RIntegerType<T>){

        r_vec<r_int> p(n);
        auto* RESTRICT p_x = x.data();

        std::vector<uint64_t> pairs(n);

        for (int i = 0; i < n; ++i) {
            auto val = p_x[i];
            uint32_t key;
            if (is_na(p_x[i])) {
                key = 0xFFFFFFFF; // Force NA last
            } else {
                key = detail::to_unsigned_or_bool(val);
            }
            // Pack: Key High, Index Low
            pairs[i] = (static_cast<uint64_t>(key) << 32) | static_cast<uint32_t>(i);
        }

        // Fast & Stable (because index is part of the unique value)
        ska_sort(pairs.begin(), pairs.end());

        int* RESTRICT p_out = p.data();
        for (int i = 0; i < n; ++i) {
            p_out[i] = static_cast<int>(pairs[i] & 0xFFFFFFFF);
        }
        return p;
    }

    // ----------------------------------------------------------------------
    // Doubles (Stable Radix Sort on Pair<u64, u32>)
    // ----------------------------------------------------------------------
    // Since double is 64-bit, we can't pack (Value+Index) into one 64-bit int.
    // We use a vector of key-index pairs and sort that

    else if constexpr (RFloatType<T>) {

        r_vec<r_int> p(n);
        auto* RESTRICT p_x = x.data();

        std::vector<std::pair<uint64_t, uint32_t>> pairs(n);

        uint32_t vec_size = n;

        for (uint32_t i = 0; i < vec_size; ++i) {
            double val = p_x[i];
            uint64_t key = is_na(p_x[i]) ? 0xFFFFFFFFFFFFFFFFULL : detail::to_unsigned_or_bool(val);
            pairs[i] = {key, i};
        }

        ska_sort(pairs.begin(), pairs.end());

        int* RESTRICT p_out = p.data();
        for (int i = 0; i < n; ++i) {
            p_out[i] = static_cast<int>(pairs[i].second);
        }
        return p;
    } else if constexpr (RStringType<T>) {
        r_vec<r_int> p(n);
        
        // Use pair for stable sorting: (string, index)
        std::vector<std::pair<std::string, uint32_t>> non_na_pairs;
        std::vector<uint32_t> na_indices;
        
        non_na_pairs.reserve(n);
        na_indices.reserve(n / 3);
        
        for (int i = 0; i < n; ++i) {
            if (is_na(x.view(i))) {
                na_indices.push_back(i);
            } else {
                non_na_pairs.emplace_back(x.view(i).cpp_str(), static_cast<uint32_t>(i));
            }
        }
        
        // sorts by string first, then index for ties
        ska_sort(non_na_pairs.begin(), non_na_pairs.end());
        
        // Write results: non-NA first, then NAs
        int* RESTRICT p_out = p.data();
        int pos = 0;
        
        for (const auto& pair : non_na_pairs) {
            p_out[pos++] = static_cast<int>(pair.second);
        }
        
        for (uint32_t na_idx : na_indices) {
            p_out[pos++] = static_cast<int>(na_idx);
        }
        
        return p;
    } else {
        return internal::cpp_stable_order(x);
    }
}

// order function (doesn't preserve order of ties)
template <RSortable T>
inline r_vec<r_int> order(const r_vec<T>& x) {
    int n = x.size();

    if (n < 500){
        return internal::cpp_order(x);
    }

    // ----------------------------------------------------------------------
    // Integers (ska_sort)
    // ----------------------------------------------------------------------

    if constexpr (RIntegerType<T>) {

        r_vec<r_int> p(n);
        auto* RESTRICT px = x.data();

        struct key_index {
            uint32_t key;
            int index;
        };
        std::vector<key_index> pairs(n);

        for (int i = 0; i < n; ++i) {
            auto val = px[i];
            uint32_t key;
            if (is_na(val)) {
                key = 0xFFFFFFFF; // NA Last
            } else {
                key = detail::to_unsigned_or_bool(val);
            }
            pairs[i] = {key, i};
        }

        ska_sort(pairs.begin(), pairs.end(), [](const key_index& k){ return k.key; });

        int* RESTRICT p_out = p.data();
        OMP_SIMD
        for (int i = 0; i < n; ++i) {
            p_out[i] = pairs[i].index;
        }
        return p;
    }

    // ----------------------------------------------------------------------
    // Doubles (unstable Radix Sort on Pair<u64, int>)
    // ----------------------------------------------------------------------
    // Since double is 64-bit, we can't pack (Value+Index) into one 64-bit int.
    // We must use a struct { uint64_t key; int index; } and sort that

    else if constexpr (RFloatType<T>) {

        r_vec<r_int> p(n);
        auto* RESTRICT px = x.data();

        struct key_index { uint64_t key; int index; };
        std::vector<key_index> pairs(n);

        for (int i = 0; i < n; ++i) {
            double val = px[i];
            uint64_t key;
            if (is_na(val)) {
                key = 0xFFFFFFFFFFFFFFFFULL; // Force NA last
            } else {
                key = detail::to_unsigned_or_bool(val);

            }
            pairs[i] = {key, i};
        }

        ska_sort(pairs.begin(), pairs.end(), [](const key_index& k){ return k.key; });

        int* RESTRICT p_out = p.data();
        OMP_SIMD
        for (int i = 0; i < n; ++i) {
            p_out[i] = pairs[i].index;
        }
        return p;

    // ----------------------------------------------------------------------
    // Strings (unstable Radix Sort on trio<string, int, bool>
    // ----------------------------------------------------------------------

    } else if constexpr (RStringType<T>) {
        r_vec<r_int> p(n);
    
        struct key_index {
            std::string str;
            int index;
            bool is_na;
        };
        std::vector<key_index> pairs;
        pairs.reserve(n);
    
        for (int i = 0; i < n; ++i) {
            if (is_na(x.view(i))) {
                // Use empty string for NA, mark with flag
                pairs.push_back({"", i, true});
            } else {
                pairs.push_back({x.view(i).c_str(), i, false});
            }
        }
    
        // Partition: non-NA first, NAs at end
        auto na_start = std::partition(pairs.begin(), pairs.end(),
            [](const key_index& p) { return !p.is_na; });
    
        // Sort non-NA strings using ska_sort
        if (na_start != pairs.begin()) {
            ska_sort(pairs.begin(), na_start, 
                [](const key_index& s) -> const std::string& { 
                    return s.str; 
                });
        }
    
        // Unpack indices
        int* RESTRICT p_out = p.data();
        for (int i = 0; i < n; ++i) {
            p_out[i] = pairs[i].index;
        }
        return p;  
    } else {
        return internal::cpp_order(x);
    }
}


// Is x in a sorted order? i.e is x increasing but not necessarily monotonically?
// To retrieve a bool result, use the `is_true` member function
template <RSortable T>
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
