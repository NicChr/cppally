#ifndef CPPALLY_R_COMBINE_H
#define CPPALLY_R_COMBINE_H

#include <cppally/r_coerce.h>
#include <cppally/r_list_helpers.h>
#include <cppally/sugar/r_subset.h>
#include <cppally/sugar/r_rep.h>
#include <cppally/sugar/r_make_vec.h>
#include <cppally/sugar/r_sexp_methods.h>

namespace cppally {

namespace internal {
inline r_vec<r_str_view> combine_levels(const r_vec<r_str_view>& x_lvls, const r_vec<r_str_view>& y_lvls){
    r_vec<r_int> new_lvl_locations = y_lvls.find(x_lvls, /*invert = */ true);

    if (new_lvl_locations.length() == 0){
        return x_lvls;
    }

    r_vec<r_str_view> new_lvls = y_lvls.subset(new_lvl_locations);

    r_size_t n_lvls = x_lvls.length() + new_lvls.length();
    r_vec<r_str_view> out(n_lvls);
    r_copy_n(out, x_lvls, 0, x_lvls.length());
    r_copy_n(out, new_lvls, x_lvls.length(), new_lvls.length());
    return out;
}
}

inline void r_copy_n(r_factors& target, const r_factors& source, r_size_t target_offset, r_size_t n){
    if (identical(target.levels(), source.levels())){
        r_copy_n(target.value, source.value, target_offset, n);
        return;   
    }
    r_vec<r_str_view> all_levels = internal::combine_levels(target.levels(), source.levels());
    target.set_levels(all_levels);
    r_vec<r_int> new_codes = source.new_codes(all_levels);
    r_copy_n(target.value, new_codes, target_offset, n);

}

inline void r_copy_n(r_df& target, const r_df& source, r_size_t target_offset, r_size_t n){
    if (n == 0) return;
    r_size_t ncols = static_cast<r_size_t>(target.ncol());
    r_vec<r_str_view> colnames = target.colnames();
    for (r_size_t j = 0; j < ncols; ++j){
        r_str_view colname = colnames.view(j);
        target.with_col(j, [&]<typename col_t>(col_t&& tcol) -> void {
            r_copy_n(tcol, col_t(source.get_col(colname)), target_offset, n);
        }, true);
    }
}

template <typename T>
T flatten(const r_vec<r_sexp>& x) = delete;

// Flatten a list of vectors, factors or data frames
// to a single vector, factor or data frame
template <RComposite T>
T flatten(const r_vec<r_sexp>& x) {

    r_vec<r_sexp> vectors = x.remove(r_null);

    r_size_t n = vectors.length();
    
    if (n == 0){
        return T();
    }

    r_size_t out_size = 0;
    for (r_size_t i = 0; i < n; ++i) {
        out_size += length(vectors.view(i));
    }

    // Grab first vector from list and resize it to final size
    T first_vec = as<T>(vectors.get(0));
    // resize is best here as it resizes, doesn't initialise extra memory
    // and preserves attributes
    T out = resize(first_vec, out_size);

    r_size_t k = length(first_vec), m;
    for (r_size_t i = 1; i < n; k += m, ++i) {
        T vec = as<T>(vectors.view(i));
        m = length(vec);
        r_copy_n(out, vec, k, m);
    }
    return out;
}

// Flatten a list of vectors, factors or data frames to a single
// vector, factor or data frame
inline r_sexp flatten(const r_vec<r_sexp>& x) {

    r_vec<r_sexp> vectors = x.remove(r_null);

    r_size_t n = vectors.length();

    if (n == 0){
        return r_null;
    }

    r_size_t out_size = 0;
    for (r_size_t i = 0; i < n; ++i) {
        out_size += length(vectors.view(i));
    }

    // Grab first vector from list and resize it to final size
    // resize is best here as it resizes, doesn't initialise extra memory
    // and preserves attributes
    r_sexp out = resize(vectors.get(0), out_size);

    r_size_t k = length(vectors.view(0)), m;
    for (r_size_t i = 1; i < n; k += m, ++i){
        r_sexp next = vectors.view(i);
        m = length(next);
        // Rolling common type
        out = visit_sexp(out, [&]<typename out_t>(out_t&& out_vec) -> r_sexp {
            return visit_sexp(next, [&]<typename next_t>(const next_t& next_vec) -> r_sexp {
                if constexpr (RComposite<out_t> && RComposite<next_t>){
                    using common_t = common_r_t<out_t, next_t>;
                    if constexpr (is<out_t, common_t>){
                        r_copy_n(out_vec, as<common_t>(next_vec), k, m);
                        return out;
                    } else {
                        // Promote out to common type, then write
                        common_t out_promoted = as<common_t>(out_vec);
                        r_copy_n(out_promoted, next_vec, k, m);
                        return as<r_sexp>(out_promoted);
                    }
                } else {
                    abort("flatten: unsupported element type %s", internal::type_str<next_t>());
                }
            });
        });
    }
    return out;
}
  
template <typename... Args>
auto combine(Args... args){
    using common_t = common_r_t<as_r_composite_t<Args>...>;
    r_vec<r_sexp> list_of_args = make_vec<r_sexp>(args...);
    return flatten<common_t>(list_of_args);
}

}

#endif
