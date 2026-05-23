#ifndef CPPALLY_R_FILL_H
#define CPPALLY_R_FILL_H

#include <cppally/r_vec.h>
#include <cppally/r_coerce.h>
#include <cppally/sugar/r_subset.h>

namespace cppally {

// filling vector values
// includes r_vec member functions + free overloads

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

// Free functions

// Sequential fill
template <RVal T>
void fill(r_vec<T>& x, r_size_t start, r_size_t n, const T& val){
    x.fill(start, n, val);
}

// Fill entire vector with value
template <RVal T>
void fill(r_vec<T>& x, const T& val){
    x.fill(val);
}

template <RVector T, internal::RSubscript U>
void fill(T& x, const r_vec<U>& where, const T& with) {
  x.fill(where, with);
}

template <internal::RSubscript U, RStringType V>
void fill(r_factors& x, const r_vec<U>& where, const r_vec<V>& with) {
  r_vec<r_int> codes = x.get_codes(with);
  x.value.fill(where, codes);
}

template <internal::RSubscript U>
void fill(r_factors& x, const r_vec<U>& where, const r_factors& with) {
    r_vec<r_str_view> strings = as<r_vec<r_str_view>>(with);
    r_vec<r_int> codes = x.get_codes(strings);
    x.value.fill(where, codes);
}

// template <internal::RSubscript U>
// void fill(r_df& x, const r_vec<U>& where, const r_df& with) {
//   x.fill(where, with);
// }

template <internal::RSubscript U, typename V>
void fill(r_sexp& x, const r_vec<U>& where, const V& with);

template <internal::RSubscript U>
void fill(r_sexp& x, const r_vec<U>& where, const r_sexp& with);

}

#endif
