#ifndef CPPALLY_R_EQUAL_H
#define CPPALLY_R_EQUAL_H


#include <cppally/r_vec_ops.h>
#include <cppally/r_visit.h>
#include <cppally/r_pmap.h>
#include <cppally/r_length.h>
#include <cppally/r_identical.h>
#include <cppally/sugar/r_df_methods.h>
#include <cppally/sugar/r_sexp_methods.h>

namespace cppally {

// Equality operator for lists
// list-comparison becomes element-wise `identical()` comparison
inline r_vec<r_lgl> operator==(const r_vec<r_sexp>& lhs, const r_vec<r_sexp>& rhs){
  return pmap([](auto a, auto b){ return r_lgl(identical(a, b)); }, lhs, rhs);
}

// r_factors vectorised operators

inline r_vec<r_lgl> operator==(const r_factors& lhs, const r_factors& rhs) {
  r_vec<r_int> fct_codes = lhs.value;
  r_vec<r_int> comparable_codes = rhs.new_codes(lhs.levels(), r_int(-1));
  return fct_codes == comparable_codes;
}

template <RStringType U>
inline r_vec<r_lgl> operator==(const r_factors& lhs, const r_vec<U>& rhs) {
  r_vec<r_int> fct_codes = lhs.value;
  r_vec<r_int> comparable_codes = lhs.get_codes(rhs, r_int(-1));
  return fct_codes == comparable_codes;
}

template <RStringType U>
inline r_vec<r_lgl> operator==(const r_vec<U>& lhs, const r_factors& rhs) {
  r_vec<r_int> comparable_codes = rhs.get_codes(lhs, r_int(-1));
  r_vec<r_int> fct_codes = rhs.value;
  return fct_codes == comparable_codes;
}

// Forward decl for r_df
template <typename T, typename U>
requires (is<T, r_sexp> || is<U, r_sexp>)
inline r_vec<r_lgl> operator==(const T& lhs, const U& rhs);

inline r_vec<r_lgl> operator==(const r_df& lhs, const r_df& rhs) {
    if (lhs.ncol() != rhs.ncol()){
        abort("`operator==`: `lhs.ncol()` must match `rhs.ncol()`");
    }

    if (lhs.nrow() == 0 || rhs.nrow() == 0){
        return r_vec<r_lgl>();
    }

    r_size_t ncols = lhs.ncol();
    r_vec<r_str_view> colnames = lhs.colnames();

    r_size_t out_size = std::max(lhs.nrow(), rhs.nrow());
    r_vec<r_lgl> out(out_size, r_true);
    
    for (r_size_t i = 0; i < ncols; ++i){
        r_str_view colname = colnames.view(i);
        r_sexp lhs_col = lhs.view_col(colname);
        r_sexp rhs_col = rhs.view_col(colname);

        lhs.with_col(colname, [&]<typename lhs_t>(const lhs_t& left_col) -> void {
            lhs_t right_col = lhs_t(rhs_col); // Assume right col is same type as left col (avoiding double visit dispatch)
            if constexpr (RDataFrame<lhs_t> || RListVector<lhs_t>){
              r_vec<r_lgl> cols_eq = left_col == right_col;
              for (int j = 0; j < out_size; ++j){
                out.set(j, out.get(j) && cols_eq.get(j));
              }
            } else {
              int leftn = length(left_col);
              int rightn = length(right_col);
              for (int j = 0, li = 0, ri = 0; j < out_size;
                recycle_index(li, leftn), 
                recycle_index(ri, rightn), ++j) {
                  out.set(j, out.get(j) && (left_col.view(li) == right_col.view(ri)));
                }
              }
            });

    }
    return out;
}

template <typename T, typename U>
requires (is<T, r_sexp> || is<U, r_sexp>)
inline r_vec<r_lgl> operator==(const T& lhs, const U& rhs) {
  if constexpr (is<T, r_sexp>){
    return internal::visit_sexp(lhs, [&]<typename lhs_t>(const lhs_t& x) -> r_vec<r_lgl> {
      return as<r_vec<r_lgl>>(x == lhs_t(rhs));
    });
  } else {
    return internal::visit_sexp(rhs, [&]<typename rhs_t>(const rhs_t& y) -> r_vec<r_lgl> {
      return as<r_vec<r_lgl>>(rhs_t(lhs) == y);
    });
  }
}

inline r_vec<r_lgl> operator!=(const r_factors& lhs, const r_factors& rhs) {
  return internal::not_equal(lhs, rhs);
}

template <RStringType U>
inline r_vec<r_lgl> operator!=(const r_factors& lhs, const r_vec<U>& rhs) {
  return internal::not_equal(lhs, rhs);
}

template <RStringType U>
inline r_vec<r_lgl> operator!=(const r_vec<U>& lhs, const r_factors& rhs) {
  return internal::not_equal(lhs, rhs);
}

inline r_vec<r_lgl> operator!=(const r_df& lhs, const r_df& rhs) {
  return internal::not_equal(lhs, rhs);
}

inline r_vec<r_lgl> operator!=(const r_vec<r_sexp>& lhs, const r_vec<r_sexp>& rhs) {
  return internal::not_equal(lhs, rhs);
}

template <typename T, typename U>
requires (is<T, r_sexp> || is<U, r_sexp>)
inline r_vec<r_lgl> operator!=(const T& lhs, const U& rhs) {
  return internal::not_equal(lhs, rhs);
}

}

#endif
