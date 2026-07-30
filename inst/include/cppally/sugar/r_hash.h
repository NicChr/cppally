#ifndef CPPALLY_R_HASH_H
#define CPPALLY_R_HASH_H

#include <cppally/r_vec.h>
#include <cppally/r_visit.h>
#include <cppally/r_attrs.h>
#include <cppally/r_identical.h>
#include <cppally/random/random_stream.h>
#include <bit>
#include <algorithm>
#include <cmath>
#include <ankerl/unordered_dense.h> // Hash maps for group IDs + unique + match

// Hash functions + hash equality operators for RVal and RVector

namespace cppally {

namespace internal {

// Hashing

// Hash combine helper
inline uint64_t hash_combine(uint64_t seed, uint64_t value) noexcept {
    return seed ^ (value + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2));
} 

// High quality 64-bit mixer from murmurhash
inline constexpr uint64_t mix_u64(uint64_t x) noexcept {
    x ^= x >> 33;
    x *= 0xff51afd7ed558ccdULL;
    x ^= x >> 33;
    x *= 0xc4ceb9fe1a85ec53ULL;
    x ^= x >> 33;
    return x;
}

inline consteval uint64_t na_real_hash() noexcept {
    return mix_u64(na_real_bits());
}
inline consteval uint64_t nan_hash() noexcept {
    return mix_u64(nan_bits());
}

template <typename T>
uint64_t r_hash_impl(const T& x) noexcept {
    if constexpr (RTimeType<T>){
        return r_hash_impl(typename T::value_type(x));
    } else if constexpr (RIntegerType<T>){
        return mix_u64(static_cast<uint64_t>(unwrap(x)));
    } else {
        return ankerl::unordered_dense::hash<unwrap_t<T>>{}(unwrap(x));
    }
};

template <>
inline uint64_t r_hash_impl(const r_dbl& x) noexcept {
    if (is_na(x)){
        // Checks that x matches exactly to R's NA_REAL
        return has_na_real_payload(x) ? na_real_hash() : nan_hash();
    } else {
        // Hash normal double
        // +0.0 to normalise -0.0 and 0.0 
        return mix_u64(std::bit_cast<uint64_t>(unwrap(x) + 0.0));
    }
};

template <>
inline uint64_t r_hash_impl(const r_cplx& x) noexcept {
        // Hash real and imag parts and mix
        return hash_combine(r_hash_impl(x.re()), r_hash_impl(x.im()));
};

template <>
inline uint64_t r_hash_impl(const r_str_view& x) noexcept {
    // Cast pointer to integer (uintptr_t)
    auto ptr_val = reinterpret_cast<uintptr_t>(unwrap(x));
    
    // Scramble the bits
    // We use ankerl's built-in wyhash mixer. It's just a multiply + XOR.
    return ankerl::unordered_dense::detail::wyhash::hash(ptr_val);
};

template <>
inline uint64_t r_hash_impl(const r_str& x) noexcept {
    return r_hash_impl(r_str_view(x));
};

template <>
inline uint64_t r_hash_impl(const r_sym& x) noexcept {
    auto ptr_val = reinterpret_cast<uintptr_t>(unwrap(x));
    return ankerl::unordered_dense::detail::wyhash::hash(ptr_val);
};

// Vector hashing

inline uint64_t r_hash_impl(const r_sexp& x);

template <RVector T>
inline uint64_t r_hash_impl(const T& x) {
        
    if (x.is_null()) return 0;
    r_size_t n = x.length();
    // Initialise the seed using the hashed vector type
    uint64_t seed = r_hash_impl(r_int(static_cast<int>(r_typeof<T>)));
    // Hash the attributes list if it exists
    if (attr::has_attrs(x)){
        r_vec<r_sexp> attrs = attr::get_attrs(x);
        seed = hash_combine(seed, r_hash_impl(attrs.names()));
        for (r_size_t i = 0; i < attrs.length(); ++i){
            seed = hash_combine(seed, r_hash_impl(attrs.view(i)));
        }
    }
    // Recursively combine hashes of elements (even if elements are vectors)
    for (r_size_t i = 0; i < n; ++i) {
        seed = hash_combine(seed, r_hash_impl(x.view(i)));
    }
    return seed;
};

inline uint64_t r_hash_impl(const r_factors& x) {
    return r_hash_impl(x.value);
};

inline uint64_t r_hash_impl(const r_df& x) {
    return r_hash_impl(x.value);
};

inline uint64_t r_hash_impl(const r_sexp& x) {
    return r_sexp_view(x, CPPALLY_MAKE_VISITOR(uint64_t, v, r_hash_impl(v)));
}


template <typename T>
struct r_hash {
    using is_avalanching = void; // Tells ankerl this is already a good quality hash
    // For hash map memory efficiency we use the underlying type
    using base_t = unwrap_t<T>;
    uint64_t operator()(const base_t& x) const noexcept(RScalar<T>) {
        if constexpr (std::is_constructible_v<T, base_t, internal::view_tag>){
            return r_hash_impl(T(x, internal::view_tag{}));
        } else {
            return r_hash_impl(T(x));
        }
    }
};

// Hash equality

template <typename T>
struct r_hash_eq {

