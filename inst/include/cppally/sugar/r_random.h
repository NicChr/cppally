#ifndef CPPALLY_R_RANDOM_H
#define CPPALLY_R_RANDOM_H

#include <cppally/r_vec.h>
#include <R_ext/Random.h>
#include <ankerl/unordered_dense.h> // wyhash::mum - portable 64x64 -> 128 multiply

namespace cppally {

namespace internal {

struct rng_guard {
  rng_guard() { safe[GetRNGstate](); }
  ~rng_guard() { PutRNGstate(); }
  rng_guard(const rng_guard&) = delete;
};

// Using R's seed, we create a new seed.
// This, in combination with `with_rng` ensures we can run reproducible code from R.
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

template <int k>
inline constexpr uint64_t rotl64(uint64_t x) noexcept {
  static_assert(k > 0 && k < 64, "rotate count must be in (0, 64)");
  return (x << k) | (x >> (64 - k));
}

// splitmix64 - expands one 64-bit seed into well-mixed state words
inline uint64_t splitmix64(uint64_t& x) noexcept {
  uint64_t z = (x += 0x9e3779b97f4a7c15ULL);
  z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
  z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
  return z ^ (z >> 31);
}

// xoshiro256++ (Blackman & Vigna, public domain).
//
// Written out rather than using <random> so cppally pulls in no random number
// library: Writing R Extensions, "Portable C and C++ code", asks compiled code
// not to use the C++11 random number library.
//
struct xoshiro256pp {

  using result_type = uint64_t;

  explicit xoshiro256pp(uint64_t seed) {
    uint64_t x = seed;
    for (uint64_t& si : s) {
      si = splitmix64(x);
    }
  }

  static constexpr result_type min() { return 0; }
  static constexpr result_type max() { return ~result_type{0}; }

  result_type operator()() {
    const uint64_t result = rotl64<23>(s[0] + s[3]) + s[0];
    const uint64_t t = s[1] << 17;

    s[2] ^= s[0];
    s[3] ^= s[1];
    s[1] ^= s[2];
    s[0] ^= s[3];
    s[2] ^= t;
    s[3] = rotl64<45>(s[3]);

    return result;
  }

  // O(n) - there is no skip-ahead for an arbitrary n. xoshiro's jump() advances
  // by a fixed 2^128, which is a tool for splitting streams, not for this
  void discard(uint64_t n) {
    while (n-- > 0) {
      (*this)();
    }
  }

private:
  uint64_t s[4];
};

}

// A random stream seeded from R once. set.seed() still determines everything,
// but R's RNG is touched exactly once per object rather than once per draw
struct random_stream {

    using result_type = internal::xoshiro256pp::result_type;

    random_stream() : random_stream(internal::with_rng([]{ return internal::draw_seed(); })) {}

    explicit random_stream(uint64_t seed) : seed_(seed), engine_(seed) {}

    // Neither copyable nor movable: both would duplicate a position, giving two
    // generators that yield the same numbers while looking independent. 
    // split() is the way to branch.

    random_stream(const random_stream&) = delete;
    random_stream& operator=(const random_stream&) = delete;
    random_stream(random_stream&&) = delete;
    random_stream& operator=(random_stream&&) = delete;

    // Modelling std::uniform_random_bit_generator means random_stream can be handed
    // straight to any <random> distribution or algorithm - std::shuffle,
    // std::normal_distribution and so on - without reaching for engine()
    static constexpr result_type min() { return internal::xoshiro256pp::min(); }
    static constexpr result_type max() { return internal::xoshiro256pp::max(); }
    result_type operator()() { return engine_(); }

    // A whole number in [0, range), or the full 64-bit range when `range` is 0.
    // Lemire's method, over ankerl's portable 128-bit multiply.
    uint64_t bounded(uint64_t range) {
        if (range == 0) [[unlikely]] {
            return engine_();
        }

        uint64_t lo = engine_();
        uint64_t hi = range;
        ankerl::unordered_dense::detail::wyhash::mum(&lo, &hi);

        if (lo < range) {
            uint64_t threshold = (~range + 1) % range; // (2^64 - range) % range
            while (lo < threshold) {
                lo = engine_();
                hi = range;
                ankerl::unordered_dense::detail::wyhash::mum(&lo, &hi);
            }
        }
        return hi;
    }

    r_dbl unif(r_dbl a = r_dbl(0), r_dbl b = r_dbl(1)) {
        // Top 53 bits scaled into [0, 1)
        double u = static_cast<double>(engine_() >> 11) * 0x1.0p-53;
        return r_dbl(unwrap(a) * (1.0 - u) + unwrap(b) * u);
    }

    template <typename index_t>
    requires (any<index_t, int, r_size_t>)
    index_t index(index_t a, index_t b) {

        if (b < a) [[unlikely]] {
            abort("`index()`: upper bound must be >= lower bound");
        }

        // Width in unsigned arithmetic so a negative `a` wraps correctly
        uint64_t range = static_cast<uint64_t>(b) - static_cast<uint64_t>(a) + 1;
        return static_cast<index_t>(static_cast<uint64_t>(a) + bounded(range));
    }

    // The seed this stream started from. Log it to replay a run via random_stream(seed)
    uint64_t seed() const { return seed_; }

    // An independent child stream. R's RNG can't be touched from a worker
    // thread, so parallel work builds its streams up front by splitting
    random_stream split() { return random_stream(engine_()); }

    void discard(uint64_t n) { engine_.discard(n); }

    internal::xoshiro256pp& engine() { return engine_; }

private:
    uint64_t seed_;
    internal::xoshiro256pp engine_;
};

// Out of scope for this header but you can implement R style vectorised sampling as shown below

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
// auto sample(const T& x, r_size_t size, bool replace) {
//     r_vec<r_int> indices = replace ? sample_indices_with_replacement<int>(x.length(), size) : sample_indices_without_replacement<int>(x.length(), size);
//     return pmap_parallel_simd([&x](r_int idx){ return x.view(static_cast<r_size_t>(unwrap(idx)));}, indices);
// }

}

#endif
