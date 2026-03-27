#ifndef CPP20_R_SETUP_H
#define CPP20_R_SETUP_H

#ifndef R_NO_REMAP
#define R_NO_REMAP 
#endif

#pragma once

#ifdef R_INTERNALS_H_
#if !(defined(R_NO_REMAP) && defined(STRICT_R_HEADERS))
#error R headers were included before cpp20 headers \
  and at least one of R_NO_REMAP or STRICT_R_HEADERS \
  was not defined.
#endif
#endif

#ifndef R_NO_REMAP
#define R_NO_REMAP
#endif

#ifndef STRICT_R_HEADERS
#define STRICT_R_HEADERS
#endif

#include <R_ext/Boolean.h>
#include <Rinternals.h>

// clang-format off
#ifdef __clang__
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wattributes"
#endif

#ifdef __GNUC__
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wattributes"
#endif
// clang-format on

#include <complex> // For complex<double>
#include <cstdint> // For uint32_t and similar
#include <iosfwd> // Forward declarations for strings

#ifdef _MSC_VER
#define RESTRICT __restrict
#else
#define RESTRICT __restrict__
#endif

#if !defined(OBJSXP) && defined(S4SXP) 
#define OBJSXP S4SXP
#endif

#ifdef _OPENMP
#include <omp.h>
#define OMP_PRAGMA(x) _Pragma(#x)
#define OMP_NUM_PROCS omp_get_num_procs()
#define OMP_THREAD_LIMIT omp_get_thread_limit()
#define OMP_MAX_THREADS omp_get_max_threads()
#define OMP_PARALLEL(n_threads) OMP_PRAGMA(omp parallel if ((n_threads) > 1) num_threads((n_threads)))
#define OMP_FOR_SIMD OMP_PRAGMA(omp for simd)
#define OMP_SIMD OMP_PRAGMA(omp simd)
#define OMP_PARALLEL_FOR_SIMD(n_threads) OMP_PRAGMA(omp parallel for simd if ((n_threads) > 1) num_threads((n_threads)))

#define OMP_DO_NOTHING
#else
#define OMP_PRAGMA(x)
#define OMP_NUM_PROCS 1
#define OMP_THREAD_LIMIT 1
#define OMP_MAX_THREADS 1
#define OMP_PARALLEL(n_threads)
#define OMP_SIMD
#define OMP_FOR_SIMD
#define OMP_PARALLEL_FOR_SIMD(n_threads)
#define OMP_DO_NOTHING
#endif

namespace cpp20 {

using r_size_t = R_xlen_t;

namespace internal {

// using r_sexp_tag_t = uint16_t; // cpp20 version of SEXPTYPE

// Currently unfinished
// enum : r_sexp_tag_t {
//     r_lgl_id = 1,
//     r_int_id = 2,
//     r_int64_id = 3,
//     r_dbl_id = 4,
//     r_cplx_id = 5,
//     r_raw_id = 6,
//     r_dates_id = 7,
//     r_pxt_id = 8,
//     r_chr = 9,
//     r_fct = 10,
//     r_list = 11,
//     r_df = 12,
//     r_unk = 13
// };


template<typename T, typename U>
inline constexpr bool between_impl(const T x, const U lo, const U hi) {
  return x >= lo && x <= hi;
}

// Wrap any callable f, and return a new callable that:
//   - takes (auto&&... args)
//   - calls f(args...) inside cpp11::unwind_protect

// Like safe but works also  for variadic fns
template <typename F>
auto r_safe_impl(F f) {
  return [f](auto&&... args)
    -> decltype(f(std::forward<decltype(args)>(args)...)) {

      using result_t = decltype(f(std::forward<decltype(args)>(args)...));

      if constexpr (std::is_void_v<result_t>) {
        unwind_protect([&] {
          f(std::forward<decltype(args)>(args)...);
        });
        // no return; result_t is void
      } else {
        return unwind_protect([&]() -> result_t {
          return f(std::forward<decltype(args)>(args)...);
        });
      }
    };

}

#define r_safe(F)                                                                      \
internal::r_safe_impl(                                                                 \
  [&](auto&&... args)                                                                  \
    -> decltype(F(std::forward<decltype(args)>(args)...)) {                            \
      return F(std::forward<decltype(args)>(args)...);                                 \
    }                                                                                  \
)


// If we find out eager initialisation of R symbols is a problem, we can use the constants below
// // Generic Lazy Loader for R Constants
// // Ptr: A pointer to the global R variable (e.g., &R_NilValue)
// template <typename T, T* Ptr>
// struct lazy_r_constant {
//     // Implicit conversion to SEXP (or whatever T is)
//     operator T() const noexcept {
//         return *Ptr;
//     }
    
//     // Allow comparison directly
//     bool operator==(T other) const noexcept { return *Ptr == other; }
//     bool operator!=(T other) const noexcept { return *Ptr != other; }
// };

// // lazy constants (to be defined later)
// inline constexpr lazy_r_constant<SEXP, &R_BlankString> blank_string_constant{};
// inline constexpr lazy_r_constant<SEXP, &R_MissingArg> missing_arg_constant{};

inline constexpr int64_t CPP20_OMP_THRESHOLD = 100000;
inline int cpp20_n_threads = 1;

}

// Set & get the number of OMP threads
inline int get_threads(){
  auto n_threads = std::min(internal::cpp20_n_threads, OMP_MAX_THREADS);
  return n_threads > 1 ? n_threads : 1;
}
inline void set_threads(int n){
  int max_threads = OMP_MAX_THREADS;
  internal::cpp20_n_threads = std::min(n, max_threads);
}


// inline bool xor_(bool a, bool b) noexcept {
//   return (a + b) == 1;
// }

namespace internal {

inline int calc_threads(r_size_t data_size){
  return data_size >= CPP20_OMP_THRESHOLD ? get_threads() : 1;
}

}

// Recycle loop indices
template<typename T>
inline constexpr void recycle_index(T& v, T size) {
  v = (++v == size) ? T(0) : v;
}

} // end of cpp20 namespace

#endif
