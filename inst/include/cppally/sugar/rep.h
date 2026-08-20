#ifndef CPPALLY_R_REP_H
#define CPPALLY_R_REP_H

#include <cppally/vector/r_vector.h>
#include <cppally/stats/stats.h>
#include <cppally/sugar/copy.h>
#include <cppally/length.h>

namespace cppally {

template <RComposite T>
requires (requires (const T& obj, r_size_t n) { obj.rep_len(n); })
T rep_len(const T& x, r_size_t n){
    return x.rep_len(n);
}

// Forward decl
inline r_sexp rep_len(const r_sexp& x, r_size_t n);

inline r_df rep_len(const r_df& x, r_size_t n){
    if (x.nrow() == n){
        return x;
      }
      r_df out = x.copy();
      out.set_nrow(n);
      int ncols = out.ncol();
      for (int i = 0; i < ncols; ++i){
          out.set_col(i, rep_len(out.view_col(i), n));
      }
      return out;
}

inline r_sexp rep_len(const r_sexp& x, r_size_t n) {
    return r_sexp_view(x, CPPALLY_MAKE_VISITOR(r_sexp, v, rep_len(v, n)));
}

template <RComposite T>
requires (requires (const T& obj, r_size_t n) { obj.resize(n); })
T resize(const T& x, r_size_t n){
    return x.resize(n);
}

// Forward decl
inline r_sexp resize(const r_sexp& x, r_size_t n);

inline r_df resize(const r_df& x, r_size_t n){
    if (x.nrow() == n){
        return x;
      }
      r_df out = x.copy();
      out.set_nrow(n);
      int ncols = out.ncol();
      for (int i = 0; i < ncols; ++i){
          out.set_col(i, resize(out.view_col(i), n));
      }
      return out;
}

inline r_sexp resize(const r_sexp& x, r_size_t n) {
    return r_sexp_view(x, CPPALLY_MAKE_VISITOR(r_sexp, v, resize(v, n)));
}

template <RVector T>
T rep(const T& x, const r_vec<r_int>& times){
    
    r_size_t n = length(x);

    r_size_t out_size;
    r_size_t n_times = length(times);
    
    if (n_times == 1){
        out_size = n * unwrap(times.get(0));
        return rep_len(x, out_size);
    } else if (n_times == n){
        auto s = sum(times, false);
        if (is_na(s)){
            abort("%s: `times` contains `NA` values", __func__);
        }
        T out(static_cast<r_size_t>(unwrap(s)));
        r_size_t k = 0;
        for (r_size_t i = 0; i < n; ++i){
          out.fill(k, unwrap(times.get(i)), x.view(i));
          k += unwrap(times.get(i));
        }
        return out;
    } else {
        abort("%s: `length(times)` must be 1 or match `length(x)`", __func__);
    }
}

inline r_factors rep(const r_factors& x, const r_vec<r_int>& times){
    return r_factors(rep(x.value, times), x.levels());
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

inline r_factors rep_each(const r_factors& x, const r_vec<r_int>& each){
    return r_factors(rep_each(x.value, each), x.levels());
}

}

#endif
