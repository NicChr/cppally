#ifndef CPP20_R_SETUP_H
#define CPP20_R_SETUP_H

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

// The R parser will search for the string "[[cpp20::register]]"
#ifdef __R_GENERATE_
  #define CPP20_REGISTER [[cpp20::register]]
#else
  #define CPP20_REGISTER 
#endif

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
#else
#define OMP_PRAGMA(x)
#define OMP_NUM_PROCS 1
#define OMP_THREAD_LIMIT 1
#define OMP_MAX_THREADS 1
#define OMP_PARALLEL(n_threads)
#define OMP_SIMD
#define OMP_FOR_SIMD
#define OMP_PARALLEL_FOR_SIMD(n_threads)
#endif

#define OMP_DO_NOTHING // Placeholder for no OMP operations

namespace cpp20 {

using r_size_t = R_xlen_t;

namespace internal {

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
inline int CPP20_N_THREADS = 1;

}

} // end of cpp20 namespace

#endif
