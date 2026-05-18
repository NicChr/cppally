#ifndef CPPALLY_R_REPLACE_H
#define CPPALLY_R_REPLACE_H

#include <cppally/r_vec.h>

namespace cppally {

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

  for (r_size_t i = 0; i < n; 
    recycle_index(oldvi, oldv_size), 
    recycle_index(newvi, newv_size), 
    ++i){
      if (identical(view(i), old_values.view(oldvi))){
        set(i, new_values.view(newvi));
      }
  }
}

template <typename T, typename U>
requires requires(T& x, const U& old_values, const U& new_values){
  x.replace(old_values, new_values); 
}
void replace(T& x, const U& old_values, const U& new_values){
  x.replace(old_values, new_values);
}

template <RStringType U>
void replace(r_factors& x, const r_vec<U>& old_values, const r_vec<U>& new_values){
  r_vec<r_int> old_codes = x.get_codes(old_values, r_int(-1));
  r_vec<r_int> new_codes = x.get_codes(new_values, r_int(-1));
  x.value.replace(old_codes, new_codes);
  x.value.replace(r_int(-1), na<r_int>());
}

inline void replace(r_factors& x, const r_factors& old_values, const r_factors& new_values){
  r_vec<r_int> old_codes = old_values.new_codes(x.levels(), r_int(-1));
  r_vec<r_int> new_codes = new_values.new_codes(x.levels(), r_int(-1));
  x.value.replace(old_codes, new_codes);
  x.value.replace(r_int(-1), na<r_int>());
}

template <typename U>
inline void replace(r_sexp& x, const U& old_values, const U& new_values);

inline void replace(r_df& x, const r_df& old_values, const r_df& new_values){
  r_size_t ncols = x.ncol();

  if (ncols != old_values.ncol() || ncols != new_values.ncol()){
    abort("%s: Number of cols must match between `x`, `old_values` and `new_values`", __func__);
  }

  for (r_size_t i = 0; i < ncols; ++i){
    r_str colname = r_str(x.colnames().view(i));
    r_sexp col = x.get_col(i);
    replace(col, old_values.get_col(colname), new_values.get_col(colname));
  }
}


template <typename U>
inline void replace(r_sexp& x, const U& old_values, const U& new_values){
  mutate_sexp(x, [&](auto& x_) {
    using x_t = std::remove_cvref_t<decltype(x_)>;
    x_t oldv = x_t(static_cast<SEXP>(old_values));
    x_t newv = x_t(static_cast<SEXP>(new_values));
    if constexpr (is<x_t, r_sexp>){
        abort("Unsupported SEXP type in `replace()`");
    } else if constexpr (requires { replace(x_, oldv, newv); }){
      replace(x_, oldv, newv);
    } else {
      abort("No available method for type %s in `replace`", internal::type_str<x_t>());
    }
  });
}

}

#endif
