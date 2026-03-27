#ifndef CPP20_R_UTILS_H
#define CPP20_R_UTILS_H

#include <cpp20/r_setup.h>

namespace cpp20 {

// Set & get the number of OMP threads
inline int get_threads(){
  auto n_threads = std::min(internal::CPP20_N_THREADS, OMP_MAX_THREADS);
  return n_threads > 1 ? n_threads : 1;
}
inline void set_threads(int n){
  int max_threads = OMP_MAX_THREADS;
  internal::CPP20_N_THREADS = std::min(n, max_threads);
}

// Recycle loop indices
template<typename T>
inline constexpr void recycle_index(T& v, T size) {
  v = (++v == size) ? T(0) : v;
}

// inline bool xor_(bool a, bool b) noexcept {
//   return (a + b) == 1;
// }

namespace internal {

template<typename T, typename U>
inline constexpr bool between_impl(const T x, const U lo, const U hi) {
  return x >= lo && x <= hi;
}

inline int calc_threads(r_size_t data_size){
    return data_size >= CPP20_OMP_THRESHOLD ? get_threads() : 1;
  }

}

}

#endif
