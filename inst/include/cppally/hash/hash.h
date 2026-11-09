#ifndef CPPALLY_R_HASH_H
#define CPPALLY_R_HASH_H

#include <cppally/scalar/hash_scalars.h>
#include <cppally/vector/r_vector.h>
#include <cppally/r_sexp/visit.h>
#include <cppally/attributes/attributes.h>
#include <cppally/identical.h>
#include <cppally/random/random_stream.h>
#include <algorithm>
#include <cmath>
#include <ankerl/unordered_dense.h> // Hash maps for group IDs + unique + match

// Hash functions + hash equality operators for RVal and RVector

namespace cppally {

namespace internal {

// Forward declarations

inline uint64_t r_hash_impl(const r_sexp& x);

inline uint64_t hash_attrs(const r_sexp& x, uint64_t seed);

template <RVector T>
uint64_t r_hash_impl(const T& x);

inline uint64_t r_hash_impl(const r_factors& x);

inline uint64_t r_hash_impl(const r_df& x);

template <typename T>
struct r_hash_fn_impl {
    using is_avalanching = void; // Tells ankerl this is already a good quality hash
    using is_transparent = void;
    // For hash map memory efficiency we use the underlying type
    using base_t = unwrap_t<T>;
    uint64_t operator()(const base_t& x) const noexcept(noexcept(r_hash_impl(internal::unsafe_reconstruct_view<T>(x)))) {
        return r_hash_impl(internal::unsafe_reconstruct_view<T>(x));
    }
};

// Hash equality

template <typename T>
struct r_hash_eq_impl {

    using is_transparent = void;
    using base_t = unwrap_t<T>;

    bool operator()(const base_t& a, const base_t& b) const noexcept(noexcept(identical(internal::unsafe_reconstruct_view<T>(a), internal::unsafe_reconstruct_view<T>(b)))) {
        return identical(internal::unsafe_reconstruct_view<T>(a), internal::unsafe_reconstruct_view<T>(b));
    }
};

template <typename T>
using r_hash_fn = r_hash_fn_impl<r_base_scalar_t<T>>;
template <typename T>
using r_hash_eq = r_hash_eq_impl<r_base_scalar_t<T>>;

template <RVector T>
uint64_t r_hash_impl(const T& x) {
        
    if (x.is_null()) return 0;
    r_size_t n = x.length();
    // Initialise the seed using the hashed vector type
    uint64_t seed = r_hash_impl(r_int(static_cast<int>(r_typeof<T>)));
    // Hash the attributes list if it exists
    if (attr::has_attrs(x)){
        seed = hash_attrs(static_cast<r_sexp>(x), seed);
    }
    // Recursively combine hashes of elements (even if elements are vectors)
    for (r_size_t i = 0; i < n; ++i) {
        seed = hash_combine(seed, r_hash_impl(x.view(i)));
    }
    return seed;
};

inline uint64_t hash_attrs(const r_sexp& x, uint64_t seed) {
    
    r_vec<r_sexp> attrs = attr::get_attrs(x);

    seed = hash_combine(seed, r_hash_impl(attrs.names()));
    r_size_t n = attrs.length();
    for (r_size_t i = 0; i < n; ++i){
        seed = hash_combine(seed, r_hash_impl(attrs.view(i)));
    }
    return seed;
}

inline uint64_t r_hash_impl(const r_factors& x) {
    return r_hash_impl(x.value);
};

inline uint64_t r_hash_impl(const r_df& x) {
    return r_hash_impl(x.value);
};

inline uint64_t r_hash_impl(const r_sexp& x) {
    return r_sexp_view(x, CPPALLY_MAKE_VISITOR(uint64_t, v, r_hash_impl(v)));
}

// An extension of Chao's estimator of population size based on the first three capture frequency counts
// doi:10.1016/j.csda.2011.01.017
template <
    typename key,
    typename storage,
    typename hash,
    typename comparator
>
inline uint64_t unique_count_estimate(const key *px, uint64_t data_size){
    
    if (data_size == 0){
        return 0;
    }

    // Bigger sample size mainly reduces skew bias and variance
    uint64_t sample_size = static_cast<uint64_t>(std::sqrt(2.0 * data_size)) + 1u;

    // Setup RNG engine using custom seed
    random_stream rs(mix_u64(data_size));

    ankerl::unordered_dense::map<key, storage, hash, comparator> counts;
    counts.reserve(sample_size);

    uint64_t f1 = 0;
    uint64_t f2 = 0;
    uint64_t f3 = 0;

    for (uint64_t i = 0; i < sample_size; ++i) {

        r_size_t sample_pick = rs.index(0, static_cast<r_size_t>(data_size) - 1);
        storage& count = counts[px[sample_pick]];
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

    if (f2 > 5 && f3 > 5) {
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

    // Logical vectors can only have at most 3 unique elements
    if constexpr (is<T, r_vec<r_lgl>>){
        return 8;
    } else {

        using data_t = typename T::data_type;

        // Just a guess (nothing informing this)
        if (data_size < static_cast<uint64_t>(internal::exp2(16))){
            return data_size / 4;
        }

        // Some adhoc benchmarks indicate that over-reserving for low cardinality data is faster when using ankerl::unordered_dense::map
        // For high cardinality, we cap it to the data size
        return std::min(data_size, 2 * unique_count_estimate<U, int, r_hash_fn<data_t>, r_hash_eq<data_t>>(px, data_size));
    }
}

}

}

#endif
