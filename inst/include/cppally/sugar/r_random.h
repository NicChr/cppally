#ifndef CPPALLY_R_RANDOM_H
#define CPPALLY_R_RANDOM_H

#include <cppally/r_vec.h>
#include <R_ext/Random.h>
#include <random>

namespace cppally {

namespace internal {

struct rng_guard {
  rng_guard() { safe[GetRNGstate](); }
  ~rng_guard() { safe[PutRNGstate](); }
  rng_guard(const rng_guard&) = delete;
};

// Using R's seed, we create a new seed.
// Doing this ensures we can run reproducible code from R.
inline uint64_t draw_seed() {
  uint64_t hi = static_cast<uint64_t>(unif_rand() * internal::exp2<double>(32));
  uint64_t lo = static_cast<uint64_t>(unif_rand() * internal::exp2<double>(32));
  return (hi << 32) ^ lo;
}


// Runs `f` with R's RNG state loaded and writing it back at function exit.
template <typename F>
decltype(auto) with_rng(F&& f) {
  internal::rng_guard guard;
  return std::forward<F>(f)();
}

}

// A random stream seeded from R once. set.seed() still determines everything,
// but R's RNG is touched exactly once per object rather than once per draw
struct random_stream {

    using result_type = std::mt19937_64::result_type;

    random_stream() : random_stream(internal::with_rng([]{ return internal::draw_seed(); })) {}

    explicit random_stream(uint64_t seed) : seed_(seed), engine_(seed) {}

    // A copy would be an identical stream, which is never what a caller wants -
    // taking `random_stream` by value instead of `random_stream&` would silently produce two
    // generators yielding the same numbers. Use split() to branch
    random_stream(const random_stream&) = delete;
    random_stream& operator=(const random_stream&) = delete;
    random_stream(random_stream&&) = default;
    random_stream& operator=(random_stream&&) = default;

    // Modelling std::uniform_random_bit_generator means random_stream can be handed
    // straight to any <random> distribution or algorithm - std::shuffle,
    // std::normal_distribution and so on - without reaching for engine()
    static constexpr result_type min() { return std::mt19937_64::min(); }
    static constexpr result_type max() { return std::mt19937_64::max(); }
    result_type operator()() { return engine_(); }

    r_dbl unif(r_dbl a = r_dbl(0), r_dbl b = r_dbl(1)) {
        return r_dbl(std::uniform_real_distribution<double>(a, b)(engine_));
    }

    template <typename index_t>
    requires (any<index_t, int, r_size_t>)
    index_t index(index_t a, index_t b) {
        return std::uniform_int_distribution<index_t>(a, b)(engine_);
    }

    // The seed this stream started from. Log it to replay a run via random_stream(seed)
    uint64_t seed() const { return seed_; }

    // An independent child stream. R's RNG can't be touched from a worker
    // thread, so parallel work builds its streams up front by splitting
    random_stream split() { return random_stream(engine_()); }

    void discard(uint64_t n) { engine_.discard(n); }

    std::mt19937_64& engine() { return engine_; }

private:
    uint64_t seed_;
    std::mt19937_64 engine_;
};

// Out of scope for this header but you can implement R style vectorised sampling as shown below

// #include <ankerl/unordered_dense.h>

// // Sample indices with replacement.
// template <typename index_t>
// requires (any<index_t, int, r_size_t>)
// auto sample_indices_with_replacement(index_t n, index_t size, random_stream& r) {

//     using data_t = as_r_scalar_t<index_t>;

//     if (n <= 0) [[unlikely]] {
//         abort("population size must be > 0");
//     }

//     if (size < 0) [[unlikely]] {
//         abort("sample size must be >= 0");
//     }

//     if (size == 0) {
//         return r_vec<data_t>();
//     }

//     r_vec<data_t> out(size);
//     for (index_t i = 0; i < size; ++i){
//         out.set(i, data_t(r.index(0, n - 1)));
//     }
//     return out;
// }
// template <typename index_t>
// requires (any<index_t, int, r_size_t>)
// auto sample_indices_with_replacement(index_t n, index_t size) {
//     random_stream r;
//     return sample_indices_with_replacement(n, size, r);
// }

// // Sample indices without replacement.
// // Partial Fisher-Yates over an identity array that is never materialised.
// template <typename index_t>
// requires (any<index_t, int, r_size_t>)
// auto sample_indices_without_replacement(index_t n, index_t size, random_stream& r) {

//     using data_t = as_r_scalar_t<index_t>;

//     if (n <= 0) [[unlikely]] {
//         abort("population size must be > 0");
//     }

//     if (size < 0) [[unlikely]] {
//         abort("sample size must be >= 0");
//     }

//     if (size > n) [[unlikely]] {
//         abort("sample size must be <= population size when sampling without replacement");
//     }

//     if (size == 0) {
//         return r_vec<data_t>();
//     }

//     // position -> value, only where that value is no longer the identity
//     ankerl::unordered_dense::map<index_t, index_t> swapped;
//     swapped.reserve(static_cast<std::size_t>(size));

//     r_vec<data_t> out(size);

//     for (index_t i = 0; i < size; ++i){
//         index_t j = r.index(i, n - 1);

//         auto it_j = swapped.find(j);
//         index_t val_j = it_j == swapped.end() ? j : it_j->second;

//         auto it_i = swapped.find(i);
//         index_t val_i = it_i == swapped.end() ? i : it_i->second;

//         out.set(i, data_t(val_j));
//         // j is always drawn from [i, n), so position i is never read again
//         swapped[j] = val_i;
//     }
//     return out;
// }

// template <typename index_t>
// requires (any<index_t, int, r_size_t>)
// auto sample_indices_without_replacement(index_t n, index_t size) {
//     random_stream r;
//     return sample_indices_without_replacement(n, size, r);
// }

// template <RAtomicVector T>
// auto sample(const T& x, r_size_t size) {
//     r_vec<r_int> indices = sample_indices_with_replacement<int>(x.length(), size);
//     return pmap_parallel_simd([&x](r_int idx){ return x.view(static_cast<r_size_t>(unwrap(idx)));}, indices);
// }

}

#endif
