#ifndef CPPALLY_R_SETUP_H
#define CPPALLY_R_SETUP_H

#ifdef R_INTERNALS_H_
#if !(defined(R_NO_REMAP) && defined(STRICT_R_HEADERS))
#error R headers were included before cppally headers \
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

#ifndef ANKERL_UNORDERED_DENSE_DISABLE_PMR
#define ANKERL_UNORDERED_DENSE_DISABLE_PMR
#endif

// To preserve R ALTREP and avoid premature data materialisation
// Either uncomment the below line or define it in your code before including <cppally.hpp>
// Note: Preserving ALTREP may incur some performance cost
// #define CPPALLY_PRESERVE_ALTREP 

#include <Rinternals.h>
#include <iosfwd> // Forward declarations for strings
#include <cstdint>

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

// The R parser will search for the string "[[cppally::register]]"
#ifdef __R_GENERATE_
  #define CPPALLY_REGISTER [[cppally::register]]
#else
  #define CPPALLY_REGISTER 
#endif

#ifdef _MSC_VER
#define RESTRICT __restrict
#else
#define RESTRICT __restrict__
#endif

#if !defined(OBJSXP) && defined(S4SXP) 
#define OBJSXP S4SXP
#endif

// Disable custom OMP macros when copy-on-modify behaviour is specified
#if defined(_OPENMP) && !defined(CPPALLY_COPY_ON_MODIFY) && !defined(CPPALLY_PRESERVE_ALTREP)
#include <omp.h>
#define OMP_PRAGMA(x) _Pragma(#x)
#define OMP_NUM_PROCS omp_get_num_procs()
#define OMP_THREAD_LIMIT omp_get_thread_limit()
#define OMP_MAX_THREADS omp_get_max_threads()
#define OMP_PARALLEL(n_threads) OMP_PRAGMA(omp parallel if ((n_threads) > 1) num_threads((n_threads)))
#define OMP_FOR_SIMD OMP_PRAGMA(omp for simd)
#define OMP_SIMD OMP_PRAGMA(omp simd)
#define OMP_PARALLEL_FOR_SIMD(n_threads) OMP_PRAGMA(omp parallel for simd if(parallel: (n_threads) > 1) num_threads((n_threads)))
#define OMP_SIMD_REDUCTION1(OP) OMP_PRAGMA(omp simd reduction(OP))
#define OMP_SIMD_REDUCTION2(OP1, OP2) OMP_PRAGMA(omp simd reduction(OP1) reduction(OP2))
#define OMP_PARALLEL_FOR_SIMD_REDUCTION1(n_threads, OP) OMP_PRAGMA(omp parallel for simd if(parallel: (n_threads) > 1) num_threads((n_threads)) reduction(OP))
#define OMP_PARALLEL_FOR_SIMD_REDUCTION2(n_threads, OP1, OP2) OMP_PRAGMA(omp parallel for simd if(parallel: (n_threads) > 1) num_threads((n_threads)) reduction(OP1) reduction(OP2))
#else
#define OMP_PRAGMA(x)
#define OMP_NUM_PROCS 1
#define OMP_THREAD_LIMIT 1
#define OMP_MAX_THREADS 1
#define OMP_PARALLEL(n_threads)
#define OMP_SIMD
#define OMP_FOR_SIMD
#define OMP_PARALLEL_FOR_SIMD(n_threads)
#define OMP_SIMD_REDUCTION1(OP1)
#define OMP_SIMD_REDUCTION2(OP1, OP2)
#define OMP_PARALLEL_FOR_SIMD_REDUCTION1(n_threads, OP)
#define OMP_PARALLEL_FOR_SIMD_REDUCTION2(n_threads, OP1, OP2)
#endif

#define OMP_DO_NOTHING // Placeholder for no OMP operations

#ifdef __SIZEOF_INT128__
using int128_otherwise_64_t = __int128_t;
inline constexpr bool int128_available = true;
#else
using int128_otherwise_64_t = int64_t;
inline constexpr bool int128_available = false;
#endif

namespace cppally {

using r_size_t = R_xlen_t;

namespace internal {
inline constexpr long long int CPPALLY_OMP_THRESHOLD = 100000;
inline int CPPALLY_N_THREADS = 1;
}

} // end of cppally namespace

#endif
