#ifndef CPPALLY_R_COMBINE_H
#define CPPALLY_R_COMBINE_H

#include <cppally/r_coerce.h>
#include <cppally/r_list_helpers.h>
#include <cppally/sugar/r_subset.h>
#include <cppally/sugar/r_make_vec.h>

namespace cppally {

// inline void r_copy_n(r_factors& target, const r_factors& source, r_size_t target_offset, r_size_t n){
//     r_vec<r_str_view> target_lvls = target.levels();
//     r_vec<r_str_view> source_lvls = source.levels();
//     // r_vec<r_int> target_codes = target.value;
//     // r_vec<r_int> source_codes = source.value;

//     // setdiff(new_levels, old_levels)
//     r_vec<r_str_view> new_lvls = source_lvls.subset(source_lvls.find(target_lvls, /*invert = */ true));
//     r_size_t n_new_lvls = new_lvls.length();
    
//     // Append new levels
//     for (r_size_t i = 0; i < n_new_lvls; ++i){
//         target.append_level(new_lvls.get(i));
//     }

//     // Replace categories
//     for (r_size_t i = 0; i < n; ++i) {
//         target.set(target_offset + i, source.view(i));
//     }
// }

template <typename T>
requires (RVector<T> || RFactor<T>)
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
  
template <typename... Args>
auto combine(Args... args){
    using common_t = common_r_t<as_r_composite_t<Args>...>;
    r_vec<r_sexp> list_of_args = make_vec<r_sexp>(args...);
    return flatten<common_t>(list_of_args);
}

}

#endif
