#ifndef CPP20_R_SUBSET_H
#define CPP20_R_SUBSET_H

#include <cpp20/r_match.h>
#include <cpp20/r_coerce.h>

namespace cpp20 {

namespace internal {

template <RNumericSubscript V = r_int, RNumericSubscript U>
r_vec<V> exclude_locs(const r_vec<U>& exclude, r_size_t xn) {

  if (xn < 0){
    abort("`xn` must be >= 0");
  }
  if constexpr (is<V, r_int>){
    if (xn > unwrap(r_limits<r_int>::max())){
     abort("`xn > r_limits<r_int>::max()`, please use `exclude_locs<r_int64>`");
   }
 }

  r_size_t n = xn;
  r_size_t m = exclude.length();
  r_size_t out_size, idx;
  r_size_t exclude_count = 0;
  r_size_t i = 0, k = 0;

  // Which elements do we keep?
  std::vector<uint8_t> keep(xn, uint8_t(1));

  for (r_size_t j = 0; j < m; ++j) {
    if (is_na(exclude.get(j))) continue;
    if (exclude.get(j) > 0){
      abort("Cannot mix positive and negative subscripts");
    }
    idx = -unwrap(exclude.get(j));
    // Check keep array for already assigned FALSE to avoid double counting
    if (idx > 0 && idx <= n && keep[idx - 1] == 1U){
      keep[idx - 1] = 0U;
      ++exclude_count;
    }
  }
  out_size = n - exclude_count;
  r_vec<V> out(out_size);

  while(k != out_size){
    if (keep[i++] == 1U){
      out.set(k++, i);
    }
  }
  return out;
}

template <typename T, internal::RSubscript U, internal::RNumericSubscript V = r_int>
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
    r_vec<V> matches = match<V>(r_vec<r_str_view>(unwrap(locs), internal::view_tag{}), names);
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
    return locs.template find<V>(r_true, false);
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
  return as<r_vec<V>>(locs);
  }
}

}

template <RVal T>
template <internal::RSubscript U>
inline r_vec<T> r_vec<T>::subset(const r_vec<U>& indices, bool check) const {

  if (indices.is_null()){
    return *this;
  }

  if constexpr (RLogicalType<U> || RStringType<U>){
    if (is_long()){
      return subset(internal::clean_locs<T, U, r_int64>(indices, *this), /*check=*/ false);
    } else {
      return subset(internal::clean_locs<T, U, r_int>(indices, *this), /*check=*/ false);
    }
  } else {

    using unsigned_int_t = std::make_unsigned_t<unwrap_t<U>>;
    r_size_t n = indices.length();

    r_vec<T> out(n);

    if (check){
      r_size_t xn = length(), k = 0;
      unsigned_int_t na_val = unwrap(na<U>());
      unsigned_int_t j;
  
      for (r_size_t i = 0; i < n; ++i){
        j = unwrap(indices.get(i));
        if (j >= 1U && static_cast<r_size_t>(j) <= xn){
          out.set(k++, view(static_cast<r_size_t>(j) - r_size_t(1)));
        } 
        // If j > n_val then it is a negative signed integer
        else if (j > na_val){
          if (is_long()){
            return subset(internal::exclude_locs<r_int64>(indices, xn));
          } else {
            return subset(internal::exclude_locs<r_int>(indices, xn));
          }
        } 
        else if (j != 0U){
          out.set(k++, na<T>());
        }
      }
  
      return out.resize(k);
    } else {
      for (r_size_t i = 0; i < n; ++i){
        out.set(i, view(unwrap(indices.get(i)) - 1));
    }
    return out;
  }
}
}

// r_vec<T> member functions

template <RVal T>
template <typename U>
r_size_t r_vec<T>::count(const r_vec<U>& values) const {

  r_size_t 
    n_values = values.length(),
    n_implicit_na_coercions = 0,
    out = 0;

  if (n_values == 0){
    return out;
  } else if (n_values == 1){
    // Just simple count loop
    return count(values.view(0)); 
  }

  // Coerce values to same vec type as r_vec<T>

  r_vec<T> values2 = as<r_vec<T>>(values);

  // Count the number of implicit NA coercions

  for (r_size_t i = 0; i < n_values; ++i){
    n_implicit_na_coercions += (cpp20::is_na(values2.view(i)) && !cpp20::is_na(values.view(i)));
  }

  // Have to explicitly request 64-bit matches (annoying)
  if (is_long()){
    r_vec<r_int64> matches = match<r_int64>(*this, values2);
    out = matches.length() - matches.na_count(); // Number of matches
    out -= n_implicit_na_coercions;
    out = std::max(out, r_size_t(0));
    return out;
  } else {
    r_vec<r_int> matches = match(*this, values2);
    out = matches.length() - matches.na_count(); // Number of matches
    out -= n_implicit_na_coercions;
    out = std::max(out, r_size_t(0));
    return out;
  }
}

