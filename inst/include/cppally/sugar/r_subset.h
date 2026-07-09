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
// It ignores NA and out-of-bounds (OOB) indices, which differs to subset() which returns NA when given invalid indices
template <internal::RNumericSubscript V = r_int, RComposite T, internal::RSubscript U>
r_vec<V> clean_locs(const r_vec<U>& locs, const T& x){

  if (locs.is_null()){
    return r_vec<V>(r_null);
  }

  r_size_t xn = length(x);
  r_size_t n = locs.length();

  if constexpr (RStringType<U>){

    static_assert(!is<V, r_int64>, "Cannot perform named-subsetting on long vectors");
    static_assert(!is<T, r_df>, "Named-subsetting of r_df is unsupported, use `r_df.select()`");

    if (x.names().is_null()){
      abort("Cannot subset on the names of an unnamed vector");
    }

    std::vector<int> matches;
    matches.reserve(n);

    for (r_size_t i = 0; i < n; ++i){
      r_int name_idx = x.name_index(locs.view(i), /*abort_on_missing = */ false);
      if (!is_na(name_idx)){
        matches.push_back(unwrap(name_idx));
      }
    }
    return as<r_vec<r_int>>(matches);
  } else if constexpr (RLogicalType<U>){
    if (locs.length() != xn) [[unlikely]] {
      abort("length of indices must match vector length when indices is `r_vec<r_lgl>`");
    }
    return locs.template find<V>(r_true, false);
  } else {

    using unsigned_int_t = std::make_unsigned_t<unwrap_t<U>>;

    std::vector<unwrap_t<V>> valid_indices;
    valid_indices.reserve(n);

    for (r_size_t i = 0; i < n; ++i){
      unsigned_int_t loc = static_cast<unsigned_int_t>(unwrap(locs.get(i)));
      if (loc < static_cast<unsigned_int_t>(xn)){
        valid_indices.push_back(static_cast<unwrap_t<V>>(loc));
      }
    }
    return as<r_vec<V>>(valid_indices);
  }
}

}

template <RVal T>
template <internal::RSubscript U>
inline r_vec<T> r_vec<T>::subset(const r_vec<U>& indices, bool invert, bool check) const {

  if (indices.is_null()){
    return *this;
  }
  
  if constexpr (RStringType<U>){
    if (is_long()){
        abort("%s: Named subsetting on long-vectors is unsupported", __func__);
    }

    r_size_t n = indices.length();
    r_vec<r_int> matches(n);
    bool do_check = false;
    for (r_size_t i = 0; i < n; ++i){
      r_int name_idx = name_index(indices.view(i), /*abort_on_missing = */ false);
      do_check = do_check || cppally::is_na(name_idx);
      matches.set(i, name_idx);
    }
    return subset(matches, /*invert=*/ invert, /*check=*/ do_check);
  } else if constexpr (RLogicalType<U>){
    if (is_long()){
      return subset(internal::clean_locs<r_int64>(indices, *this), /*invert=*/ invert, /*check=*/ false);
    } else {
      return subset(internal::clean_locs<r_int>(indices, *this), /*invert=*/ invert, /*check=*/ false);
    }
  } else {
    if (invert){
      if (is_long()){
        return subset(internal::exclude_locs<r_int64>(indices, length()), /*invert=*/ false, /*check=*/ false);
      } else {
        return subset(internal::exclude_locs<r_int>(indices, length()), /*invert=*/ false, /*check=*/ false);
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
        if (j < static_cast<unsigned_int_t>(xn)){
          out.set(i, view(static_cast<r_size_t>(j)));
        } else if (j > na_val) [[unlikely]] {
          // If j > n_val then it is a negative signed integer
          abort("Negative indices are unsupported, use `invert = true`");
        } else {
          if constexpr (RScalar<T>){
            out.set(i, na<T>());
          }
        }
      }
    } else {
      for (r_size_t i = 0; i < n; ++i){
        out.set(i, view(static_cast<r_size_t>(unwrap(indices.get(i)))));
    }
  }
  r_vec<r_str_view> nms = names();
  if (!nms.is_null()){
    r_vec<r_str_view> new_nms = nms.subset(indices, invert, check);
    out.set_names(new_nms);
  }
  return out;
}
}


// Free subset functions

template <RVector T, internal::RSubscript U>
inline T subset(const T& x, const r_vec<U>& indices, bool invert = false, bool check = true) {
  return x.subset(indices, invert, check);
}
template <internal::RSubscript U>
inline r_factors subset(const r_factors& x, const r_vec<U>& indices, bool invert = false, bool check = true) {
  return x.subset(indices, invert, check);
}

template <internal::RSubscript U>
inline r_sexp subset(const r_sexp& x, const r_vec<U>& indices, bool invert = false, bool check = true);

inline r_df subset(const r_df& x, const r_vec<r_int>& indices, bool invert = false, bool check = true){

  int ncol = x.ncol();

  if (ncol == 0){
    // We don't have a function atm that tells us what the resulting size should be here
    // So subset a dummy vector
    r_vec<r_int> dummy(x.nrow()); // Uninitialised dummy vector
    return r_df(r_vec<r_sexp>(), false, subset(dummy, indices, invert, check).length());
  }
  r_vec<r_sexp> out(ncol);
  for (int i = 0; i < ncol; ++i){
    out.set(i, subset(x.value.view(i), indices, invert, check));
  }
  out.set_names(x.colnames());
  return r_df(out, false, length(out.view(0)));
}

}

#endif
