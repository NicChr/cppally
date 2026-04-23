#ifndef CPPALLY_R_SUBSET_H
#define CPPALLY_R_SUBSET_H

#include <cppally/r_setup.h>
#include <cppally/r_utils.h>
#include <cppally/sugar/r_match.h>
#include <cppally/r_coerce.h>
#include <vector> // For C++ vectors

namespace cppally {

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
  std::vector<uint8_t> keep(xn, 1U);

  for (r_size_t j = 0; j < m; ++j) {
    if (is_na(exclude.get(j))) continue;
    if (exclude.get(j) < 0) [[unlikely]] {
      abort("Please supply positive indices to %s", __func__);
    }
    idx = unwrap(exclude.get(j));
    // Check keep array for already assigned FALSE to avoid double counting
    if (idx < n && keep[idx] == 1U){
      keep[idx] = 0U;
      ++exclude_count;
    }
  }
  out_size = n - exclude_count;
  r_vec<V> out(out_size);

  while (k != out_size){
    if (keep[i] == 1U){
      out.set(k++, V(static_cast<unwrap_t<V>>(i)));
    }
    ++i;
  }
  return out;
}

// Returns valid indices
// It ignores NA and out-of-bounds (OOB) indices, which differs to subset() which returns NA when given NA or OOB
template <internal::RNumericSubscript V = r_int, typename T, internal::RSubscript U>
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
    r_size_t pos_count = 0,
    oob_count = 0,
    na_count = 0,
    neg_count = 0;

  for (r_size_t i = 0; i < n; ++i){
    auto loc = unwrap(locs.get(i));
    if (is_na(loc)){
      ++na_count;
    } else if (loc < 0){
      ++neg_count;
    } else {
      ++pos_count;
      if (static_cast<r_size_t>(loc) >= xn){
        ++oob_count;
      }
    }
  }

  if (neg_count > 0){
    abort("Negative indices are not allowed, use `invert = true`");
  }
  if (oob_count > 0 || na_count > 0){
    r_size_t out_size = pos_count - oob_count;
    r_vec<V> out(out_size);

    r_size_t k = 0;
    for (r_size_t i = 0; i < n; ++i){
      auto loc = unwrap(locs.get(i));
      if (!is_na(loc) && loc >= 0 && static_cast<r_size_t>(loc) < xn){
        out.set(k++, V(static_cast<unwrap_t<V>>(loc)));
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
inline r_vec<T> r_vec<T>::subset(const r_vec<U>& indices, bool check, bool invert) const {

  if (indices.is_null()){
    return *this;
  }

  if constexpr (RLogicalType<U> || RStringType<U>){
    if (is_long()){
      return subset(internal::clean_locs<r_int64>(indices, *this), /*check=*/ false, /*invert=*/ invert);
    } else {
      return subset(internal::clean_locs<r_int>(indices, *this), /*check=*/ false, /*invert=*/ invert);
    }
  } else {

    if (invert){
      if (is_long()){
        return subset(internal::exclude_locs<r_int64>(indices, length()), false, false);
      } else {
        return subset(internal::exclude_locs<r_int>(indices, length()), false, false);
      }
    }

    using unsigned_int_t = std::make_unsigned_t<unwrap_t<U>>;
    r_size_t n = indices.length();

    r_vec<T> out(n);

    if (check){
      r_size_t xn = length();
      unsigned_int_t na_val = unwrap(na<U>());
      unsigned_int_t j;
  
      for (r_size_t i = 0; i < n; ++i){
        j = unwrap(indices.get(i));
        if (static_cast<r_size_t>(j) < xn){
          out.set(i, view(static_cast<r_size_t>(j)));
        } else if (j > na_val){
          // If j > n_val then it is a negative signed integer
          abort("Negative indices are unsupported, use `invert = true`");
        } else {
          if constexpr (RScalar<T>){
            out.set(i, na<T>());
          }
        }
      }
      return out;
    } else {
      for (r_size_t i = 0; i < n; ++i){
        out.set(i, view(unwrap(indices.get(i))));
    }
    return out;
  }
}
}

// r_vec<T> member functions

template <RVal T>
r_size_t r_vec<T>::count(const r_vec<T>& values) const {

  r_size_t 
    n_values = values.length(),
    out = 0;

  if (n_values == 0){
    return out;
  } else if (n_values == 1){
    // Just simple count loop
    return count(values.view(0)); 
  }

  // Have to explicitly request 64-bit matches (annoying)
  if (is_long()){
    r_vec<r_int64> matches = match<r_int64>(*this, values);
    out = matches.length() - matches.na_count(); // Number of matches
    out = std::max(out, r_size_t(0));
    return out;
  } else {
    r_vec<r_int> matches = match(*this, values);
    out = matches.length() - matches.na_count(); // Number of matches
    out = std::max(out, r_size_t(0));
    return out;
  }
}

template <RVal T>
template <internal::RNumericSubscript V>
r_vec<V> r_vec<T>::find(const r_vec<T>& values, bool invert) const {


  if constexpr (is<V, r_int>){
    if (is_long()){
      abort("`x` is a long vector, please use `find<r_int64>` for 64-bit locations");
    }
  }

  r_size_t n_values = values.length();

  if (n_values == 0){
    if (invert){
      r_size_t n = length();
      r_vec<V> out(n);
      OMP_SIMD
      for (r_size_t i = 0; i < n; ++i) out.set(i, V(static_cast<unwrap_t<V>>(i)));
      return out;
    } else {
      return r_vec<V>();
    }
  } else if (n_values == 1){
    // Just simple find loop
    return find<V>(values.view(0), /*invert=*/ invert); 
  }
  r_vec<V> matches = match<V>(*this, values);
  return matches.template find<V>(na<V>(), !invert);
}

template <RVal T>
r_vec<T> r_vec<T>::remove(const r_vec<T>& values) const {
  if (is_long()){
    r_vec<r_int64> keep = find<r_int64>(values, /*invert=*/ true);
    return subset(keep, false);
  } else {
    r_vec<r_int> keep = find<r_int>(values, /*invert=*/ true);
    return subset(keep, false);
  }
}

template <RVal T>
template <internal::RSubscript U>
void r_vec<T>::fill(const r_vec<U>& where, const r_vec<T>& with) {

  if (is_null()) return;

  r_size_t with_size = with.length();
  r_size_t withi = 0;

  if (with_size == 0){
    return;
  }

  r_vec<T> with_clean = as<r_vec<T>>(with);

  if (is_long()){
    // Clean where vector
    r_vec<r_int64> where_clean = internal::clean_locs<r_int64>(where, *this);
    r_size_t where_size = where_clean.length();
  
    for (r_size_t i = 0; i < where_size; recycle_index(withi, with_size), ++i){
      set(unwrap(where_clean.get(i)), with_clean.get(withi));
    }
  } else {
    // Clean where vector
    r_vec<r_int> where_clean = internal::clean_locs<r_int>(where, *this);
    r_size_t where_size = where_clean.length();
  
    for (r_size_t i = 0; i < where_size; recycle_index(withi, with_size), ++i){
      set(unwrap(where_clean.get(i)), with_clean.get(withi));
    }
  }
}

template <RVal T>
void r_vec<T>::replace(const r_vec<T>& old_values, const r_vec<T>& new_values){

  if (is_null()) return;

  r_size_t n = length();
  r_size_t oldv_size = old_values.length();
  r_size_t newv_size = new_values.length();
  r_size_t oldvi = 0, newvi = 0;

  if (oldv_size == 0 || newv_size == 0) return;

  if (oldv_size == 1 && newv_size == 1){
    replace(old_values.view(0), new_values.view(0));
    return;
  }

  r_vec<T> prev = as<r_vec<T>>(old_values);
  r_vec<T> repl = as<r_vec<T>>(new_values);

  for (r_size_t i = 0; i < n; 
    recycle_index(oldvi, oldv_size), 
    recycle_index(newvi, newv_size), 
    ++i){
      if (identical(view(i), prev.view(oldvi))){
        set(i, repl.view(newvi));
      }
  }
}

}

#endif