template <RVal T>
template <internal::RNumericSubscript V, typename U>
r_vec<V> r_vec<T>::find(const r_vec<U>& values, bool invert) const {


  if constexpr (is<V, r_int>){
    if (is_long()){
      abort("`x` is a long vector, please use `find<r_int64>` for 64-bit locations");
    }
  }

  r_size_t n_values = values.length();

  if (n_values == 0){
    return r_vec<V>();
  } else if (n_values == 1){
    // Just simple find loop
    return find<V>(values.view(0)); 
  }

  // Coerce values to same vec type as r_vec<T>

  r_vec<T> values2 = as<r_vec<T>>(values);

  // Remove values that were implicitly coerced to NA
  // r_size_t n_implicit_na_coercions = 0;
  // for (r_size_t i = 0; i < n_values; ++i){
  //   n_implicit_na_coercions += (cpp20::is_na(values2.view(i)) && !cpp20::is_na(values.view(i)));
  // } 
  r_size_t k = 0;
  r_vec<V> exclude(n_values, 0); // Fill with 0 to signify no exclusions by default
  for (r_size_t i = 0; i < n_values; ++i){
    bool implicit_na_coercion = (cpp20::is_na(values2.view(i)) && !cpp20::is_na(values.view(i)));
    if (implicit_na_coercion){
      exclude.set(k++, -(i + 1));
    }
  }
  if (k > 0){
    values2 = values2.subset(exclude);
  }
  r_vec<V> matches = match<V>(*this, values2);
  return matches.template find<V>(na<V>(), !invert);
}

template <RVal T>
template <internal::RSubscript U>
void r_vec<T>::fill(const r_vec<U>& where, const r_vec<T>& with) {

  if (is_null()) return;

  r_size_t with_size = with.length();
  r_size_t withi = 0;

  if (is_long()){
    // Clean where vector
    r_vec<r_int64> where_clean = internal::clean_locs<T, U, r_int64>(where, *this);
    r_size_t where_size = where_clean.length();
  
    for (r_size_t i = 0; i < where_size; recycle_index(withi, with_size), ++i){
      set(unwrap(where_clean.get(i)) - 1, with.get(withi));
    }
  } else {
    // Clean where vector
    r_vec<r_int> where_clean = internal::clean_locs<T, U, r_int>(where, *this);
    r_size_t where_size = where_clean.length();
  
    for (r_size_t i = 0; i < where_size; recycle_index(withi, with_size), ++i){
      set(unwrap(where_clean.get(i)) - 1, with.get(withi));
    }
  }
}

template <RVal T>
template <internal::RSubscript U>
void r_vec<T>::replace(const r_vec<U>& where, const r_vec<T>& old_values, const r_vec<T>& new_values){

  if (is_null()) return;

  r_size_t oldv_size = old_values.length();
  r_size_t newv_size = new_values.length();
  r_size_t oldvi = 0, newvi = 0;

  if (is_long()){
    // Clean where vector
    r_vec<r_int64> where_clean = clean_locs<T, U, r_int64>(where, *this);
    r_size_t where_size = where_clean.length();
    auto* RESTRICT p_where = where_clean.data();
  
    for (r_size_t i = 0; i < where_size; 
      recycle_index(oldvi, oldv_size), 
      recycle_index(newvi, newv_size),
      ++i){
        auto loc = p_where[i] - 1;

        if ( (view(loc) == old_values.view(oldvi)).is_true()){
          set(loc, new_values.view(newvi));
        }
    }
  } else {
    // Clean where vector
    r_vec<r_int> where_clean = clean_locs<T, U, r_int>(where, *this);
    r_size_t where_size = where_clean.length();
    auto* RESTRICT p_where = where_clean.data();
  
    for (r_size_t i = 0; i < where_size; 
      recycle_index(oldvi, oldv_size), 
      recycle_index(newvi, newv_size), 
      ++i){
        auto loc = p_where[i] - 1;

        if ( (view(loc) == old_values.view(oldvi)).is_true()){
          set(loc, new_values.view(newvi));
        }
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
