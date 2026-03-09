#ifndef CPP20_R_VISIT_H
#define CPP20_R_VISIT_H

#include <cpp20/internal/r_vec.h>
#include <cpp20/internal/r_factor.h>

namespace cpp20 {

// A cleaner lambda-based alternative to
// using the canonical switch(TYPEOF(x))

template <class F>
decltype(auto) visit_sexp(SEXP x, F&& f) {
switch (internal::CPP20_TYPEOF(x)) {
    case LGLSXP:                        return f(r_vec<r_lgl>(x));
    case INTSXP:                        return f(r_vec<r_int>(x));
    case internal::CPP20_INT64SXP:      return f(r_vec<r_int64>(x));
    case REALSXP:                       return f(r_vec<r_dbl>(x));
    case STRSXP:                        return f(r_vec<r_str>(x));
    case VECSXP:                        return f(r_vec<r_sexp>(x));
    case CPLXSXP:                       return f(r_vec<r_cplx>(x));
    case RAWSXP:                        return f(r_vec<r_raw>(x));
    case internal::CPP20_INTDATESXP:    return f(r_vec<r_date_t<r_int>>(x));
    case internal::CPP20_REALDATESXP:   return f(r_vec<r_date_t<r_dbl>>(x));
    case internal::CPP20_INT64PSXTSXP:  return f(r_vec<r_psxct_t<r_int64>>(x));
    case internal::CPP20_REALPSXTSXP:   return f(r_vec<r_psxct_t<r_dbl>>(x));
    // case internal::CPP20_FCTSXP:        return f(r_factors(x));
    // case CPP20_DFSXP:                return f(r_df(x));
    default:                            return f(r_sexp(x));
}
}

template <class F>
decltype(auto) visit_sexp(const r_sexp& x, F&& f) {
switch (internal::CPP20_TYPEOF(x)) {
    case LGLSXP:                        return f(r_vec<r_lgl>(x));
    case INTSXP:                        return f(r_vec<r_int>(x));
    case internal::CPP20_INT64SXP:      return f(r_vec<r_int64>(x));
    case REALSXP:                       return f(r_vec<r_dbl>(x));
    case STRSXP:                        return f(r_vec<r_str>(x));
    case VECSXP:                        return f(r_vec<r_sexp>(x));
    case CPLXSXP:                       return f(r_vec<r_cplx>(x));
    case RAWSXP:                        return f(r_vec<r_raw>(x));
    case internal::CPP20_INTDATESXP:    return f(r_vec<r_date_t<r_int>>(x));
    case internal::CPP20_REALDATESXP:   return f(r_vec<r_date_t<r_dbl>>(x));
    case internal::CPP20_INT64PSXTSXP:  return f(r_vec<r_psxct_t<r_int64>>(x));
    case internal::CPP20_REALPSXTSXP:   return f(r_vec<r_psxct_t<r_dbl>>(x));
    // case internal::CPP20_FCTSXP:        return f(r_factors(x));
    // case CPP20_DFSXP:                return f(r_df(x));
    default:                            return f(r_sexp(x));
}
}


// visits all elements, visitor receives (r_size_t i, r_vec<T> elem)
template <typename Visitor>
void visit_list_elements(const r_vec<r_sexp>& x, Visitor&& vis) {
    r_size_t n = x.length();
    for (r_size_t i = 0; i < n; ++i) {
        visit_sexp(x.view(i), [&]<typename T>(T elem) {
            if constexpr (is<T, r_sexp>){
                abort("Don't know how to deal with object of type %s", Rf_type2char(TYPEOF(elem)));
            } else {
                vis(i, std::move(elem));
            }
        });
    }
}

}

#endif
