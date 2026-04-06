#ifndef CPP20_R_HASH_H
#define CPP20_R_HASH_H

#include <cpp20/r_vec.h>
#include <cpp20/r_visit.h>
#include <cpp20/r_attrs.h>
#include <cpp20/sugar/r_identical.h>
#include <bit>
#include <algorithm>
#include <ankerl/unordered_dense.h> // Hash maps for group IDs + unique + match

// Hash functions + hash equality operators for RVal and RVector

namespace cpp20 {

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
struct r_hash_impl {
    // This tells Ankerl map 'this hash is already high quality'
    using is_avalanching = void;

    [[nodiscard]] uint64_t operator()(unwrap_t<T> x) const noexcept {
        if constexpr (RTimeType<T>){
            return r_hash_impl<inherited_type_t<T>>{}(T(x));
        } else if constexpr (RIntegerType<T>){
            return mix_u64(static_cast<uint64_t>(x));
        } else {
            return ankerl::unordered_dense::hash<unwrap_t<T>>{}(x);
        }
    }
};

template <>
struct r_hash_impl<r_dbl> {
    using is_avalanching = void;

    [[nodiscard]] uint64_t operator()(double x) const noexcept {
        if (is_na(x)){
            // Checks that x matches exactly to R's NA_REAL
            return is_na_real(x) ? na_real_hash() : nan_hash();
        } else {
            // Hash normal double
            // +0.0 to normalise -0.0 and 0.0 
            return mix_u64(std::bit_cast<uint64_t>(x + 0.0));
        }
    }
};

template <>
struct r_hash_impl<r_cplx> {
    using is_avalanching = void;

    [[nodiscard]] uint64_t operator()(std::complex<double> x) const noexcept {
        r_hash_impl<r_dbl> hasher;
        // Hash real and imag parts and mix
        uint64_t h1 = hasher(x.real());
        uint64_t h2 = hasher(x.imag());
        return hash_combine(h1, h2);
    }
};

template<>
struct r_hash_impl<r_str_view> {
    using is_avalanching = void;

    [[nodiscard]] uint64_t operator()(SEXP x) const noexcept {
        // Cast pointer to integer (uintptr_t)
        auto ptr_val = reinterpret_cast<uintptr_t>(x);
        
        // Scramble the bits
        // We use ankerl's built-in wyhash mixer. It's just a multiply + XOR.
        return ankerl::unordered_dense::detail::wyhash::hash(ptr_val);
    }
};

template<>
struct r_hash_impl<r_str> {
    using is_avalanching = void;

