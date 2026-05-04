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
    if (n == x.nrow()){
        return x;
    }
    int ncols = x.ncol();
    r_vec<r_sexp> cols(ncols);
    for (int i = 0; i < ncols; ++i){
        cols.set(i, rep_len(x.value.view(i), n));
    }
    attr::set_attrs(cols, attr::get_attrs(x));
    // SHALLOW_DUPLICATE_ATTRIB(cols, x);
    return r_df(cols, n, internal::no_checks_tag{});
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
    int ncols = x.ncol();
    r_vec<r_sexp> out(ncols);
    for (int i = 0; i < ncols; ++i){
        out.set(i, resize(x.value.view(i), n));
    }
    attr::set_old_names(out, attr::get_old_names(x));
    return r_df(out, false, n);
}

inline r_sexp resize(const r_sexp& x, r_size_t n){
    return r_sexp(CPPALLY_VIEW_AND_APPLY(x, /*return_type = */ SEXP, /*fn = */ resize, n));
}

}

#endif
