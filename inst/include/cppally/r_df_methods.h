#ifndef CPPALLY_R_DF_METHODS_H
#define CPPALLY_R_DF_METHODS_H

// Methods for r_df that requires r_visit.h

#include <cppally/r_vec.h>
#include <cppally/r_attrs.h>
#include <cppally/r_visit.h>
#include <cppally/r_rep.h>
#include <cppally/r_list_helpers.h>
#include <cppally/sugar/r_subset.h>
#include <string>

namespace cppally {

namespace internal {

inline r_vec<r_sexp> new_df_impl(const r_vec<r_sexp>& cols, bool recycle, int nrows){

    r_size_t n = cols.length();
    r_vec<r_sexp> out(n);

    if (nrows < 0){
        abort("Supply a valid `nrows`");
    }

    if (recycle){
        for (r_size_t i = 0; i < n; ++i){
            out.set(i, rep_len(cols.view(i), nrows));
        }
    } else {
        for (r_size_t i = 0; i < n; ++i){
            if (static_cast<int>(length(cols.view(i))) != nrows) [[unlikely]] {
                abort("new_df_impl: lengths of cols must match `nrows`");
            }
            out.set(i, cols.view(i));
        }
    }

    // Always provide names
    r_vec<r_str_view> names = cols.names();
    if (names.is_null()){
        names = r_vec<r_str_view>(n);
        for (r_size_t i = 0; i < n; ++i){
            r_str elem( (std::string("col_") + std::to_string(i + 1)).c_str() );
            names.set(i, elem);
        } 
    }
    out.set_names(names); 

    r_vec<r_int> row_names = create_row_names(nrows);
    attr::set_attr(out, symbol::class_sym, r_vec<r_str_view>(1, r_str_view(cached_str<"data.frame">())));
    attr::set_attr(out, symbol::row_names_sym, row_names);
    return out;
}

inline r_vec<r_sexp> new_df_impl(const r_vec<r_sexp>& cols, bool recycle = true){
    r_size_t nrows;
    if (recycle){
        nrows = internal::recycle_size(cols);
    } else {
        if (cols.length() == 0){
            nrows = 0;
        } else {
            nrows = length(cols.view(0));
        }
    }
    return new_df_impl(cols, recycle, nrows);
}

}

// Constructor from list of cols
// Supply a nrows value for a custom recycle length
inline r_df::r_df(const r_vec<r_sexp>& cols, bool recycle) : value(internal::new_df_impl(cols, recycle)){
    init_df();
}
inline r_df::r_df(const r_vec<r_sexp>& cols, bool recycle, int nrows) : value(internal::new_df_impl(cols, recycle, nrows)){
    nrow_ = nrows;
}
// Atomic vector constructor
template <RScalar T>
inline r_df::r_df(const r_vec<T>& col) : r_df(r_vec<r_sexp>(1, r_sexp(static_cast<SEXP>(col), internal::view_tag{})), false, static_cast<int>(col.length())) {}
// Factor constructor
inline r_df::r_df(const r_factors& col) : r_df(col.value){}

inline r_df r_df::get_row(int index) const {
    return subset(*this, r_vec<r_int>(1, r_int(index)), false, false);
}

// inline r_vec<r_sexp> r_df::get_row(int index) const {
//     int ncols = ncol();
//     r_vec<r_sexp> out(ncols);
//     attr::set_old_names(out, colnames());
//     for (int i = 0; i < ncols; ++i){
//         out.set(i, r_sexp(view_sexp(value.view(i), [index](const auto& vec) -> SEXP {
//             using vec_t = decltype(vec);
//             if constexpr (RVector<vec_t> || RFactor<vec_t>){
//                 return as<SEXP>(vec.view(index));
//             } else {
//                 abort("error");
//             }
//         }), internal::view_tag{}));
//     }
//     return out;
// }

inline r_sexp r_df::get_col(int index) const {
    return subset(value, r_vec<r_int>(1, r_int(index)), false, false).get(0);
}

template <RStringType U>
inline r_sexp r_df::get_col(U name) const {
    r_vec<r_sexp> sset = subset(value, r_vec<U>(1, name), true, false);
    if (sset.length() == 0){
        return sset.sexp;
    } else {
        return sset.get(0);
    }
}

inline r_sexp r_df::get_col(const char* name) const {
    return get_col(r_str(name));
}

template <internal::RSubscript U>
inline r_df r_df::select(const r_vec<U>& cols) const {
    return r_df(value.subset(cols), false, nrow());
}

}

#endif
