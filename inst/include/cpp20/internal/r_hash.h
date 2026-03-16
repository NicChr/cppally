#ifndef CPP20_R_HASH_H
#define CPP20_R_HASH_H

#include <cpp20/internal/r_vec.h>
#include <cpp20/internal/r_visit.h>
#include <cpp20/internal/r_attrs.h>
#include <bit>
#include <algorithm>

// Hash functions + hash equality operators for RVal and RVector

namespace cpp20 {

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

template <RVal T>
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

template <typename T>
struct r_vec_hash_impl;

template <RVal T>
struct r_vec_hash_impl<r_vec<T>> {
    using is_avalanching = void;

    [[nodiscard]] size_t operator()(const r_vec<T>& x) const noexcept {
        uint64_t seed = 0;

        if (x.is_null()) return seed;

        r_size_t n = x.length();

        if (n == 0) return r_hash_impl<r_int>{}(r_typeof<r_vec<T>>);

        const auto* p_x = x.data();

        for (r_size_t i = 0; i < n; ++i) {
            seed = hash_combine(seed, r_hash_impl<T>{}(p_x[i]));
        }
        return seed;
    }
};

template<>
struct r_vec_hash_impl<r_factors> {
    using is_avalanching = void;

    [[nodiscard]] size_t operator()(const r_factors& x) const noexcept {
        uint64_t seed = 0;

        r_vec<r_int> codes = x.value;

        if (codes.is_null()) return seed;

        r_size_t n = codes.length();

        if (n == 0) return r_hash_impl<r_int>{}(r_typeof<r_factors>);

        const int* p_x = codes.data();

        for (r_size_t i = 0; i < n; ++i) {
            seed = hash_combine(seed, r_hash_impl<r_int>{}(p_x[i]));
        }
        return hash_combine(seed, r_vec_hash_impl<r_vec<r_str_view>>{}(x.levels()));
    }
};

// Specialization for lists
template<>
struct r_hash_impl<r_sexp> {
    using is_avalanching = void;

    [[nodiscard]] size_t operator()(SEXP x) const {

        auto x_ = r_sexp(x, internal::view_tag{});

        if (x_.is_null()) return 0;

        size_t seed = 0;

        int type = TYPEOF(x);
        
        // Recursively hash the element
        size_t h = visit_sexp(x_, [this, seed](const auto& vec) -> size_t {

            size_t seed_ = seed;

            using vec_t = std::remove_cvref_t<decltype(vec)>;

            if constexpr (is<vec_t, r_sexp>){
                abort("List contains unsupported element type, current implementation can only hash vectors and factors");
            } else if constexpr (is<vec_t, r_vec<r_sexp>>){
                r_size_t n = vec.length();
                for (r_size_t i = 0; i < n; ++i) {
                    size_t elem_hash = (*this)(vec.view(i));
                    seed_ = hash_combine(seed_, elem_hash);
                }
                return seed_;
            } else {
                return r_vec_hash_impl<vec_t>{}(vec);
            }
        });
        return hash_combine(h, static_cast<size_t>(type));
    }
};


template <RVal T>
struct r_hash_eq_impl {
    using is_transparent = void;
    bool operator()(const unwrap_t<T>& a, const unwrap_t<T>& b) const noexcept {
        return a == b;
    }
};

template <>
struct r_hash_eq_impl<r_dbl> {
    using is_transparent = void;
    bool operator()(double a, double b) const noexcept {
        if (is_na(a) && is_na(b)){
            return is_na_real(a) == is_na_real(b);
        } else {
            return a == b;
        }
    }
};

template <>
struct r_hash_eq_impl<r_cplx> {
    using is_transparent = void;
    bool operator()(std::complex<double> a, std::complex<double> b) const noexcept {
        r_hash_eq_impl<r_dbl> eq;
        return eq(a.real(), b.real()) && eq(a.imag(), b.imag());
    }
};

// Meant to be used for elements of lists
// Since they are r_sexp we have to 'visit' the type of vector it is
template<>
struct r_hash_eq_impl<r_sexp> {
    bool operator()(SEXP x, SEXP y) const {
    if (x == y) return true; // same pointer
    if (Rf_xlength(x) != Rf_xlength(y)) return false;
    if (TYPEOF(x) != TYPEOF(y)) return false;
    
    bool x_has_attrs = attr::has_attrs(r_sexp(x, internal::view_tag{}));
    bool y_has_attrs = attr::has_attrs(r_sexp(y, internal::view_tag{}));
    if (x_has_attrs != y_has_attrs) return false;
    
    if (x_has_attrs && y_has_attrs){
        if (!r_hash_eq_impl<r_sexp>{}(attr::get_attrs(r_sexp(x, internal::view_tag{})), attr::get_attrs(r_sexp(y, internal::view_tag{})))){
            return false;
        }
    }

    return visit_sexp(x, [y](auto vec1) -> bool {
        using vec_t = decltype(vec1);

        if constexpr (!RVector<vec_t>){
            abort("List contains non-vector element, current implementation can only hash vectors");
        } else {
            
            // View-only copy of y
            auto vec2 = vec_t(y, internal::view_tag{});

            if constexpr (is<vec_t, r_sexp>){
                abort("List contains unsupported element type, current implementation can only hash vectors and factors");
            } else if constexpr (is<vec_t, r_vec<r_sexp>>) {
                r_size_t n = vec1.length();
                for (r_size_t i = 0; i < n; ++i){
                    if (!r_hash_eq_impl<r_sexp>{}(vec1.view(i), vec2.view(i))){
                        return false;
                    }
                }
                return true;
            } else {
                using data_t = typename vec_t::data_type;
                return std::memcmp(vec1.data(), vec2.data(), vec1.size() * sizeof(unwrap_t<data_t>)) == 0; 
            }
        }
        });
    }
};

template <RVal T>
struct r_hash : r_hash_impl<std::remove_cvref_t<T>> {};
template <RVal T>
struct r_hash_eq : r_hash_eq_impl<std::remove_cvref_t<T>> {};


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

}

#endif
