#ifndef CPPALLY_R_PMAP_H
#define CPPALLY_R_PMAP_H

#include <cppally/vector/r_vector.h>
#include <array>
#include <utility>

// pmap is a powerful utility for vectorising scalar C++ functions.
// It executes user-supplied lambdas (optionally with SIMD instructions and/or multiple parallel threads) across all elements of the supplied vectors.
// If vectors do not share the same length, elements are traversed by recycling through the shorter vectors' elements. If any vectors are empty (0-length), the output vector will also be empty.
// Please note that SIMD/multiple threads are ALWAYS disabled for some types (concept: RVectorisable) like r_str and r_sexp, even when explicitly requested.
// Author: Nick Christofides
// License: MIT
// Year: 2026

#define CPPALLY_OMP_DISPATCH(body, n, parallel, simd)    \
  if constexpr (parallel){                               \
    const int n_threads = internal::calc_threads(n);     \
    if constexpr (simd){                                 \
      if (n_threads > 1){                                \
        OMP_PARALLEL_FOR_SIMD(n_threads)                 \
        for (r_size_t i = 0; i < n; ++i) body(i);        \
      } else {                                           \
        OMP_SIMD                                         \
        for (r_size_t i = 0; i < n; ++i) body(i);        \
      }                                                  \
    } else {                                             \
      if (n_threads > 1){                                \
        OMP_PARALLEL_FOR(n_threads)                      \
        for (r_size_t i = 0; i < n; ++i) body(i);        \
      } else {                                           \
        for (r_size_t i = 0; i < n; ++i) body(i);        \
      }                                                  \
    }                                                    \
  } else {                                               \
    OMP_SIMD                                             \
    for (r_size_t i = 0; i < n; ++i) body(i);            \
  }

