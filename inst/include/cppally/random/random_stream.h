#ifndef CPPALLY_RANDOM_STREAM_H
#define CPPALLY_RANDOM_STREAM_H

#include <cppally/r_setup.h>
#include <cppally/utils.h> // for exp2
#include <R_ext/Random.h>
#include <Xoshiro-cpp/XoshiroCpp.hpp> // xoshiro256++ (Ryo Suzuki, MIT)
#include <cmath>
#include <cstdint>
#if defined(_MSC_VER) && defined(_M_X64)
#include <intrin.h> // _umul128
#endif

namespace cppally {

namespace internal {

// Taken from ankerl/unordered_dense.h
// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// SPDX-License-Identifier: MIT
// Copyright (c) 2022 Martin Leitner-Ankerl <martin.ankerl@gmail.com>
inline void mum(std::uint64_t* a, std::uint64_t* b) noexcept {
#if defined(__SIZEOF_INT128__)
  __uint128_t r = *a;
  r *= *b;
  *a = static_cast<std::uint64_t>(r);
  *b = static_cast<std::uint64_t>(r >> 64U);
#elif defined(_MSC_VER) && defined(_M_X64)
  *a = _umul128(*a, *b, b);
#else
  std::uint64_t ha = *a >> 32U;
  std::uint64_t hb = *b >> 32U;
  std::uint64_t la = static_cast<std::uint32_t>(*a);
  std::uint64_t lb = static_cast<std::uint32_t>(*b);
  std::uint64_t hi{};
  std::uint64_t lo{};
  std::uint64_t rh = ha * hb;
  std::uint64_t rm0 = ha * lb;
  std::uint64_t rm1 = hb * la;
  std::uint64_t rl = la * lb;
  std::uint64_t t = rl + (rm0 << 32U);
  auto c = static_cast<std::uint64_t>(t < rl);
  lo = t + (rm1 << 32U);
  c += static_cast<std::uint64_t>(lo < t);
  hi = rh + (rm0 >> 32U) + (rm1 >> 32U) + c;
  *a = lo;
  *b = hi;
#endif
}

struct rng_guard {
  rng_guard() { safe[GetRNGstate](); }
  ~rng_guard() { PutRNGstate(); }
  rng_guard(const rng_guard&) = delete;
};

// Using R's seed, we create a new seed.
// This, in combination with `draw_from_r` ensures we can run reproducible code from R.
inline uint64_t draw_seed() {
  return unwind_protect([]{
    uint64_t hi = static_cast<uint64_t>(unif_rand() * exp2(32));
    uint64_t lo = static_cast<uint64_t>(unif_rand() * exp2(32));
    return (hi << 32) ^ lo;
  });
}

}


// Runs `f` with R's RNG state loaded, writing it back on exit. 
// You can safely call R's C API RNG functions with this.
template <typename F>
decltype(auto) draw_from_r(F&& f) {
  internal::rng_guard guard;
  return std::forward<F>(f)();
}

// A random stream seeded from R once. set.seed() still determines everything,
// but R's RNG is touched once per object - two unif_rand() draws - rather than
// once per draw.
//
// Drives xoshiro256++ (Blackman & Vigna, public domain) via the bundled
// Xoshiro-cpp. Not <random>: Writing R Extensions, "Portable C and C++ code",
// asks compiled code not to use the C++11 random number library
struct random_stream {

  using engine_type = XoshiroCpp::Xoshiro256PlusPlus;
  using result_type = engine_type::result_type;

  random_stream() : random_stream(draw_from_r([]{ return internal::draw_seed(); })) {}

  explicit random_stream(uint64_t seed) noexcept : seed_(seed), engine_(seed) {}

  // Not copyable as this could yield 2 generators that produce the same numbers while looking independent. 
  // split() is the way to branch.
  random_stream(const random_stream&) = delete;
  random_stream& operator=(const random_stream&) = delete;
  random_stream(random_stream&&) = default;
  random_stream& operator=(random_stream&&) = default;

  // Modelling std::uniform_random_bit_generator means random_stream can be handed
  // straight to any <random> distribution or algorithm - std::shuffle,
  // std::normal_distribution and so on
  static constexpr result_type min() noexcept { return engine_type::min(); }
  static constexpr result_type max() noexcept { return engine_type::max(); }
  result_type operator()() noexcept { return engine_(); }

  r_dbl unif() noexcept {
    // Top 53 bits scaled into [0, 1) - exact, bit-reproducible as is
    return r_dbl(static_cast<double>(engine_() >> 11) * 0x1.0p-53);
  }
  
  r_dbl unif(double a, double b) noexcept {
    double u = unif(); 
    return r_dbl(std::fma(b, u, a * (1.0 - u)));
  }

  // Returns a random index in [a, b] : b > a
  // Lemire's divisionless method along with ankerl's portable 128bit multiply
  // makes this fast, portable, and hence reproducible.
  r_int64 index(int64_t a, int64_t b) noexcept {

    if (b < a) [[unlikely]] {
      return r_int64::na();
    }

    uint64_t span = static_cast<uint64_t>(b) - static_cast<uint64_t>(a);
    return r_int64(static_cast<int64_t>(static_cast<uint64_t>(a) + bounded(span + 1u)));
  }

  // The seed this stream started from. Log it to replay a run via random_stream(seed)
  uint64_t seed() const noexcept { return seed_; }

  // An independent child stream. R's RNG can't be touched from a worker
  // thread, so parallel work builds its streams up front by splitting
  random_stream split() noexcept { return random_stream(engine_()); }

  // O(n) - there is no skip-ahead for an arbitrary n. xoshiro's jump() advances
  // by a fixed 2^128, which is a tool for splitting streams, not for this
  void discard(uint64_t n) noexcept {
    while (n-- > 0) {
      engine_();
    }
  }

  private:

  // A whole number in [0, range). `range == 0` means the full 64-bit range -
  // index()'s full-width case, where span + 1 wraps. Load-bearing: without it
  // mum() yields 0 and every full-width draw would return the lower bound.
  // Lemire's method, over ankerl's portable 128-bit multiply.
  uint64_t bounded(uint64_t range) noexcept {
      if (range == 0) [[unlikely]] {
          return engine_();
      }
  
      uint64_t lo = engine_();
      uint64_t hi = range;
      internal::mum(&lo, &hi);
  
      if (lo < range) {
          uint64_t threshold = (~range + 1) % range; // (2^64 - range) % range
          while (lo < threshold) {
              lo = engine_();
              hi = range;
              internal::mum(&lo, &hi);
          }
      }
      return hi;
  }
  
  uint64_t seed_;
  engine_type engine_;

};

}

#endif
