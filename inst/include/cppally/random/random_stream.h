#ifndef CPPALLY_RANDOM_STREAM_H
#define CPPALLY_RANDOM_STREAM_H

#include <cppally/r_setup.h>
#include <cppally/utils.h> // for exp2
#include <cppally/math/portable_log.h>
#include <cppally/math/portable_exp.h>
#include <cppally/math/log_factorial.h>
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

  r_dbl uniform() noexcept {
    // Top 53 bits scaled into [0, 1) - exact, bit-reproducible as is
    return r_dbl(static_cast<double>(engine_() >> 11) * 0x1.0p-53);
  }
  
  r_dbl uniform(double a, double b) noexcept {
    if (!(a <= b)) [[unlikely]] {
      return r_dbl::na();
    }
    double u = uniform();
    return r_dbl(internal::fma(b, u, a * (1.0 - u)));
  }

  r_lgl bernoulli(double prob) noexcept {
    if (!(prob >= 0.0 && prob <= 1.0)) [[unlikely]] {
      return r_na;
    }
    double u = uniform();
    return r_lgl(u < prob);
  }

  // Ahrens & Dieter (1972) algorithm SA.
  r_dbl exponential() noexcept {

    // q[k - 1] = sum(log(2)^i / i!, i = 1, ..., k)
    static constexpr double q[16] = {
      0x1.62e42fefa39efp-1, 0x1.dde327edaeb53p-1, 0x1.fa4e30c4b355fp-1, 0x1.ff3adba0a1e99p-1,
      0x1.ffe99f9fde3ccp-1, 0x1.fffdd0310dc39p-1, 0x1.ffffcffd0a1c1p-1, 0x1.fffffc550e636p-1,
      0x1.ffffffbf58dddp-1, 0x1.fffffffbf2c80p-1, 0x1.ffffffffc45d9p-1, 0x1.fffffffffcd54p-1,
      0x1.ffffffffffd80p-1, 0x1.fffffffffffe3p-1, 0x1.fffffffffffffp-1, 0x1.0000000000000p+0
    };
    static_assert(q[15] == 1.0); // Guarantees the loop terminates

    double a = 0.0;
    double u = uniform();
    while (u == 0.0) {
      u = uniform();
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

    double umin = uniform();
    int i = 0;
    while (u > q[i]) {
      double ustar = uniform();
      if (ustar < umin) {
        umin = ustar;
      }
      ++i;
    }

    return r_dbl(internal::fma(umin, q[0], a));
  }

  r_dbl exponential(double rate) noexcept {
    if (r_dbl(rate).is_na()) [[unlikely]] {
      return r_dbl(rate);
    }
    return r_dbl(exponential() / rate);
  }

  // Bit-portable standard normal deviates
  // Half-normal by rejection from an Exp(1) envelope, then a random sign (von Neumann 1951; see Devroye 1986)
  r_dbl normal() noexcept {
    double x;
    double threshold;
    do {
      x = exponential();
      threshold = 0.5 * (x - 1.0) * (x - 1.0);
    } while (exponential() < threshold);

    return r_dbl((engine_() >> 63) ? -x : x);
  }

  r_dbl normal(double mean, double sd) noexcept {
    if (r_dbl(mean).is_na() || !(sd >= 0.0)) [[unlikely]] {
      return r_dbl::na();
    }
    double z = normal();
    return r_dbl(internal::fma(sd, z, mean));
  }

  // Log-normal deviates
  r_dbl lognormal() noexcept {
    return r_dbl(internal::portable_exp(normal()));
  }

  r_dbl lognormal(double meanlog, double sdlog) noexcept {
    double z = normal(meanlog, sdlog);
    return r_dbl(internal::portable_exp(z));
  }

  // Weibull deviates
  r_dbl weibull(double shape) noexcept {
    if (!(shape > 0.0)) [[unlikely]] {
      return r_dbl::na();
    }
    double e = exponential();
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
      u = 2.0 * unwrap(uniform()) - 1.0;
      v = 2.0 * unwrap(uniform()) - 1.0;
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
      double e = exponential();
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
  r_dbl chi_squared(double degrees_of_freedom) noexcept {
    return gamma(0.5 * degrees_of_freedom, 2.0);
  }

  // Student's t deviates: z / sqrt(chi_squared(df) / df)
  r_dbl student_t(double degrees_of_freedom) noexcept {
    if (!(degrees_of_freedom > 0.0)) [[unlikely]] {
      return r_dbl::na();
    }
    double z = normal();
    if (r_dbl(degrees_of_freedom).is_infinite()) {
      return r_dbl(z);
    }
    double c = chi_squared(degrees_of_freedom);
    return r_dbl(z / std::sqrt(c / degrees_of_freedom));
  }

  // F deviates (Fisher 1924, Snedecor 1934): chi-squared ratios
  r_dbl fisher_snedecor(double degrees_of_freedom1, double degrees_of_freedom2) noexcept {
    if (!(degrees_of_freedom1 > 0.0 && degrees_of_freedom2 > 0.0)) [[unlikely]] {
      return r_dbl::na();
    }
    r_dbl chi1 = chi_squared(degrees_of_freedom1);
    r_dbl chi2 = chi_squared(degrees_of_freedom2);

    double num = r_dbl(degrees_of_freedom1).is_infinite() ? 1.0 : unwrap(chi1) / degrees_of_freedom1;
    double den = r_dbl(degrees_of_freedom2).is_infinite() ? 1.0 : unwrap(chi2) / degrees_of_freedom2;
    return r_dbl(num / den);
  }

  // Beta deviates
  r_dbl beta(double shape1, double shape2) noexcept {
    if (!(shape1 >= 0.0 && shape2 >= 0.0)) [[unlikely]] {
      return r_dbl::na();
    }
    bool inf1 = r_dbl(shape1).is_infinite();
    bool inf2 = r_dbl(shape2).is_infinite();
    if (shape1 == 0.0 || shape2 == 0.0 || inf1 || inf2) [[unlikely]] {
      if (shape1 == shape2) {
        return r_dbl(shape1 == 0.0 ? static_cast<double>(engine_() >> 63) : 0.5);
      }
      return r_dbl(shape1 == 0.0 || inf2 ? 0.0 : 1.0);
    }
    if (shape1 < 1.0 && shape2 < 1.0) {
      double gx = gamma_mt(shape1 + 1.0);
      double ex = exponential();
      double gy = gamma_mt(shape2 + 1.0);
      double ey = exponential();
      return r_dbl(1.0 / internal::fma(gy / gx, internal::portable_exp(ex / shape1 - ey / shape2), 1.0));
    }
    double x = gamma(shape1);
    double y = gamma(shape2);
    return r_dbl(1.0 / (1.0 + y / x));
  }

  // Logistic deviates
  r_dbl logistic() noexcept {
    double u;
    do {
      u = uniform();
    } while (u == 0.0);
    return r_dbl(internal::portable_log(u / (1.0 - u)));
  }

  r_dbl logistic(double location, double scale) noexcept {
    if (r_dbl(location).is_na() || !(scale >= 0.0)) [[unlikely]] {
      return r_dbl::na();
    }
    double z = logistic();
    return r_dbl(internal::fma(scale, z, location));
  }

  // Poisson deviates
  // Knuth's multiplication method below 10, otherwise Hormann's (1993) PTRS transformed rejection
  r_int64 poisson(double lambda) noexcept {
    // Counts can't be super large
    if (!(lambda >= 0.0 && lambda < internal::exp2(62))) [[unlikely]] {
      return r_int64::na();
    }
    if (lambda < 10.0) {
      double threshold = internal::portable_exp(-lambda);
      double prod = uniform();
      int64_t k = 0;
      while (prod > threshold) {
        prod *= unwrap(uniform());
        ++k;
      }
      return r_int64(k);
    }
    double slam = std::sqrt(lambda);
    double log_lambda = internal::portable_log(lambda);
    double b = internal::fma(2.53, slam, 0.931);
    double a = internal::fma(0.02483, b, -0.059);
    double inv_alpha = 1.1239 + 1.1328 / (b - 3.4);
    double vr = 0.9277 - 3.6224 / (b - 2.0);
    while (true) {
      double u = unwrap(uniform()) - 0.5;
      double v = uniform();
      double us = 0.5 - std::fabs(u);
      double k = std::floor(internal::fma(2.0 * a / us + b, u, lambda + 0.43));
      if (us >= 0.07 && v <= vr) {
        return r_int64(static_cast<int64_t>(k));
      }
      if (k < 0.0 || (us < 0.013 && v > us)) {
        continue;
      }
      double log_ratio = internal::portable_log(v * inv_alpha / (a / (us * us) + b));
      if (log_ratio <= internal::fma(k, log_lambda, -lambda) - internal::log_factorial(k)) {
        return r_int64(static_cast<int64_t>(k));
      }
    }
  }

  // Negative binomial deviates
  r_int64 negative_binomial(double size, double prob) noexcept {
    if (!(size >= 0.0 && prob > 0.0 && prob <= 1.0)) [[unlikely]] {
      return r_int64::na();
    }
    if (prob == 1.0) {
      return r_int64(0);
    }
    double lambda = gamma(size, (1.0 - prob) / prob);
    return poisson(lambda);
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
      double x = normal();
      double xs = x + inv_c;
      if (xs <= 0.0) {
        continue;
      }
      double w = c * xs;
      double ww = w * w;
      double v = ww * w;
      double x2 = x * x;
      double u = uniform();

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
