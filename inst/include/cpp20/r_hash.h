#ifndef CPP20_R_HASH_H
#define CPP20_R_HASH_H

#include <cpp20/r_vec.h>
#include <cpp20/r_visit.h>
#include <cpp20/r_attrs.h>
#include <bit>
#include <algorithm>

// Hash functions + hash equality operators for RVal and RVector

namespace cpp20 {

namespace internal {

// identical checks that a and b are exactly the same
// Always returns true if they are both the same NA
// needed for hash equality

template <typename T>
inline bool identical_impl(const T& a, const T& b) {
    if constexpr (RVal<T>){
        return unwrap(a) == unwrap(b);
    } else if constexpr (CastableToRVal<T>){
        using r_t = as_r_val_t<T>;
        return identical_impl<r_t>(r_t(a), r_t(b));
    } else {
        return a == b;
    }
}

template<>
inline bool identical_impl<r_dbl>(const r_dbl& a, const r_dbl& b) {
    // If both (NA or NaN)
    if (is_na(a) && is_na(b)){
        return is_na_real(unwrap(a)) == is_na_real(unwrap(b));
    } else {
        return unwrap(a) == unwrap(b);
    }
}

template<>
inline bool identical_impl<r_cplx>(const r_cplx& a, const r_cplx& b) {
    return identical_impl(a.re(), b.re()) && identical_impl(a.im(), b.im());
}

template <RVector T>
inline bool identical_impl(const T& a, const T& b) {
    SEXP x = unwrap(a);
    SEXP y = unwrap(b);
    if (x == y) return true; // same pointer
    if (a.length() != b.length()) return false;
    if (TYPEOF(a) != TYPEOF(b)) return false;
    
    bool x_has_attrs = attr::has_attrs(a);
    bool y_has_attrs = attr::has_attrs(b);
    if (x_has_attrs != y_has_attrs) return false;
    
    if (x_has_attrs && y_has_attrs){
        r_vec<r_sexp> a_attrs = attr::get_attrs(a);
        r_vec<r_sexp> b_attrs = attr::get_attrs(b);

        if (a_attrs.length() != b_attrs.length()) return false;
        if (!identical_impl(a_attrs.names(), b_attrs.names())) return false;
        
            // Only do the rest of the attr checks if pointers do not match
            if (unwrap(a_attrs) != unwrap(b_attrs)){
                    r_vec<r_str_view> names1 = a_attrs.names();
                    r_vec<r_str_view> names2 = b_attrs.names();
                    if (!identical_impl(names1, names2)) return false;

                    for (r_size_t i = 0; i < a_attrs.length(); ++i){
                        if (!identical_impl(a_attrs.view(i), b_attrs.view(i))) return false;
                    }
            }   
        // Not sure why this produces recursion crash when it can handle lists..
        // if (!identical_impl(a_attrs, b_attrs)){
        //     return false;
        // }
    }

    r_size_t n = a.length();

    if constexpr (is<T, r_vec<r_sexp>>){
        
        // Visit each list element
        for (r_size_t i = 0; i < n; ++i){

        bool ident = view_sexp(a.view(i), [&](const auto& vec1) -> bool {
            using vec1_t = std::remove_cvref_t<decltype(vec1)>;
    
            // If we can't map SEXP to a known type then just use R's version
            if constexpr (is<vec1_t, r_sexp>){
                return R_compute_identical(a.view(i), b.view(i), 16);
            } else {
                // Important: to reduce usage of nested view_sexp, we use the fact that
                // types were checked earlier (via TYPEOF), therefore b[[i]] can be constructed the same way as a[[i]]
                // as they share the same type
                auto vec2 = vec1_t(b.view(i), view_tag{});
                return identical_impl(vec1, vec2);  
            }
            });
            if (!ident){
                return false;
            }
        }
    } else {
        for (r_size_t i = 0; i < n; ++i){
            if (!identical_impl(a.view(i), b.view(i))){
                return false;
            }
        } 
    }
    return true;
}

inline bool identical_impl(const r_factors& a, const r_factors& b) {
    return identical_impl(a.value, b.value);
}

template <>
inline bool identical_impl<r_sexp>(const r_sexp& a, const r_sexp& b) {
    SEXP x = unwrap(a);
    SEXP y = unwrap(b);
    if (x == y) return true; // same pointer
    
    // Visit both SEXP
    return view_sexp(a, [&](const auto& vec1) -> bool {
        using vec1_t = decltype(vec1);

        if constexpr (is<vec1_t, r_sexp>){
            return R_compute_identical(x, y, 16);
        } else {
            return view_sexp(b, [vec1](const auto& vec2) -> bool {
                using vec2_t = decltype(vec2);

                if constexpr (!is<vec1_t, vec2_t>){
                    return false;
                } else {
                    return identical_impl(vec1, vec2);
                }
            });
        }
        });
}

inline bool identical_impl(SEXP a, SEXP b) {
    return identical_impl<r_sexp>(r_sexp(a, view_tag{}), r_sexp(b, view_tag{}));
}

}

// Identical takes type into account
template <typename T, typename U>
inline constexpr bool identical(const T& a, const U& b) {
    if constexpr (is<T, U>){
        return internal::identical_impl(a, b);
    } else {
        return false;
    }
}

// Hashing

namespace internal {

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