    [[nodiscard]] uint64_t operator()(SEXP x) const noexcept {
        return r_hash_impl<r_str_view>{}(x);
    }
};

template <>
struct r_hash_impl<r_sym> {
    [[nodiscard]] uint64_t operator()(SEXP x) const noexcept {
        return r_hash_impl<r_str_view>{}(x);
    }
};


// Vector hashing

// // Forward declarations before r_hash_impl<r_sexp>
inline uint64_t hash_factor(const r_factors& x);

template <RVector T>
uint64_t hash_vec(const T& x);

inline uint64_t hash_sym(const r_sym& x);

template <RVal T>
struct r_hash_impl<r_vec<T>>;

// Specialization for elements of lists
template<>
struct r_hash_impl<r_sexp> {
    using is_avalanching = void;
    [[nodiscard]] uint64_t operator()(SEXP x) const {
        auto x_ = r_sexp(x, internal::view_tag{});
        if (x_.is_null()) return 0;
        
        // Recursively hash the element
        return view_sexp(x_, [](const auto& vec) -> uint64_t {
            using vec_t = std::remove_cvref_t<decltype(vec)>;
            if constexpr (is<vec_t, r_sexp>){
                abort("Unsupported element type, current implementation can only hash vectors and factors");
            } else if constexpr (RFactor<vec_t>){
                return hash_factor(vec);
            } else if constexpr (RSymbolType<vec_t>){
                return hash_sym(vec);
            } else {
                return hash_vec(vec);
            }
        });
    }
};

template <RVal T>
struct r_hash_impl<r_vec<T>> {
    using is_avalanching = void;
    [[nodiscard]] uint64_t operator()(const SEXP& x) const noexcept {

        r_vec<T> xvec = r_vec<T>(x, internal::view_tag{});
        
        if (xvec.is_null()) return 0;
        r_size_t n = xvec.length();
        // Initialise the seed using the hashed vector type
        uint64_t seed = r_hash_impl<r_int>{}(r_typeof<r_vec<T>>);
        // Hash the attributes list if it exists
        if (attr::has_attrs(xvec)){
            r_vec<r_sexp> attrs = attr::get_attrs(xvec);
            seed = hash_combine(seed, r_hash_impl<r_vec<r_str_view>>{}(attrs.names()));
            for (r_size_t i = 0; i < attrs.length(); ++i){
                seed = hash_combine(seed, r_hash_impl<r_sexp>{}(attrs.view(i)));
            }
        }
        // Recursively combine hashes of elements (even if elements are vectors)
        for (r_size_t i = 0; i < n; ++i) {
            seed = hash_combine(seed, r_hash_impl<T>{}(unwrap(xvec.view(i))));
        }
        return seed;
    }
};

template<>
struct r_hash_impl<r_factors> {
    using is_avalanching = void;
    [[nodiscard]] uint64_t operator()(const SEXP& x) const noexcept {
        return r_hash_impl<r_vec<r_int>>{}(x);
    }
};

// Defined after r_vec<T> and r_factors are complete
inline uint64_t hash_factor(const r_factors& x) {
    return r_hash_impl<r_vec<r_int>>{}(x.value);
}

template <RVector T>
uint64_t hash_vec(const T& x) {
    return r_hash_impl<T>{}(x);
}

inline uint64_t hash_sym(const r_sym& x) {
    return r_hash_impl<r_sym>{}(x);
}

template <typename T>
struct r_hash : r_hash_impl<std::remove_cvref_t<T>> {};

// Hash equality

template <RVal T>
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

// // Alternative (not working atm)

// template <RIntegerType T>
// uint64_t r_hash_impl (const T& x) noexcept {
//     return mix_u64(static_cast<uint64_t>(unwrap(x)));
// };

// template <RFloatType T>
// uint64_t r_hash_impl (const T& x) noexcept {
//     if (is_na(x)){
//         // Checks that x matches exactly to R's NA_REAL
//         return is_na_real(x) ? na_real_hash() : nan_hash();
//     } else {
//         // Hash normal double
//         // +0.0 to normalise -0.0 and 0.0 
//         return mix_u64(std::bit_cast<uint64_t>(unwrap(x) + 0.0));
//     }
// };

// template <RComplexType T>
// uint64_t r_hash_impl (const T& x) noexcept {
//     // Hash real and imag parts and mix
//     uint64_t h1 = r_hash_impl<r_dbl>(x.re());
//     uint64_t h2 = r_hash_impl<r_dbl>(x.im());
//     return hash_combine(h1, h2);
// };

// template <RStringType T>
// uint64_t r_hash_impl (const T& x) noexcept {
//     // Cast pointer to integer (uintptr_t)
//     uintptr_t ptr_val = reinterpret_cast<uintptr_t>(unwrap(x));
//     // Scramble the bits
//     // We use ankerl's built-in wyhash mixer. It's just a multiply + XOR.
//     return ankerl::unordered_dense::detail::wyhash::hash(ptr_val);
// };

// template <RRawType T>
// uint64_t r_hash_impl (const T& x) noexcept {
//     return mix_u64(static_cast<uint64_t>(unwrap(x)));
// }

// template <RTimeType T>
// uint64_t r_hash_impl (const T& x) noexcept {
//     return r_hash_impl<inherited_type_t<T>>(x);
// };


// // Same as hashing strings above
// template <RSymbolType T>
// uint64_t r_hash_impl (const T& x) noexcept {
//     uintptr_t ptr_val = reinterpret_cast<uintptr_t>(unwrap(x));
//     return ankerl::unordered_dense::detail::wyhash::hash(ptr_val);
// };

// template <CastableToRVal T>
// requires (CppType<T>)
// uint64_t r_hash_impl (const T& x) noexcept {
//     return r_hash_impl(as_r_val_t<T>(x));
// };

// // Vector hashing

// // // Forward declarations before r_hash_impl<r_sexp>
// template <RVector T>
// uint64_t r_hash_impl(const T& x) noexcept;
// template <RFactor T>
// uint64_t r_hash_impl(const T& x) noexcept ;

// // Specialization for elements of lists
// template <typename T>
// requires (is<T, r_sexp>)
// uint64_t r_hash_impl(const T& x) noexcept {
//     auto x_ = r_sexp(x, internal::view_tag{});
//     if (x_.is_null()) return 0;
//     // Recursively hash the element
//     return view_sexp(x_, [](const auto& vec) -> uint64_t {
//         using vec_t = std::remove_cvref_t<decltype(vec)>;
//         if constexpr (is<vec_t, r_sexp>){
//             abort("Unsupported element type, current implementation can only hash vectors and factors");
//         } else {
//             return r_hash_impl<vec_t>(vec);
//         }
//     });
// };
// template <RVector T>
// uint64_t r_hash_impl(const T& x) noexcept {

//     if (x.is_null()) return 0;
//     r_size_t n = x.length();
//     // Initialise the seed using the hashed vector type
//     uint64_t seed = r_hash_impl<r_int>(r_int(r_typeof<r_vec<T>>));
//     // Hash the attributes list if it exists
//     if (attr::has_attrs(x)){
//         r_vec<r_sexp> attrs = attr::get_attrs(x);
//         seed = hash_combine(seed, r_hash_impl(attrs.names()));
//         for (r_size_t i = 0; i < attrs.length(); ++i){
//             seed = hash_combine(seed, r_hash_impl<r_sexp>(attrs.view(i)));
//         }
//     }
//     // If vector is a list then we recursively combine hashes of vector elements
//     if constexpr (is<T, r_sexp>){
//         for (r_size_t i = 0; i < n; ++i){
//             uint64_t h = view_sexp(x.view(i), [](const auto& vec) -> uint64_t {
//                 using vec_t = std::remove_cvref_t<decltype(vec)>;
//                 if constexpr (is<vec_t, r_sexp>){
//                     abort("List contains unsupported element type, current implementation can only hash vectors and factors");
//                 } else {
//                     return r_hash_impl<vec_t>(vec);
//                 }
//             });
//             seed = hash_combine(seed, h);
//         }
//     } else {
//         for (r_size_t i = 0; i < n; ++i) {
//             seed = hash_combine(seed, r_hash_impl<T>(x.view(i)));
//         }
//     }
//     return seed;
// };

// template <RFactor T>
// uint64_t r_hash_impl(const T& x) noexcept {
//     return r_hash_impl<r_vec<r_int>>(x.value);
// }

// template <typename T>
// struct r_dense_map_hash {
//     using is_avalanching = void;
//     [[nodiscard]] uint64_t operator()(const unwrap_t<T>& x) const noexcept {
//         return r_hash_impl<T>(x);
//     }
// };

// template <typename T>
// struct r_hash : r_dense_map_hash<std::remove_cvref_t<T>>{};
