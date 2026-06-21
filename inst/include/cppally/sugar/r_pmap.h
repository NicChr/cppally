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
  requires std::invocable<F, r_size_t, Ts...>
auto pmap_impl(F fn, const r_vec<Ts>&... vecs) {

  constexpr int n_vecs = sizeof...(Ts);

  using out_t = std::invoke_result_t<F, r_size_t, Ts...>;
  static_assert(RVal<out_t>, "pmap: output type is not storable in r_vec");

  if constexpr (n_vecs == 0) {
    return r_vec<out_t>();
  } else {
    // Check that all vectors are of the same length
    const std::array<r_size_t, n_vecs> lens{ vecs.length()... };
    const r_size_t n = lens[0];
    for (r_size_t l : lens) {
      if (l != n) [[unlikely]] {
        abort("pmap: all vectors must have equal length");
      }
    }

    r_vec<out_t> out(n);

    constexpr bool vectorisable_or_parallelisable = (RVectorisable<Ts> && ...) && RVectorisable<out_t>;

    if constexpr (vectorisable_or_parallelisable && (simd || parallel)) {

      auto* p_out = out.data();

      if constexpr (simd){
        if constexpr (parallel){
          int n_threads = internal::calc_threads(n);
          if (n_threads > 1){
            std::apply([&](auto*... ps){
              OMP_PARALLEL_FOR_SIMD(n_threads)
              for (r_size_t i = 0; i < n; ++i) {
                p_out[i] = unwrap(fn(i, Ts(ps[i])...));
              }
            }, std::tuple{ vecs.data()... });
          } else {
            std::apply([&](auto*... ps){
              OMP_SIMD
              for (r_size_t i = 0; i < n; ++i) {
                p_out[i] = unwrap(fn(i, Ts(ps[i])...));
              }
            }, std::tuple{ vecs.data()... });
          }
        } else {
          std::apply([&](auto*... ps){
            OMP_SIMD
            for (r_size_t i = 0; i < n; ++i) {
              p_out[i] = unwrap(fn(i, Ts(ps[i])...));
            }
          }, std::tuple{ vecs.data()... });
        }
      } else {
        // Must be parallel = true in this branch
        int n_threads = internal::calc_threads(n);
        if (n_threads > 1){
          std::apply([&](auto*... ps){
            OMP_PARALLEL(n_threads)
            for (r_size_t i = 0; i < n; ++i) {
              p_out[i] = unwrap(fn(i, Ts(ps[i])...));
            }
          }, std::tuple{ vecs.data()... });
        } else {
          std::apply([&](auto*... ps){
            for (r_size_t i = 0; i < n; ++i) {
              p_out[i] = unwrap(fn(i, Ts(ps[i])...));
            }
          }, std::tuple{ vecs.data()... });
        }
      }
    } else {
      for (r_size_t i = 0; i < n; ++i) {
        out.set(i, fn(i, vecs.view(i)...));
      }
    }
    return out;
  }
}

// map m x n vectors to 1 x n output by applying a user function: fn(x0, x1, x2, ..., xn)
template <typename F, RVal... Ts>
  requires std::invocable<F, Ts...>
auto pmap(F fn, const r_vec<Ts>&... vecs) {
  return pmap_impl<false>([&](r_size_t, Ts... vs){ return fn(vs...); }, vecs...);
}

// map m x n vectors to 1 x n output by applying a user function: fn(r_size_t index, x0, x1, x2, ..., xn)
template <typename F, RVal... Ts>
  requires std::invocable<F, r_size_t, Ts...>
auto pmap_with_index(F fn, const r_vec<Ts>&... vecs) {
  return pmap_impl<false>(fn, vecs...);
}

}

#endif
