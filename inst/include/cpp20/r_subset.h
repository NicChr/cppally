#ifndef CPP20_R_SUBSET_H
#define CPP20_R_SUBSET_H

#include <cpp20/r_match.h>

namespace cpp20 {

namespace internal {

template <RIntegerLocation U, RIntegerLocation V = r_int>
r_vec<V> exclude_locs(const r_vec<U>& exclude, unwrap_t<U> xn) {

  using int_t = unwrap_t<common_math_t<U, V>>;

  if (xn < 0){
    abort("`xn` must be >= 0");
  }
  if constexpr (is<V, r_int>){
    if (xn > r_limits<r_int>::max()){
     abort("`xn > r_limits<r_int>::max()`, please use `exclude_locs<%s, r_int64>`", internal::type_str<U>());
   }
 }

  int_t n = xn;
  int_t m = exclude.length();
  int_t out_size, idx;
  int_t exclude_count = 0;
  int_t i = 0, k = 0;

  // Which elements do we keep?
  std::vector<uint8_t> keep(n, uint8_t(1));

  for (int_t j = 0; j < m; ++j) {
    if (is_na(exclude.get(j))) continue;
    if (exclude.get(j) > 0){
      abort("Cannot mix positive and negative subscripts");
    }
    idx = -exclude.get(j);
    // Check keep array for already assigned FALSE to avoid double counting
    if (idx > 0 && idx <= n && keep[idx - 1] == uint8_t(1)){
      keep[idx - 1] = uint8_t(0);
      ++exclude_count;
    }
  }
  out_size = n - exclude_count;
  r_vec<V> out(out_size);

  while(k != out_size){
    if (keep[i++] == uint8_t(1)){
      out.set(k++, i);
    }
  }
  return out;
}

inline r_size_t count_true(const r_vec<r_lgl>& x, const uint_fast64_t n){
  uint_fast64_t size = 0;
  auto* RESTRICT p_x = x.data();
  #pragma omp simd reduction(+:size)
  for (uint_fast64_t j = 0; j != n; ++j) size += (p_x[j] == 1);
  return size;
}

}

template <internal::RIntegerLocation U = r_int>
inline r_vec<U> which(const r_vec<r_lgl>& x, bool invert = false){

  r_size_t n = x.length();

  using int_t = unwrap_t<U>;

  if constexpr (is<U, r_int>){
    if (n > r_limits<r_int>::max()){
      abort("`x` is a long vector, please use which<r_int64> instead");
    }
  }

  int_t true_count = internal::count_true(x, n);
  int_t whichi = 0; 
  int_t i = 0; 

  if (invert){
    int_t out_size = n - true_count;
    r_vec<U> out(out_size);
    while (whichi < out_size){
        out.set(whichi, i + 1);
        whichi += static_cast<int_t>(!x.get(i++).is_true());
    }
    return out;
  } else {
    int_t out_size = true_count;
    r_vec<U> out(out_size);
    while (whichi < out_size){
      out.set(whichi, i + 1);
      whichi += static_cast<int_t>(x.get(i++).is_true());
  }
  return out;
  }
}


template <typename T, internal::RLocation U, internal::RIntegerLocation V = r_int>
r_vec<V> clean_locs(const r_vec<U>& locs, const r_vec<T>& x){

  if (locs.is_null()){
    return r_vec<V>(r_null);
  }

  r_size_t xn = x.length();
  r_size_t n = locs.length();

  if constexpr (RStringType<U>){
    
    r_vec<r_str_view> names = x.names();

    if (names.is_null()){
      abort("Cannot subset on the names of an unnamed vector");
    }
    r_vec<V> matches = match<r_str_view, V>(r_vec<r_str_view>(unwrap(locs), internal::view_tag{}), names);
    r_size_t n_na = matches.na_count();
    r_size_t out_size = n - n_na;
    r_vec<V> out(out_size);

    r_size_t k = 0;
    for (r_size_t i = 0; i < n; ++i){
      if (!is_na(matches.get(i))) out.set(k++, matches.get(i));
    }
    return out;
  } else if constexpr (RLogicalType<U>){
    if (locs.length() != xn){
      abort("length of indices must match vector length when indices is `r_vec<r_lgl>`");
    }
    return which<V>(locs, false);
  } else {
    r_size_t zero_count = 0,
    pos_count = 0,
    oob_count = 0,
    na_count = 0,
    neg_count = 0;

  for (r_size_t i = 0; i < n; ++i){
    auto loc = unwrap(locs.get(i));
    zero_count += (loc == 0);
    pos_count += (loc > 0);
    na_count += is_na(loc);
    oob_count += !is_na(loc) && std::abs(loc) > xn;
  }

  neg_count = n - pos_count - zero_count - na_count;

  if ( (pos_count > 0 && neg_count > 0) ||
       (neg_count > 0 && na_count > 0)){
    abort("Cannot mix positive and negative indices");
  }

  if (neg_count > 0){
    return internal::exclude_locs<V>(locs, xn);
  }
  if (zero_count > 0 || oob_count > 0 || na_count > 0){
    r_size_t out_size = pos_count - oob_count;
    r_vec<V> out(out_size);

    r_size_t k = 0;
    for (r_size_t i = 0; i < n; ++i){
      if (internal::between_impl<r_size_t>(unwrap(locs.get(i)), r_size_t(1), xn)){
        out.set(k++, locs.get(i));
      }
    }
    return out;
  }
  return locs;
  }
}

template <RVal T>
template <internal::RLocation U>
inline r_vec<T> r_vec<T>::subset(const r_vec<U>& indices, bool check) const {

  if (indices.is_null()){
    return *this;
  }

  if constexpr (RLogicalType<U> || RStringType<U>){
    return subset(clean_locs(indices, *this), /*check=*/ false);
  } else {

    using unsigned_int_t = std::make_unsigned_t<unwrap_t<U>>;
    unsigned_int_t n = indices.length();

    r_vec<T> out(n);

    if (check){
      unsigned_int_t
      xn = length(),
        k = 0,
        na_val = unwrap(na<U>()),
        j;
  
      for (unsigned_int_t i = 0; i < n; ++i){
        j = unwrap(indices.get(i));
        if (internal::between_impl<unsigned_int_t>(j, unsigned_int_t(1), xn)){
          out.set(k++, view(--j));
        } 
        // If j > n_val then it is a negative signed integer
        else if (j > na_val){
          return subset(internal::exclude_locs(indices, xn));
        } 
        else if (j != 0U){
          out.set(k++, na<T>());
        }
      }
  
      return out.resize(k);
    } else {
      for (unsigned_int_t i = 0; i < n; ++i){
        out.set(i, view(unwrap(indices.get(i)) - 1));
    }
    return out;
  }
}
}

template <RVal T>
template <internal::RLocation U>
void r_vec<T>::replace(const r_vec<U>& where, const r_vec<T>& with) {

  if (is_null()) return;

  r_size_t with_size = with.length();

  r_size_t xi;
  r_size_t withi = 0;

  if (is_long()){
    // Clean where vector
    r_vec<r_int64> where_clean = clean_locs<T, U, r_int64>(where, *this);
    r_size_t where_size = where_clean.length();
  
    for (r_size_t i = 0; i < where_size; recycle_index(withi, with_size), ++i){
      set(where_clean.get(i) - 1, with.get(withi));
    }
  } else {
    // Clean where vector
    r_vec<r_int> where_clean = clean_locs<T, U, r_int>(where, *this);
    r_size_t where_size = where_clean.length();
  
    for (r_size_t i = 0; i < where_size; recycle_index(withi, with_size), ++i){
      set(where_clean.get(i) - 1, with.get(withi));
    }
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
