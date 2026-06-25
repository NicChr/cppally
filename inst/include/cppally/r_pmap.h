#ifndef CPPALLY_R_PMAP_H
#define CPPALLY_R_PMAP_H

#include <cppally/r_vec.h>
#include <array>
#include <tuple>

namespace cppally {

// Engine for the pmap family. fn(r_size_t index, x0, x1, ..., xn) -> output element type.
// simd applies the loop in an OMP SIMD region; only honoured when every type is RVectorisable,
// otherwise it falls through to the scalar path.
template <bool simd = false, bool parallel = false, typename F, RVal... Ts>
  requires std::invocable<F&, r_size_t, Ts...>
auto pmap_impl(F fn, const r_vec<Ts>&... vecs) {

  #define CPPALLY_DO_MAP_WITH_DATA for (r_size_t i = 0; i < n; ++i) p_out[i] = unwrap(fn(i, Ts(ps[i])...));
  #define CPPALLY_DO_MAP for (r_size_t i = 0; i < n; ++i) out.set(i, fn(i, vecs.view(i)...));

  constexpr int n_vecs = sizeof...(Ts);

  using out_t = std::remove_cvref_t<std::invoke_result_t<F&, r_size_t, Ts...>>;
  static_assert(RVal<out_t>, "pmap: output type is not storable in r_vec");

  if constexpr (n_vecs == 0) {
    return r_vec<out_t>();
  } else {
    bool recycle = false;
    // Check that all vectors are of the same length
    const std::array<r_size_t, n_vecs> lens{ vecs.length()... };
    r_size_t n = lens[0];
    for (r_size_t l : lens) {
      if (l == 0){
        return r_vec<out_t>();
      }
      if (n != l){
        n = std::max(n, l);
        recycle = true;
      }      
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

      auto* p_out = out.data();

      // Unpack the data pointers once; only the pragma on each for-loop varies between branches.
      std::apply([&](auto*... ps){
        if constexpr (parallel){
          const int n_threads = internal::calc_threads(n);
          if constexpr (simd){
            if (n_threads > 1){
              OMP_PARALLEL_FOR_SIMD(n_threads)
              CPPALLY_DO_MAP_WITH_DATA
            } else {
              OMP_SIMD
              CPPALLY_DO_MAP_WITH_DATA
            }
          } else {
            if (n_threads > 1){
              OMP_PARALLEL(n_threads)
              CPPALLY_DO_MAP_WITH_DATA
            } else {
              CPPALLY_DO_MAP_WITH_DATA
            }
          }
        } else {
          // !parallel implies simd here (the enclosing branch requires simd || parallel)
          OMP_SIMD
          CPPALLY_DO_MAP_WITH_DATA
        }
      }, std::tuple{ vecs.data()... });

    } else {
      CPPALLY_DO_MAP
    }
    return out;
  }
  #undef CPPALLY_DO_MAP_WITH_DATA
  #undef CPPALLY_DO_MAP
}

// map m x n vectors to 1 x n output by applying a user function: fn(x0, x1, x2, ..., xn)
template <typename F, RVal... Ts>
  requires std::invocable<F&, Ts...>
auto pmap(F fn, const r_vec<Ts>&... vecs) {
  return pmap_impl<false>([&](r_size_t, Ts... vs){ return fn(vs...); }, vecs...);
}

// map m x n vectors to 1 x n output by applying a user function: fn(r_size_t index, x0, x1, x2, ..., xn)
template <typename F, RVal... Ts>
  requires std::invocable<F&, r_size_t, Ts...>
auto pmap_with_index(F fn, const r_vec<Ts>&... vecs) {
  return pmap_impl<false>(fn, vecs...);
}

template <typename F, RVal... Ts>
  requires std::invocable<F&, Ts...>
auto pmap_simd(F fn, const r_vec<Ts>&... vecs) {
  return pmap_impl<true>([&](r_size_t, Ts... vs){ return fn(vs...); }, vecs...);
}

template <typename F, RVal... Ts>
  requires std::invocable<F&, Ts...>
auto pmap_parallel(F fn, const r_vec<Ts>&... vecs) {
  return pmap_impl<false, true>([&](r_size_t, Ts... vs){ return fn(vs...); }, vecs...);
}

template <typename F, RVal... Ts>
  requires std::invocable<F&, Ts...>
auto pmap_parallel_simd(F fn, const r_vec<Ts>&... vecs) {
  return pmap_impl<true, true>([&](r_size_t, Ts... vs){ return fn(vs...); }, vecs...);
}

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

template <RVal T>
T lag(const cursor<T>& c, r_size_t k = 1, const T& default_value = na<T>()) {
  r_size_t idx = c.i - k;
  return c.oob(idx) ? default_value : c.src->view(idx);
}
template <RVal T>
T lead(const cursor<T>& c, r_size_t k = 1, const T& default_value = na<T>()) {
  return lag(c, -k, default_value);
}
template <RVal T>
T curr(const cursor<T>& c) { 
  return c.src->view(c.i);
}

template <typename F, RVal... Ts>
  requires std::invocable<F&, cursor<Ts>...>
auto pmap_with_shift(F fn, const r_vec<Ts>&... vecs) {
  if constexpr (sizeof...(Ts) > 1) {
    const std::array<r_size_t, sizeof...(Ts)> lens{ vecs.length()... };
    for (r_size_t l : lens) {
      if (l != lens[0]) [[unlikely]] {
        abort("pmap_window: all inputs must be the same length");
      }
    }
  }
  return pmap_with_index([&](r_size_t i, Ts...){
    return fn(cursor<Ts>{ &vecs, i, vecs.length() }...);
  }, vecs...);
}

}

#endif
