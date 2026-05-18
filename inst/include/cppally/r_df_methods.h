#ifndef CPPALLY_R_DF_METHODS_H
#define CPPALLY_R_DF_METHODS_H

// Methods for r_df that requires r_visit.h

#include <cppally/r_vec.h>
#include <cppally/r_attrs.h>
#include <cppally/r_visit.h>
#include <cppally/r_coerce.h>
#include <cppally/sugar/r_rep.h>
#include <cppally/r_list_helpers.h>
#include <cppally/sugar/r_subset.h>
#include <cppally/sugar/r_make_vec.h>
#include <string>

namespace cppally {

namespace internal {

inline r_vec<r_sexp> new_df_impl(const r_vec<r_sexp>& cols, bool recycle, int nrows){

    r_size_t n = cols.length();
    r_vec<r_sexp> out(n);

    if (nrows < 0) [[unlikely]] {
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
    attr::set_attr(out, symbol::class_sym, data_frame_class());
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
    cached_nrow = nrows;
}
// Atomic vector constructor
template <RScalar T>
inline r_df::r_df(const r_vec<T>& col) : r_df(r_vec<r_sexp>(1, r_sexp(static_cast<SEXP>(col), internal::view_tag{})), false, static_cast<int>(col.length())) {}
// Factor constructor
inline r_df::r_df(const r_factors& col) : r_df(col.value){}

inline r_df r_df::get_row(int index) const {
    int ncols = ncol();
    r_vec<r_sexp> out(ncols);
    out.set_names(colnames());
    attr::set_old_class(out, internal::data_frame_class());
    attr::set_attr(out, symbol::row_names_sym, internal::create_row_names(1));
    for (int i = 0; i < ncols; ++i){
        out.set(i, r_sexp(view_sexp(value.view(i), [index](const auto& vec) -> SEXP {
            using vec_t = std::remove_cvref_t<decltype(vec)>;
            if constexpr (requires { vec.view(index); }){
                return as<SEXP>(vec.view(index));
            } else {
                abort("No view member exists for type %s", internal::type_str<vec_t>());
            }
        }), internal::view_tag{}));
    }
    return r_df(out, 1, internal::no_checks_tag{});
}

template <internal::RSubscript U>
inline r_df r_df::select(const r_vec<U>& cols) const {
    return r_df(value.subset(cols), false, nrow());
}

inline r_vec<r_str> r_df::rownames() const {
    return as<r_vec<r_str>>(attr::get_attr(value, symbol::row_names_sym));
}

// inline void check_compatible_dfs(const r_df& x, const r_df& y){
//     if (!identical(x.colnames(), y.colnames())) [[unlikely]] {
//         abort("(compatible_dfs): `x` and `y` must have the same colnames");
//     }
//     if (!identical(x.nrow(), y.nrow())) [[unlikely]] {
//         abort("(compatible_dfs): `x` and `y` must have the same nrows");
//     }
//     // If col types aren't the same..
//     //     abort("(compatible_dfs): `x` and `y` must have the same nrows");
//     // }
// }

// template <internal::RSubscript T, internal::RSubscript U>
// void r_df::fill(const r_vec<T>& row_indices, const r_vec<U>& col_indices, const r_df& replacement) {

//     r_vec<r_int> row_locs = internal::clean_locs(row_indices, *this);
//     r_vec<r_int> col_locs = internal::clean_locs(col_indices, *this);

//     int ncols = col_locs.length();

//     if (ncols != replacement.ncol()){
//         abort("fill: `replacement.ncol()` must equal data frame ncol");
//     }

//     for (int i = 0; i < ncols; ++i){
//         int col_loc = col_locs.get(i);
//         r_sexp col = get_col(col_loc);
//         cppally::fill(col, row_locs, replacement.value.view(i));
//     }

// }

// template <internal::RSubscript T>
// void r_df::fill(const r_vec<T>& row_indices, const r_df& replacement) {
//     r_vec<r_int> col_locs(ncol());
//     col_locs.iota();
//     this->fill(row_indices, col_locs, replacement);
// }

// Make in-line data frame
template <typename... Args>
inline r_df make_df(Args&&... args) {
    return r_df(make_vec<r_sexp>(std::forward<Args>(args)...), /*recycle = */ true);
}

// Dispatch on the i-th column's wrapped type, gated to RComposite.
// Non-RComposite SEXP types (r_sym, fallback r_sexp) abort.
template <typename index_t, class F>
decltype(auto) r_df::with_col(const index_t& index, F&& f, bool view_only) const {
    if (view_only){
        return view_sexp(view_col(index),
        [&]<typename T>(T&& x) -> decltype(f(std::declval<r_vec<r_lgl>>())) {
          using U = std::remove_cvref_t<T>;
          if constexpr (RComposite<U>) {
              return f(std::forward<T>(x));
          } else {
              abort("col_apply: r_df can't handle column type: %s", internal::type_str<U>());
          }
        });
    } else {
        return visit_sexp(view_col(index),
        [&]<typename T>(T&& x) -> decltype(f(std::declval<r_vec<r_lgl>>())) {
          using U = std::remove_cvref_t<T>;
          if constexpr (RComposite<U>) {
            return f(std::forward<T>(x));
          } else {
            abort("col_apply: r_df can't handle column type: %s", internal::type_str<U>());
          }
        });
    }
}

}

#endif
