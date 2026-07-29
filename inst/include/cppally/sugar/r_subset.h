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

    // static_assert(!is<V, r_int64>, "Cannot perform named-subsetting on long vectors");
    static_assert(!is<T, r_df>, "Named-subsetting of r_df is unsupported, use `r_df.select()`");

    if (x.names().is_null()){
      abort("Cannot subset on the names of an unnamed vector");
    }

    // Long-vector name lookup is caught at runtime by name_index/name hashing.
    std::vector<unwrap_t<V>> matches;
    matches.reserve(n);

    for (r_size_t i = 0; i < n; ++i){
      r_int name_idx = x.name_index(locs.view(i), /*abort_on_missing = */ false);
      if (!is_na(name_idx)){
        matches.push_back(unwrap(name_idx));
      }
    }
    return as<r_vec<V>>(matches);
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

template <RVector T, internal::RSubscript U>
inline T subset(const T& x, const r_vec<U>& indices, bool invert = false, bool check = true) {

  using data_t = typename std::remove_cvref_t<T>::data_type;
  
  if (indices.is_null()){
    return x;
  }
  
  if constexpr (RStringType<U>){
    if (x.is_long()){
        abort("%s: Named subsetting on long-vectors is unsupported", __func__);
    }

    r_size_t n = indices.length();
    r_vec<r_int> matches(n);
    bool do_check = false;
    for (r_size_t i = 0; i < n; ++i){
      r_int name_idx = x.name_index(indices.view(i), /*abort_on_missing = */ false);
      do_check = do_check || is_na(name_idx);
      matches.set(i, name_idx);
    }
    return subset(x, matches, /*invert=*/ invert, /*check=*/ do_check);
  } else if constexpr (RLogicalType<U>){
    if (x.is_long()){
      return subset(x, internal::clean_locs<r_int64>(indices, x), /*invert=*/ invert, /*check=*/ false);
    } else {
      return subset(x, internal::clean_locs<r_int>(indices, x), /*invert=*/ invert, /*check=*/ false);
    }
  } else {
    if (invert){
      if (x.is_long()){
        return subset(x, internal::exclude_locs<r_int64>(indices, x.length()), /*invert=*/ false, /*check=*/ false);
      } else {
        return subset(x, internal::exclude_locs<r_int>(indices, x.length()), /*invert=*/ false, /*check=*/ false);
      }
    }

    using unsigned_int_t = std::make_unsigned_t<unwrap_t<U>>;
    r_size_t n = indices.length();

    T out = [&]() -> T {
      if (check){
        T local_out(n);
    
        r_size_t xn = x.length();
        unsigned_int_t na_val = unwrap(na<U>());
        unsigned_int_t j;
    
        for (r_size_t i = 0; i < n; ++i){
          j = unwrap(indices.get(i));
          if (j < static_cast<unsigned_int_t>(xn)){
            local_out.set(i, x.view(static_cast<r_size_t>(j)));
          } else if (j > na_val) [[unlikely]] {
            abort("Negative indices are unsupported, use `invert = true`");
          } else {
            if constexpr (requires { data_t::na(); }){
              local_out.set(i, na<data_t>());
            }
          }
        }
        return local_out;
      } else {
        return pmap_parallel_simd([&x](U idx){ return x.view(static_cast<r_size_t>(unwrap(idx)));}, indices);
      }
    }();

    r_vec<r_str_view> nms = x.names();
    if (!nms.is_null()){
      r_vec<r_str_view> new_nms = subset(nms, indices, invert, check);
      out.set_names(new_nms);
    }
    return out;
  }
}
template <internal::RSubscript U>
inline r_factors subset(const r_factors& x, const r_vec<U>& indices, bool invert = false, bool check = true) {
  return r_factors(subset(x.value, indices, invert, check), x.levels(), false);
}

template <internal::RSubscript U>
inline r_sexp subset(const r_sexp& x, const r_vec<U>& indices, bool invert = false, bool check = true);

template <internal::RSubscript U>
requires (!RStringType<U>)
inline r_df subset(const r_df& x, const r_vec<U>& indices, bool invert = false, bool check = true){

  if (indices.is_null()){
    return x;
  }

  // Normalise logicals and `invert` here so each column sees plain positive indices
  if constexpr (RLogicalType<U>){
    return subset(x, internal::clean_locs<r_int>(indices, x), invert, /*check=*/ false);
  } else {
    if (invert){
      return subset(x, internal::exclude_locs<r_int>(indices, x.nrow()), /*invert=*/ false, /*check=*/ false);
    }

    int ncol = x.ncol();

    if (ncol == 0){
      return r_df(r_vec<r_sexp>(), false, static_cast<int>(indices.length()));
    }
    r_vec<r_sexp> out(ncol);
    for (int i = 0; i < ncol; ++i){
      out.set(i, subset(x.value.view(i), indices, /*invert=*/ false, check));
    }
    out.set_names(x.colnames());
    return r_df(out, false, length(out.view(0)));
  }
}

}

#endif
