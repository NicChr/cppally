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

}

// Runs `f` with R's RNG state loaded and writing it back at function exit.
template <typename F>
decltype(auto) with_rng(F&& f) {
  internal::rng_guard guard;
  return std::forward<F>(f)();
}

// Sample indices with replacement.
template <typename index_t>
requires (any<index_t, int, r_size_t>)
auto sample_indices_with_replacement(index_t n, index_t size, uint64_t seed) {

    using data_t = as_r_scalar_t<index_t>;

    if (n <= 0) [[unlikely]] {
        abort("population size must be > 0");
    }

    if (size < 0) [[unlikely]] {
        abort("sample size must be >= 0");
    }

    if (size == 0) {
        return r_vec<data_t>();
    }

    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<index_t> dist(0, n - 1);

    r_vec<data_t> out(size);
    for (index_t i = 0; i < size; ++i){
        out.set(i, data_t(dist(rng)));
    }
    return out;
}
template <typename index_t>
requires (any<index_t, int, r_size_t>)
auto sample_indices_with_replacement(index_t n, index_t size) {
    uint64_t seed = with_rng([]{ return internal::draw_seed(); });
    return sample_indices_with_replacement(n, size, seed);
}

}

#endif