    using is_transparent = void;

    using base_t = unwrap_t<T>;
    bool operator()(const base_t& a, const base_t& b) const {
        if constexpr (std::is_constructible_v<T, base_t, internal::view_tag>){
            return identical(T(a, internal::view_tag{}), T(b, internal::view_tag{}));
        } else {
            return identical(T(a), T(b));
        }
    }
};

// Vector of data frame row hashes - combine hashes across cols
inline std::vector<uint64_t> row_hashes(const r_df& x) {
    int nrow = x.nrow();
    int ncol = x.ncol();
    std::vector<uint64_t> row_ids(size_t(nrow), 0U);
    for (int c = 0; c < ncol; ++c) {
        internal::view_sexp(x.value.view(c), [&]<typename ColT>(const ColT& col) {
            if constexpr (requires (int i) { r_hash_impl(col.view(i)); }) {
                for (int i = 0; i < nrow; ++i) {
                    row_ids[i] = hash_combine(row_ids[i], r_hash_impl(col.view(i)));
                }
            } else {
                abort("make_groups(r_df): unsupported column type");
            }
        });
    }
    return row_ids;
}

// An extension of Chao's estimator of population size based on the first three capture frequency counts
// doi:10.1016/j.csda.2011.01.017
template <RVector T, typename U>
inline uint64_t unique_count_estimate(const U *px, uint64_t data_size){

    using data_t = typename T::data_type;

    // Bigger sample size mainly reduces skew bias and variance
    uint64_t sample_size = std::max(static_cast<uint64_t>(std::sqrt(2.0 * data_size)) + 1u, uint64_t(1024));

    // Setup RNG engine using custom seed
    random_stream rs(mix_u64(data_size));

    ankerl::unordered_dense::map<
        U,
        uint32_t,
        r_hash<data_t>,
        r_hash_eq<data_t>
    > counts;
    counts.reserve(sample_size);

    uint64_t f1 = 0;
    uint64_t f2 = 0;
    uint64_t f3 = 0;

    for (uint64_t i = 0; i < sample_size; ++i) {

        r_size_t sample_pick = rs.index<r_size_t>(0, static_cast<r_size_t>(data_size) - 1);
        uint32_t& count = counts[px[sample_pick]];
        ++count;

        if (count == 1) {
            // Increase number of singletons by 1
            ++f1;
        } else if (count == 2) {
            // Increase number of doubletons by 1
            // Since this is a doubleton, we decrease the singleton count by 1
            --f1;
            ++f2;
        } else if (count == 3) {
            // Increase number of tripletons by 1
            // Decrease count of doubletons by 1
            --f2;
            ++f3;
        } else if (count == 4){
            // Appears more than 3 times, hence decrease tripleton count by 1
            --f3;
        }
    }

    uint64_t est = counts.size();

    // chao1 estimator formula
    // chao1 = n_obs + ( (f1) * (f1-1) ) / ( 2 * (f2+1) )
    //
    // Lanumteang & Bohning extension - used when f2 and f3 are both > 10
    // Extended chao1 estimator formula
    // chao1_ext = n_obs + ( (3f1f3) / (2f2^2) ) * ( (f1^2) / (2f2) ) = n_obs + ( (3f1^3f3) / (4f2^3) )

    if (f2 > 10 && f3 > 10) {
        est += (3 * f1 * f1 * f1 * f3) / (4 * f2 * f2 * f2);
    } else if (f1 > 1) {
        est += (f1 * (f1 - 1)) / (2 * (f2 + 1));
    }

    return std::min(est, data_size);
}

// Initial guess of unique size N/4
// sampling is used where possible to refine the guess
template <RVector T, typename U>
inline uint64_t get_hash_map_reserve_size(const U *px, uint64_t data_size) {

    using data_t = typename T::data_type;
    using primitive_t = unwrap_t<data_t>;

    // Logical vectors can only have at most 3 unique elements
    if constexpr (is<T, r_vec<r_lgl>>){
        return 4;
    }

    // If the range of possible values is small then no need to sample, we can use that range as the estimate
    if constexpr (CppIntegerType<primitive_t>){
        constexpr uint64_t span = static_cast<uint64_t>(std::numeric_limits<primitive_t>::max()) - static_cast<uint64_t>(std::numeric_limits<primitive_t>::min());
        if ((span + 1u) < 1000u){
            return std::min(data_size, span + 1u);
        }
    }

    uint64_t guess = data_size / 4;

    // Only do sampling if data is large
    if (data_size > static_cast<uint64_t>(internal::exp2<double>(16))){
        guess = std::max(guess, unique_count_estimate<T>(px, data_size));
    }

    guess = std::min(guess, static_cast<uint64_t>(internal::exp2<double>(20))); // Bound to 2^20
    return guess;
}

}

}

#endif
