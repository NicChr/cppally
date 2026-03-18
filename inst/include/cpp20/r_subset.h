#ifndef CPP20_R_SUBSET_H
#define CPP20_R_SUBSET_H

#include <cpp20/r_match.h>

namespace cpp20 {

template <typename U>
requires (any<U, r_int, r_int64>)
r_vec<U> exclude_locs(const r_vec<U>& exclude, unwrap_t<U> xn) {

  using int_t = unwrap_t<U>;

  int_t n = xn;
  int_t m = exclude.length();
  int_t out_size, idx;
  int_t exclude_count = 0;
  int_t i = 0, k = 0;

  // Which elements do we keep?
  std::vector<bool> keep(n, true);

  for (int_t j = 0; j < m; ++j) {
    if (is_na(exclude.get(j))) continue;
    if (exclude.get(j) > 0){
      abort("Cannot mix positive and negative subscripts");
    }
    idx = -exclude.get(j);
    // Check keep array for already assigned FALSE to avoid double counting
    if (idx > 0 && idx <= n && keep[idx - 1] == 1){
      keep[idx - 1] = false;
      ++exclude_count;
    }
  }
  out_size = n - exclude_count;
  r_vec<U> out(out_size);

  while(k != out_size){
    if (keep[i++] == true){
      out.set(k++, i);
    }
  }
  return out;
}

namespace internal {

inline r_size_t count_true(const r_vec<r_lgl>& x, const uint_fast64_t n){
  uint_fast64_t size = 0;
  auto* RESTRICT p_x = x.data();
  #pragma omp simd reduction(+:size)
  for (uint_fast64_t j = 0; j != n; ++j) size += (p_x[j] == 1);
  return size;
}

}

inline r_vec<r_int> which(const r_vec<r_lgl>& x, bool invert = false){
  r_size_t n = x.length();
  r_size_t true_count = internal::count_true(x, n);
  int whichi = 0;
  int i = 0; 

  if (invert){
      r_size_t out_size = n - true_count;
      r_vec<r_int> out(out_size);
      while (whichi < out_size){
          out.set(whichi, i + 1);
          whichi += static_cast<int>((x.get(i++) != r_true));
      }
      return out;
  } else {
      r_size_t out_size = true_count;
      r_vec<r_int> out(out_size);
      while (whichi < out_size){
          out.set(whichi, i + 1);
          whichi += static_cast<int>((x.get(i++) == r_true));
      }
      return out;
  }
}

template <RVal T>
template <typename U>
requires (any<U, r_lgl, r_int, r_int64, r_str_view, r_str>)
inline r_vec<T> r_vec<T>::subset(const r_vec<U>& indices) const {

  if (indices.is_null()){
    return *this;
  }

  if constexpr (is<U, r_lgl>){
    auto locs = which(indices);
    int n = locs.length();
  
    auto out = r_vec<T>(n);
  
    OMP_SIMD
    for (int i = 0; i < n; ++i){
      out.set(i, view(unwrap(locs.get(i)) - 1));
    }

    return out;

  } else if constexpr (RStringType<U>){

    auto vec_names = names();

    if (vec_names.is_null()){
      // NA filled vector
      return r_vec<T>(indices.length(), na<T>());
    }

    auto locs = match(indices, vec_names);
    return subset(locs);
  } else {

    using unsigned_int_t = std::make_unsigned_t<unwrap_t<U>>;

    unsigned_int_t
    xn = length(),
      n = indices.length(),
      k = 0,
      na_val = unwrap(na<U>()),
      j;

    auto out = r_vec<T>(n);

    for (unsigned_int_t i = 0; i < n; ++i){
      j = unwrap(indices.get(i));
      if (internal::between_impl<unsigned_int_t>(j, unsigned_int_t(1), xn)){
        out.set(k++, view(--j));
      } 
      // If j > n_val then it is a negative signed integer
      else if (j > na_val){
        return subset(exclude_locs(indices, xn));
      } 
      else if (j != 0U){
        out.set(k++, na<T>());
      }
    }

    return out.resize(k);
  }
}


// 0-indexed negative locations (can't do "everything but first location" with this approach)
// template <typename U>
// requires (any<U, r_int, r_int64>)
// r_vec<U> exclude_locs(const r_vec<U>& exclude, unwrap_t<U> xn) {

//   using int_t = unwrap_t<U>;

//   int_t n = xn;
//   int_t m = exclude.length();
//   int_t out_size, idx;
//   int_t exclude_count = 0;
//   int_t i = 0, k = 0;

//   // Which elements do we keep?
//   std::vector<bool> keep(n, true);

//   for (int_t j = 0; j < m; ++j) {
//     if (is_na(exclude.get(j))) continue;
//     if (exclude.get(j) > 0){
//       abort("Cannot mix positive and negative subscripts");
//     }
//     idx = -exclude.get(j);
//     // Check keep array for already assigned FALSE to avoid double counting
//     if (idx < n && keep[idx]){
//       keep[idx] = false;
//       ++exclude_count;
//     }
//   }
//   out_size = n - exclude_count;
//   auto out = r_vec<r_int>(out_size);

//   while(k != out_size){
//     if (keep[i++]){
//       out.set(k++, i - 1);
//     }
//   }
//   return out;
// }

// // 0-indexed indices
// template <RVal T>
// template <typename U>
// requires (any<U, r_int, r_int64>)
// inline r_vec<T> r_vec<T>::subset(const r_vec<U>& indices) const {

//   using unsigned_int_t = std::make_unsigned_t<unwrap_t<U>>;

//   unsigned_int_t
//   xn = length(),
//     n = indices.length(),
//     k = 0,
//     na_val = unwrap(na<U>()),
//     j;

//   auto out = r_vec<T>(n);

//   for (unsigned_int_t i = 0; i < n; ++i){
//     j = unwrap(indices.get(i));
//     if (j < xn){
//       out.set(k++, get(j));
//     } 
//     // If j > n_val then it is a negative signed integer
//     else if (j > na_val){
//       return subset(exclude_locs(indices, xn));
//     } 
//     else {
//       out.set(k++, na<T>());
//     }
//   }
//   return out.resize(k);
// }


}

#endif
