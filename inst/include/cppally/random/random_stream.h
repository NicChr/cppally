#ifndef CPPALLY_RANDOM_STREAM_H
#define CPPALLY_RANDOM_STREAM_H

#include <cppally/r_setup.h>
#include <cppally/utils.h> // for exp2
#include <cppally/math/portable_log.h>
#include <cppally/math/portable_exp.h>
#include <cppally/math/fma.h>
#include <R_ext/Random.h>
#include <Xoshiro-cpp/XoshiroCpp.hpp> // xoshiro256++ (Ryo Suzuki, MIT)
#include <cmath>
#include <cstdint>
#include <limits>
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
// All random number generating members are bit-portable across different machines.
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

  // Returns a random index in [a, b] : b > a
  // Lemire's divisionless method along with ankerl's portable 128bit multiply
  // makes this fast, portable, and hence reproducible.
  r_int64 index(int64_t a, int64_t b) noexcept {

    if (r_int64(a).is_na() || b < a) [[unlikely]] {
      return r_int64::na();
    }

    uint64_t span = static_cast<uint64_t>(b) - static_cast<uint64_t>(a);
    return r_int64(static_cast<int64_t>(static_cast<uint64_t>(a) + bounded(span + 1u)));
  }

  r_dbl unif() noexcept {
    // Top 53 bits scaled into [0, 1) - exact, bit-reproducible as is
    return r_dbl(static_cast<double>(engine_() >> 11) * 0x1.0p-53);
  }
  
  r_dbl unif(double a, double b) noexcept {
    double u = unif(); 
    return r_dbl(internal::fma(b, u, a * (1.0 - u)));
  }

  r_lgl bernoulli(double prob) noexcept {
    if (!(prob >= 0.0 && prob <= 1.0)) [[unlikely]] {
      return r_na;
    }
    double u = unif();
    return r_lgl(u < prob);
  }

  // Ahrens & Dieter (1972) algorithm SA.
  r_dbl exp() noexcept {

    // q[k - 1] = sum(log(2)^i / i!, i = 1, ..., k)
    static constexpr double q[16] = {
      0x1.62e42fefa39efp-1, 0x1.dde327edaeb53p-1, 0x1.fa4e30c4b355fp-1, 0x1.ff3adba0a1e99p-1,
      0x1.ffe99f9fde3ccp-1, 0x1.fffdd0310dc39p-1, 0x1.ffffcffd0a1c1p-1, 0x1.fffffc550e636p-1,
      0x1.ffffffbf58dddp-1, 0x1.fffffffbf2c80p-1, 0x1.ffffffffc45d9p-1, 0x1.fffffffffcd54p-1,
      0x1.ffffffffffd80p-1, 0x1.fffffffffffe3p-1, 0x1.fffffffffffffp-1, 0x1.0000000000000p+0
    };
    static_assert(q[15] == 1.0); // Guarantees the loop terminates

    double a = 0.0;
    double u = unif();
    while (u == 0.0) {
      u = unif();
    }

    u += u;
    while (u <= 1.0) {
      a += q[0];
      u += u;
    }
    u -= 1.0;

    if (u <= q[0]) {
      return r_dbl(a + u);
    }

    double umin = unif();
    int i = 0;
    while (u > q[i]) {
      double ustar = unif();
      if (ustar < umin) {
        umin = ustar;
      }
      ++i;
    }

    return r_dbl(internal::fma(umin, q[0], a));
  }

  r_dbl exp(double rate) noexcept {
    return r_dbl(exp() / rate);
  }

  // Bit-portable standard normal deviates
  // Half-normal by rejection from an Exp(1) envelope, then a random sign (von Neumann 1951; see Devroye 1986)
  r_dbl norm() noexcept {
    double x;
    double threshold;
    do {
      x = exp();
      threshold = 0.5 * (x - 1.0) * (x - 1.0);
    } while (exp() < threshold);

    return r_dbl((engine_() >> 63) ? -x : x);
  }

  r_dbl norm(double mean, double sd) noexcept {
    if (r_dbl(mean).is_na() || !(sd >= 0.0)) [[unlikely]] {
      return r_dbl::na();
    }
    double z = norm();
    return r_dbl(internal::fma(sd, z, mean));
  }

  // Log-normal deviates
  r_dbl lnorm() noexcept {
    return r_dbl(internal::portable_exp(norm()));
  }

  r_dbl lnorm(double meanlog, double sdlog) noexcept {
    double z = norm(meanlog, sdlog);
    return r_dbl(internal::portable_exp(z));
  }

  // Weibull deviates
  r_dbl weibull(double shape) noexcept {
    if (!(shape > 0.0)) [[unlikely]] {
      return r_dbl::na();
    }
    double e = exp();
    return r_dbl(internal::portable_exp(internal::portable_log(e) / shape));
  }

  r_dbl weibull(double shape, double scale) noexcept {
    if (!(scale >= 0.0)) [[unlikely]] {
      return r_dbl::na();
    }
    double w = weibull(shape);
    return r_dbl(w * scale);
  }

  // Cauchy deviates: v / u for a uniform point (u, v) in the unit disc
  // Equivalent to cauchy(location = 0, scale = 1)
  r_dbl cauchy() noexcept {
    double u;
    double v;
    do {
      u = 2.0 * unwrap(unif()) - 1.0;
      v = 2.0 * unwrap(unif()) - 1.0;
    } while (u == 0.0 || internal::fma(u, u, v * v) > 1.0);
    return r_dbl(v / u);
  }

  r_dbl cauchy(double location, double scale) noexcept {
    if (r_dbl(location).is_na() || !(scale >= 0.0)) [[unlikely]] {
      return r_dbl::na();
    }
    double z = cauchy();
    return r_dbl(internal::fma(scale, z, location));
  }

  // gamma deviates
  // Marsaglia & Tsang (2000) for shape >= 1, boosted via gamma(shape + 1) * U^(1 / shape) below 1
  r_dbl gamma(double shape) noexcept {
    if (!(shape >= 0.0)) [[unlikely]] {
      return r_dbl::na();
    }
    if (shape == 0.0 || r_dbl(shape).is_infinite()) [[unlikely]] {
      return r_dbl(shape);
    }
    if (shape < 1.0) {
      double g = gamma_mt(shape + 1.0);
      double e = exp();
      return r_dbl(g * internal::portable_exp(-e / shape));
    }
    return r_dbl(gamma_mt(shape));
  }

  r_dbl gamma(double shape, double scale) noexcept {
    if (!(scale >= 0.0)) [[unlikely]] {
      return r_dbl::na();
    }
    double g = gamma(shape);
    return r_dbl(g * scale);
  }

  // Chi-squared deviates
  r_dbl chisq(double degrees_of_freedom) noexcept {
    return gamma(0.5 * degrees_of_freedom, 2.0);
  }

  // Student's t deviates: z / sqrt(chisq(df) / df)
  r_dbl student_t(double degrees_of_freedom) noexcept {
    if (!(degrees_of_freedom > 0.0)) [[unlikely]] {
      return r_dbl::na();
    }
    double z = norm();
    if (r_dbl(degrees_of_freedom).is_infinite()) {
      return r_dbl(z);
    }
    double c = chisq(degrees_of_freedom);
    return r_dbl(z / std::sqrt(c / degrees_of_freedom));
  }

  // F deviates (Fisher 1924, Snedecor 1934): chisq ratios
  r_dbl fisher_snedecor(double degrees_of_freedom1, double degrees_of_freedom2) noexcept {
    if (!(degrees_of_freedom1 > 0.0 && degrees_of_freedom2 > 0.0)) [[unlikely]] {
      return r_dbl::na();
    }
    r_dbl chi1 = chisq(degrees_of_freedom1);
    r_dbl chi2 = chisq(degrees_of_freedom2);

    double num = r_dbl(degrees_of_freedom1).is_infinite() ? 1.0 : unwrap(chi1) / degrees_of_freedom1;
    double den = r_dbl(degrees_of_freedom2).is_infinite() ? 1.0 : unwrap(chi2) / degrees_of_freedom2;
    return r_dbl(num / den);
  }

  // Logistic deviates
  r_dbl logis() noexcept {
    double u;
    do {
      u = unif();
    } while (u == 0.0);
    return r_dbl(internal::portable_log(u / (1.0 - u)));
  }

  r_dbl logis(double location, double scale) noexcept {
    if (r_dbl(location).is_na() || !(scale >= 0.0)) [[unlikely]] {
      return r_dbl::na();
    }
    double z = logis();
    return r_dbl(internal::fma(scale, z, location));
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
  // index()'s full-width case, where span + 1 wraps.
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

  // Marsaglia & Tsang (2000), shape >= 1
  double gamma_mt(double shape) noexcept {
    double d = shape - 1.0 / 3.0;
    double inv_c = std::sqrt(9.0 * d);
    double c = 1.0 / inv_c;
    while (true) {
      double x = norm();
      double xs = x + inv_c;
      if (xs <= 0.0) {
        continue;
      }
      double w = c * xs;
      double ww = w * w;
      double v = ww * w;
      double x2 = x * x;
      double u = unif();

      if (1.0 - u > 0.0331 * (x2 * x2)) {
        return d * v;
      }
      double bound = internal::fma(d, internal::fma(-ww, w, 1.0) + internal::portable_log(v), 0.5 * x2);
      if (internal::portable_log(u) < bound) {
        return d * v;
      }
    }
  }

  uint64_t seed_;
  engine_type engine_;

};

}

#endif
