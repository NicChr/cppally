#ifndef CPPALLY_R_REP_H
#define CPPALLY_R_REP_H

#include <cppally/r_vec.h>
#include <cppally/r_visit.h>

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
    r_df out(x.value, false, x.nrow());
    for (r_size_t i = 0; i < x.ncol(); ++i){
        out.value.set(i, rep_len(out.value.view(i), n));
    }
    out.set_nrow(n);
    return out;
}

inline r_sexp rep_len(const r_sexp& x, r_size_t n){
    return r_sexp(CPPALLY_VIEW_AND_APPLY(x, /*return_type = */ SEXP, /*fn = */ rep_len, n));
}

}

#endif
