#ifndef CPPALLY_R_HASH_H
#define CPPALLY_R_HASH_H

#include <cppally/r_vec.h>
#include <cppally/r_visit.h>
#include <cppally/r_attrs.h>
#include <cppally/r_identical.h>
#include <bit>
#include <algorithm>
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

inline consteval uint64_t na_real_hash(){
    return mix_u64(na_real_bits());
}
inline consteval uint64_t nan_hash(){
    return mix_u64(nan_bits());
}

template <typename T>
uint64_t r_hash_impl(const T& x) noexcept {
    if constexpr (RTimeType<T>){
        return r_hash_impl(inherited_type_t<T>(x));
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
        return is_na_real(x) ? na_real_hash() : nan_hash();
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

template <RVector T>
inline uint64_t r_hash_impl(const T& x) noexcept {
        
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

template<>
inline uint64_t r_hash_impl(const r_factors& x) noexcept {
    return r_hash_impl(x.value);
};


// Specialization for elements of lists
template<>
inline uint64_t r_hash_impl(const r_sexp& x) noexcept {
    return CPPALLY_VIEW_AND_APPLY(x, /*return_type = */ uint64_t, /*fn = */ r_hash_impl);
};


template <typename T>
struct r_hash {
    
    using is_avalanching = void; // Tells ankerl this is already a good quality hash

    // For hash map memory efficiency we use the underlying type
    using base_t = unwrap_t<T>;
    uint64_t operator()(const base_t& x) const noexcept {
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


// Initial guess of unique size is N/4 floored to the nearest power of 2
template <typename T>
inline uint64_t get_hash_map_reserve_size(uint64_t data_size) {
    uint64_t res = std::bit_floor(data_size >> 2);
    return std::min<uint64_t>(res, 1ULL << 19); // Cap to 2^19
}

template <>
inline uint64_t get_hash_map_reserve_size<r_lgl>(uint64_t data_size) {
    return std::min<uint64_t>(std::bit_floor(data_size >> 2), 4);
}



}

}

#endif
