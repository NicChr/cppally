#ifndef CPPALLY_R_COMBINE_H
#define CPPALLY_R_COMBINE_H

#include <cppally/r_coerce.h>
#include <cppally/r_list_helpers.h>
#include <cppally/sugar/r_subset.h>
#include <cppally/sugar/r_make_vec.h>

namespace cppally {

// namespace internal {
// inline r_vec<r_str_view> combine_levels(const r_vec<r_str_view>& x_lvls, const r_vec<r_str_view>& y_lvls){

//     r_vec<r_str_view> new_lvls = y_lvls.subset(y_lvls.find(x_lvls, /*invert = */ true));
    
//     if (new_lvls.length() == 0){
//         return x_lvls;
//     }

//     r_size_t n_lvls = x_lvls.length() + new_lvls.length();
//     r_vec<r_str_view> out(n_lvls);
//     r_copy_n(out, x_lvls, 0, x_lvls.length());
//     r_copy_n(out, new_lvls, x_lvls.length(), new_lvls.length());
//     return out;
// }
// }

template <typename T>
T flatten(const r_vec<r_sexp>& x) = delete;

template <RVector T>
T flatten(const r_vec<r_sexp>& x) {
    
    r_size_t n = x.length();
    r_size_t out_size = 0;
    for (r_size_t i = 0; i < n; ++i) {
        out_size += length(x.view(i));
    }

    T out(out_size);
    r_size_t k = 0; 
    r_size_t m;

    for (r_size_t i = 0; i < n; k += m, ++i) {
        T vec = as<T>(x.view(i));
        m = length(vec);
        r_copy_n(out, vec, k, m);
    }
    return out;
}

// template <>
// inline r_factors flatten<r_factors>(const r_vec<r_sexp>& x) {
    
//     r_size_t n = x.length();
//     r_size_t out_size = 0;
//     for (r_size_t i = 0; i < n; ++i) {
//         out_size += length(x.view(i));
//     }

//     r_vec<r_str_view> out(out_size);
//     r_vec<r_str_view> all_levels{};
//     r_size_t k = 0; 
//     r_size_t m;

//     for (r_size_t i = 0; i < n; k += m, ++i) {
//         r_factors fct = as<r_factors>(x.view(i));
//         all_levels = combine_levels(all_levels, fct.levels());
//         m = length(fct);
//         r_copy_n(out, as<r_vec<r_str_view>>(fct), k, m);
//     }
//     return r_factors(out, all_levels);
// }
  
template <typename... Args>
auto combine(Args... args){
    using common_t = common_r_t<as_r_composite_t<Args>...>;
    r_vec<r_sexp> list_of_args = make_vec<r_sexp>(args...);
    return flatten<common_t>(list_of_args);
}

}

#endif
