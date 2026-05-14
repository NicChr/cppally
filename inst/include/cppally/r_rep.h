#ifndef CPPALLY_R_REP_H
#define CPPALLY_R_REP_H

#include <cppally/r_vec.h>
#include <cppally/sugar/r_stats.h>
#include <cppally/r_visit.h>
#include <cppally/r_copy.h>

namespace cppally {

template <RVector T>
T rep_len(const T& x, r_size_t n){
    return x.rep_len(n);
}
inline r_factors rep_len(const r_factors& x, r_size_t n){
    r_vec<r_int> out = x.value.rep_len(n);
    attr::set_attrs(out, attr::get_attrs(x));
    return r_factors(static_cast<SEXP>(out), false);
}

// Forward decl
inline r_sexp rep_len(const r_sexp& x, r_size_t n);

inline r_df rep_len(const r_df& x, r_size_t n){
    if (x.nrow() == n){
        return x;
      }
      r_df out = shallow_copy(x);
      out.set_nrow(n);
      int ncols = out.ncol();
      for (int i = 0; i < ncols; ++i){
          out.set_col(i, rep_len(out.view_col(i), n));
      }
      return out;
}

inline r_sexp rep_len(const r_sexp& x, r_size_t n){
    return r_sexp(CPPALLY_VIEW_AND_APPLY(x, /*return_type = */ SEXP, /*fn = */ rep_len, n));
}

template <RVector T>
T resize(const T& x, r_size_t n){
    return x.resize(n);
}
inline r_factors resize(const r_factors& x, r_size_t n){
    r_vec<r_int> out = x.value.resize(n);
    attr::set_attrs(out, attr::get_attrs(x));
    return r_factors(static_cast<SEXP>(out), false);
}

// Forward decl
inline r_sexp resize(const r_sexp& x, r_size_t n);

inline r_df resize(const r_df& x, r_size_t n){
    if (n == x.nrow()){
        return x;
    }
    r_df out = shallow_copy(x);
    out.set_nrow(n);
    int ncols = out.ncol();

    for (int i = 0; i < ncols; ++i){
        out.set_col(i, resize(out.view_col(i), n));
    }
    return out;
}

inline r_sexp resize(const r_sexp& x, r_size_t n){
    return r_sexp(CPPALLY_VIEW_AND_APPLY(x, /*return_type = */ SEXP, /*fn = */ resize, n));
}

template <RVector T>
T rep(const T& x, const r_vec<r_int>& times){
    
    r_size_t n = length(x);

    r_size_t out_size;
    r_size_t n_times = length(times);
    
    if (n_times == 1){
        out_size = n * times.data()[0];
        return rep_len(x, out_size);
    } else if (n_times == n){
        r_dbl out_size = sum(times, false);
        if (is_na(out_size)){
            abort("%s: `times` contains `NA` values", __func__);
        }
        T out(out_size);
        r_size_t k = 0;
        for (r_size_t i = 0; i < n; ++i){
          out.fill(k, times.data()[i], x.view(i));
          k += times.data()[i];
        }
        return out;
    } else {
        abort("%s: `length(times)` must be 1 or match `length(x)`", __func__);
    }
}

template <RVector T>
T rep_each(const T& x, const r_vec<r_int>& each){
  if (length(each) == 1){
    if (identical(each.get(0), r_int(1))){
      return x;
    }
    return rep(x, rep_len(each, length(x)));
  }
  return rep(x, each);
}

}

#endif