    uint64_t operator()(unwrap_t<T> x) const noexcept {
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

    uint64_t operator()(double x) const noexcept {
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

    uint64_t operator()(std::complex<double> x) const noexcept {
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

    uint64_t operator()(SEXP x) const noexcept {
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

    uint64_t operator()(SEXP x) const noexcept {
        return r_hash_impl<r_str_view>{}(x);
    }
};

// Vector hashing

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
            } else {
                return r_hash_impl<vec_t>{}(vec);
            }
        });
    }
};

template <RVal T>
struct r_hash_impl<r_vec<T>> {
    using is_avalanching = void;

    [[nodiscard]] uint64_t operator()(const r_vec<T>& x) const noexcept {
        
        if (x.is_null()) return 0;

        r_size_t n = x.length();

        // Initialise the seed using the hashed vector type
        uint64_t seed = r_hash_impl<r_int>{}(r_typeof<r_vec<T>>);

        // Hash the attributes list if it exists

        if (attr::has_attrs(x)){
            r_vec<r_sexp> attrs = attr::get_attrs(x);

            seed = hash_combine(seed, r_hash_impl<r_vec<r_str_view>>{}(attrs.names()));
            for (r_size_t i = 0; i < attrs.length(); ++i){
                seed = hash_combine(seed, r_hash_impl<r_sexp>{}(attrs.view(i)));
            }
        }

        // If vector is a list then we recursively combine hashes of vector elements
        
        if constexpr (is<T, r_sexp>){
            
            for (r_size_t i = 0; i < n; ++i){

                // Recursively hash the elements
                uint64_t h = view_sexp(x.view(i), [](const auto& vec) -> uint64_t {
        
                    using vec_t = std::remove_cvref_t<decltype(vec)>;
        
                    if constexpr (is<vec_t, r_sexp>){
                        abort("List contains unsupported element type, current implementation can only hash vectors and factors");
                    } else {
                        return r_hash_impl<vec_t>{}(vec);
                    }
                });
                seed = hash_combine(seed, h);
        }
    } else {
        for (r_size_t i = 0; i < n; ++i) {
            seed = hash_combine(seed, r_hash_impl<T>{}(unwrap(x.view(i))));
        }
    }
    return seed;
}
};

template<>
struct r_hash_impl<r_factors> {
    using is_avalanching = void;

    [[nodiscard]] uint64_t operator()(const r_factors& x) const noexcept {
        return r_hash_impl<r_vec<r_int>>{}(x.value);
    }
};


template <typename T>
struct r_hash : r_hash_impl<std::remove_cvref_t<T>> {};

// Hash equality

template <RVal T>
struct r_hash_eq {
    using is_transparent = void;
    using base_t = unwrap_t<T>;
    bool operator()(const base_t& a, const base_t& b) const noexcept {
        return identical(a, b);
    }
};


// Return initial hash map reserve size as power of 2
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

// Useful helper to calculate n unique values - can be useful for various algorithms
template <RVal T>
inline r_size_t n_unique(const r_vec<T>& x) {
    
    r_size_t n = x.length();
  
    // Hash set for O(n) de-duplication
    ankerl::unordered_dense::set<
      unwrap_t<T>, 
      internal::r_hash<T>, 
      internal::r_hash_eq<T>
    > seen;

    seen.reserve(internal::get_hash_map_reserve_size<T>(n));

    auto* RESTRICT p_x = x.data(); 
  
    for (r_size_t i = 0; i < n; ++i) {
      seen.insert(p_x[i]);
      // Since r_lgl can be either true, false or NA, we can safely return early if n_unique == 3
      if constexpr (is<T, r_lgl>){
        if (seen.size() == 3) return 3;
      }
    }
    return seen.size();
}

inline r_size_t n_unique(const r_factors& x) {
    return n_unique(x.value);
}

}

#endif
