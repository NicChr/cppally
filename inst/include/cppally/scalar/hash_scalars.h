#ifndef CPPALLY_HASH_SCALARS_H
#define CPPALLY_HASH_SCALARS_H

#include <cppally/scalar/scalars.h>
#include <cppally/na.h>
#include <cppally/r_sym.h>
#include <cppally/std_compat.h>
#include <ankerl/unordered_dense.h> // Hash maps for group IDs + unique + match

// Hash functions for RScalar

namespace cppally {

namespace internal {

// Different NaN have different bit representations so this normalises NaN hashing, and r_dbl::nan is guaranteed to not collide with r_dbl::na
inline consteval uint64_t nan_bits() noexcept {
    return internal::bit_cast<uint64_t>(static_cast<double>(r_dbl::nan()));
}

inline consteval uint64_t na_real_bits() noexcept {
    return internal::bit_cast<uint64_t>(static_cast<double>(na<r_dbl>()));
}

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

template <RIntegerType T>
inline uint64_t r_hash_impl(const T& x) noexcept {
    return mix_u64(static_cast<uint64_t>(unwrap(x)));
};

template <RScalar T>
inline uint64_t r_hash_impl(const T& x) noexcept {

    using value_type = typename std::remove_cvref_t<T>::value_type;

    if constexpr (RScalar<value_type>){
        return r_hash_impl(static_cast<value_type>(x));
    } else {
        // Fallback
        return ankerl::unordered_dense::hash<unwrap_t<T>>{}(unwrap(x));
    }
};

template<>
inline uint64_t r_hash_impl(const r_dbl& x) noexcept {
    if (is_na(x)){
        // Checks that x matches exactly to R's NA_REAL
        return has_na_real_payload(x) ? na_real_hash() : nan_hash();
    } else {
        // Hash normal double
        // +0.0 to normalise -0.0 and 0.0 
        return mix_u64(internal::bit_cast<uint64_t>(unwrap(x) + 0.0));
    }
};

template<>
inline uint64_t r_hash_impl(const r_cplx& x) noexcept {
    // Hash real and imag parts and mix
    return hash_combine(r_hash_impl(x.re()), r_hash_impl(x.im()));
};


CPPALLY_NOINLINE inline uint64_t hash_str_text(r_str_view x){
    using str_hash = ankerl::unordered_dense::hash<std::string_view>;
    if (!x.needs_utf8_translation()) {
        return str_hash{}(x.cpp_str());
    } else {
        return str_hash{}(string::utf8_string(x));
    }
}

// Hash the UTF-8 text
inline uint64_t r_hash_impl(const r_str_view& x) {
    // ASCII is unmarked, so ASCII-based CHARSXPs are unique and we can hash the pointer
    if (x.is_na() || x.is_ascii()) {
        return ankerl::unordered_dense::detail::hash_int(reinterpret_cast<uintptr_t>(unwrap(x)));
    } else {
        return hash_str_text(x);
    }
}

inline uint64_t r_hash_impl(const r_str& x) {
    return r_hash_impl(r_str_view(x));
}

inline uint64_t r_hash_impl(r_sym x) noexcept {
    auto ptr_val = reinterpret_cast<uintptr_t>(unwrap(x));
    return ankerl::unordered_dense::detail::hash_int(ptr_val);
};

}

}

#endif
