#ifndef CPPALLY_DRAW_PARALLEL_H
#define CPPALLY_DRAW_PARALLEL_H

#include <cppally/r_concepts.h>
#include <cppally/random/random_stream.h>
#include <cppally/utils.h>
#include <algorithm>
#include <type_traits>
#include <utility>
#include <vector>

namespace cppally {

namespace internal {

template <typename F>
requires (std::invocable<const F&, random_stream&, r_size_t>)
inline void draw_parallel_for(uint64_t seed, r_size_t n, const F& f) {
  
  static_assert(noexcept(f(std::declval<random_stream&>(), r_size_t(0))),
                "draw_parallel_for(): f runs inside an OpenMP region, so it must be noexcept");

  // Draws per split stream - changing this affects reproducibility!
  constexpr r_size_t chunk = r_size_t(1) << 12; // 2^12
  r_size_t n_chunks = n > 0 ? (n + chunk - 1) / chunk : 0;

  random_stream r(seed);

  std::vector<random_stream> streams;
  streams.reserve(static_cast<std::size_t>(n_chunks));
  for (r_size_t c = 0; c < n_chunks; ++c) {
    streams.push_back(r.split());
  }

  int n_threads = static_cast<int>(std::min<r_size_t>(internal::calc_threads(n), n_chunks));
  if (n_threads > 1){
    OMP_PARALLEL_FOR(n_threads)
    for (r_size_t c = 0; c < n_chunks; ++c) {
      random_stream& rs = streams[c];
      r_size_t chunk_start = c * chunk;
      r_size_t chunk_end = std::min(n, chunk_start + chunk);
      for (r_size_t i = chunk_start; i < chunk_end; ++i) {
        f(rs, i);
      }
    }
  } else {
    for (r_size_t c = 0; c < n_chunks; ++c) {
      random_stream& rs = streams[c];
      r_size_t chunk_start = c * chunk;
      r_size_t chunk_end = std::min(n, chunk_start + chunk);
      for (r_size_t i = chunk_start; i < chunk_end; ++i) {
        f(rs, i);
      }
    }
  }
}

}

// Multi-threaded random draws.
// Fills out[i] = f(stream, i) for i in [0, n) across multiple threads.
// Each fixed-width chunk of indices draws from its own random_stream.
// Child random_streams are split from `r` on the main thread.
// Results do NOT depend on the number of threads, hence single-threaded results always match multi-threaded results.
// Example usage:
// After threads set with cppally::set_threads()
// r_vector<r_dbl> uniform_samples(n);
// draw_parallel_into(uniform_samples.data(), n, [](random_stream& r, r_size_t) noexcept {
//   return r.uniform();
// });
// return uniform_samples;

template <typename T, typename F>
requires (!std::is_const_v<T> && std::invocable<const F&, random_stream&, r_size_t>)
inline void draw_parallel_into(uint64_t seed, T* target, r_size_t n, const F& f) {
  static_assert(noexcept(f(std::declval<random_stream&>(), r_size_t(0))),
                "draw_parallel_into(): f runs inside an OpenMP region, so it must be noexcept");
  internal::draw_parallel_for(seed, n, [&](random_stream& rs, r_size_t i) noexcept {
    target[i] = unwrap(f(rs, i));
  });
}

template <typename T, typename F>
requires (!std::is_const_v<T> && std::invocable<const F&, random_stream&, r_size_t>)
inline void draw_parallel_into(T* target, r_size_t n, const F& f) {
  draw_parallel_into(random_stream().seed(), target, n, f);
}

// template <typename T, typename F>
// requires (RVectorisable<T> && !std::is_const_v<T> && std::invocable<const F&, random_stream&, r_size_t>)
// inline void draw_parallel_into(r_vec<T>& target, const F& f) {
//   target.maybe_ensure_exclusive();
//   draw_parallel_into(random_stream().seed(), target.data(), target.length(), f);
// }

}

#endif