namespace cppally {

namespace internal {

// Output vector length
template <std::size_t K>
r_size_t pmap_extent(const std::array<r_size_t, K>& lens, bool& recycle) noexcept {
  static_assert(K > 0, "pmap_extent: needs at least one input length");
  r_size_t n = lens[0];
  for (r_size_t l : lens) {
    if (l == 0){
      return 0;
    }
    if (n != l){
      n = std::max(n, l);
      recycle = true;
    }
  }
  return n;
}

template <bool simd = false, bool parallel = false, typename F, RVal T>
  requires std::invocable<F&, r_size_t, T>
auto pmap_impl(F fn, const r_vec<T>& vec) {

  using out_t = std::remove_cvref_t<std::invoke_result_t<F&, r_size_t, T>>;
  static_assert(RVal<out_t>, "pmap: output type is not storable in r_vec");

  const r_size_t n = vec.length();

  if (n == 0){
    return r_vec<out_t>();
  }

  r_vec<out_t> out(n);

  constexpr bool vectorisable_or_parallelisable = RVectorisable<T> && RVectorisable<out_t>;

  if constexpr (vectorisable_or_parallelisable && (simd || parallel)) {

    [&, n](auto* RESTRICT p_out, auto* RESTRICT p_x){

      auto body = [&fn, p_out, p_x](r_size_t i){
        p_out[i] = unwrap(fn(i, internal::unsafe_reconstruct_view<T>(p_x[i])));
      };

      CPPALLY_OMP_DISPATCH(body, n, parallel, simd)

    }(out.data(), vec.data());

  } else {
    for (r_size_t i = 0; i < n; ++i) out.set(i, fn(i, vec.view(i)));
  }
  return out;
}

template <bool simd = false, bool parallel = false, typename F, RVal T, RVal U>
  requires (std::invocable<F&, r_size_t, T, U>)
auto pmap_impl(F fn, const r_vec<T>& vec1, const r_vec<U>& vec2) {

  using out_t = std::remove_cvref_t<std::invoke_result_t<F&, r_size_t, T, U>>;
  static_assert(RVal<out_t>, "pmap: output type is not storable in r_vec");

  bool recycle = false;
  // Check that all vectors are of the same length
  const std::array<r_size_t, 2> lens{ vec1.length(), vec2.length() };
  const r_size_t n = pmap_extent(lens, recycle);

  if (n == 0){
    return r_vec<out_t>();
  }

  if (recycle && (lens[0] == 1 || lens[1] == 1)){
    r_vec<out_t> out(n);

    if (lens[0] == 1){ // If LHS is a scalar
      const T val = vec1.get(0);
      if constexpr (RVectorisable<U> && RVectorisable<out_t> && (simd || parallel)){
        [&, n](auto* RESTRICT p_out, auto* RESTRICT p_x){
          
          auto body = [&fn, val, p_out, p_x](r_size_t i){
            p_out[i] = unwrap(fn(i, val, internal::unsafe_reconstruct_view<U>(p_x[i])));
          };
          
          CPPALLY_OMP_DISPATCH(body, n, parallel, simd)

        }(out.data(), vec2.data());
      } else {
        for (r_size_t i = 0; i < n; ++i) out.set(i, fn(i, val, vec2.view(i)));
      }
    } else { // RHS is a scalar
      const U val = vec2.get(0);
      if constexpr (RVectorisable<T> && RVectorisable<out_t> && (simd || parallel)){
        [&, n](auto* RESTRICT p_out, auto* RESTRICT p_x){
          
          auto body = [&fn, val, p_out, p_x](r_size_t i){
            p_out[i] = unwrap(fn(i, internal::unsafe_reconstruct_view<T>(p_x[i]), val));
          };

          CPPALLY_OMP_DISPATCH(body, n, parallel, simd)

        }(out.data(), vec1.data());
      } else {
        for (r_size_t i = 0; i < n; ++i) out.set(i, fn(i, vec1.view(i), val));
      }
    }
    return out;
  }

  r_vec<out_t> out(n);

  if (recycle){
    // Can't use SIMD or multiple threads here. Per-vector counters wrap via recycle_index
    // (no div), each paired with its own length.
    r_size_t j1 = 0;
    r_size_t j2 = 0;
    for (r_size_t i = 0; i < n; recycle_index(j1, lens[0]), recycle_index(j2, lens[1]), ++i){
      out.set(i, fn(i, vec1.view(j1), vec2.view(j2)));
    }
    return out;
  }

  constexpr bool vectorisable_or_parallelisable = RVectorisable<T> && RVectorisable<U> && RVectorisable<out_t>;

  if constexpr (vectorisable_or_parallelisable && (simd || parallel)) {

    [&, n](auto* RESTRICT p_out, auto* RESTRICT p_x1, auto* RESTRICT p_x2){
      
      auto body = [&fn, p_out, p_x1, p_x2](r_size_t i){
        p_out[i] = unwrap(fn(i, internal::unsafe_reconstruct_view<T>(p_x1[i]), internal::unsafe_reconstruct_view<U>(p_x2[i])));
      };

      CPPALLY_OMP_DISPATCH(body, n, parallel, simd)

    }(out.data(), vec1.data(), vec2.data());

  } else {
    for (r_size_t i = 0; i < n; ++i) out.set(i, fn(i, vec1.view(i), vec2.view(i)));
  }
  return out;
}

template <bool simd = false, bool parallel = false, typename F, RVal... Ts>
  requires (std::invocable<F&, r_size_t, Ts...> && sizeof...(Ts) != 1 && sizeof...(Ts) != 2) // 1 and 2 are handled above
auto pmap_impl(F fn, const r_vec<Ts>&... vecs) {

  constexpr int n_vecs = sizeof...(Ts);

  using out_t = std::remove_cvref_t<std::invoke_result_t<F&, r_size_t, Ts...>>;
  static_assert(RVal<out_t>, "pmap: output type is not storable in r_vec");

  if constexpr (n_vecs == 0) {
    return r_vec<out_t>();
  } else {
    bool recycle = false;
    // Check that all vectors are of the same length
    const std::array<r_size_t, n_vecs> lens{ vecs.length()... };
    const r_size_t n = pmap_extent(lens, recycle);

    if (n == 0){
      return r_vec<out_t>();
    }

    r_vec<out_t> out(n);

    if (recycle){
      // Can't use SIMD or multiple threads here. Per-vector counters wrap via recycle_index
      // (no div), each paired with its own lens[Is].
      [&]<std::size_t... Is>(std::index_sequence<Is...>){
        std::array<r_size_t, n_vecs> j{};
        for (r_size_t i = 0; i < n; (recycle_index(j[Is], lens[Is]), ...), ++i){
          out.set(i, fn(i, vecs.view(j[Is])...));
        }
      }(std::index_sequence_for<Ts...>{});
      return out;
    }

    constexpr bool vectorisable_or_parallelisable = (RVectorisable<Ts> && ...) && RVectorisable<out_t>;

    if constexpr (vectorisable_or_parallelisable && (simd || parallel)) {
      
      [&, n](auto* RESTRICT p_out, auto* RESTRICT ... ps){
        
        auto body = [&fn, p_out, ps...](r_size_t i){
          p_out[i] = unwrap(fn(i, internal::unsafe_reconstruct_view<Ts>(ps[i])...));
        };
        
        CPPALLY_OMP_DISPATCH(body, n, parallel, simd)

      }(out.data(), vecs.data()...);
    } else {
      for (r_size_t i = 0; i < n; ++i) out.set(i, fn(i, vecs.view(i)...));
    }
    return out;
  }
}

}

// map m x n vectors to 1 x n output by applying a user function: fn(x0, x1, x2, ..., xn)
template <typename F, RVal... Ts>
  requires std::invocable<F&, Ts...>
auto pmap(F fn, const r_vec<Ts>&... vecs) {
  return internal::pmap_impl<false>([&](r_size_t, Ts... vs){ return fn(vs...); }, vecs...);
}

// map m x n vectors to 1 x n output by applying a user function: fn(r_size_t index, x0, x1, x2, ..., xn)
template <typename F, RVal... Ts>
  requires std::invocable<F&, r_size_t, Ts...>
auto pmap_with_index(F fn, const r_vec<Ts>&... vecs) {
  return internal::pmap_impl<false>(fn, vecs...);
}

template <typename F, RVal... Ts>
  requires std::invocable<F&, r_size_t, Ts...>
auto pmap_simd_with_index(F fn, const r_vec<Ts>&... vecs) {
  return internal::pmap_impl<true>(fn, vecs...);
}

template <typename F, RVal... Ts>
  requires std::invocable<F&, r_size_t, Ts...>
auto pmap_parallel_with_index(F fn, const r_vec<Ts>&... vecs) {
  return internal::pmap_impl<false, true>(fn, vecs...);
}

template <typename F, RVal... Ts>
  requires std::invocable<F&, r_size_t, Ts...>
auto pmap_parallel_simd_with_index(F fn, const r_vec<Ts>&... vecs) {
  return internal::pmap_impl<true, true>(fn, vecs...);
}

template <typename F, RVal... Ts>
  requires std::invocable<F&, Ts...>
auto pmap_simd(F fn, const r_vec<Ts>&... vecs) {
  return internal::pmap_impl<true>([&](r_size_t, Ts... vs){ return fn(vs...); }, vecs...);
}

template <typename F, RVal... Ts>
  requires std::invocable<F&, Ts...>
auto pmap_parallel(F fn, const r_vec<Ts>&... vecs) {
  return internal::pmap_impl<false, true>([&](r_size_t, Ts... vs){ return fn(vs...); }, vecs...);
}

template <typename F, RVal... Ts>
  requires std::invocable<F&, Ts...>
auto pmap_parallel_simd(F fn, const r_vec<Ts>&... vecs) {
  return internal::pmap_impl<true, true>([&](r_size_t, Ts... vs){ return fn(vs...); }, vecs...);
}

namespace internal {

template <RVal T>
struct cursor {
  const r_vec<T>* src;
  r_size_t i;
  r_size_t n;
  bool oob(r_size_t idx) const noexcept {
    using r_usize_t = std::make_unsigned_t<r_size_t>;
    return static_cast<r_usize_t>(idx) >= static_cast<r_usize_t>(n);
  }
};

}


template <RVal T>
bool lag_exists(const internal::cursor<T>& c, r_size_t k = 1) noexcept {
  return !c.oob(c.i - k);
}
template <RVal T>
bool lead_exists(const internal::cursor<T>& c, r_size_t k = 1) noexcept {
  return lag_exists(c, -k);
}
template <RVal T>
T lag(const internal::cursor<T>& c, r_size_t k = 1, const T& default_value = na<T>()) {
  return lag_exists(c, k) ? c.src->view(c.i - k) : default_value;
}
template <RVal T>
T lead(const internal::cursor<T>& c, r_size_t k = 1, const T& default_value = na<T>()) {
  return lag(c, -k, default_value);
}
template <RVal T>
T curr(const internal::cursor<T>& c) {
  return c.src->view(c.i);
}

// pmap but positional helpers like `lag()`, `lead()` and `curr()` must be used
// e.g. [](auto a){ return curr(a); } instead of [](auto a){ return a; }
template <typename F, RVal... Ts>
  requires std::invocable<F&, internal::cursor<Ts>...>
auto pmap_with_shift(F fn, const r_vec<Ts>&... vecs) {
  const std::array<r_size_t, sizeof...(Ts)> lens{ vecs.length()... };
  for (r_size_t l : lens) {
    if (l != lens[0]) [[unlikely]] {
      abort("pmap_window: all inputs must be the same length");
    }
  }
  return pmap_with_index([&](r_size_t i, Ts...){
    return fn(internal::cursor<Ts>{ &vecs, i, lens[0] }...);
  }, vecs...);
}

}

#undef CPPALLY_OMP_DISPATCH

#endif
