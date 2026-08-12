#ifndef CPPALLY_R_SEXP_METHODS_H
#define CPPALLY_R_SEXP_METHODS_H

// Type-safe methods for r_sexp

#include <cppally/r_visit.h>
#include <cppally/r_coerce.h>
#include <cppally/r_identical.h>
#include <cppally/sugar/r_rep.h>
#include <cppally/sugar/r_subset.h>
#include <cppally/sort/sort.h>
#include <cppally/unique/unique.h>
#include <cppally/group/groups.h>
#include <cppally/match/match.h>
#include <cppally/sugar/r_replace_at.h>

namespace cppally {

namespace internal {

template <RVector T>
inline bool identical_impl(const T& a, const T& b) {
    if (internal::ptrs_identical(a, b)) return true; // same pointer
    if (a.length() != b.length()) return false;
    
    bool x_has_attrs = attr::has_attrs(a);
    bool y_has_attrs = attr::has_attrs(b);
    if (x_has_attrs != y_has_attrs) return false;
    
    if (x_has_attrs && y_has_attrs){
        r_vec<r_sexp> a_attrs = attr::get_attrs(a);
        r_vec<r_sexp> b_attrs = attr::get_attrs(b);

        if (a_attrs.length() != b_attrs.length()) return false;
        if (!identical_impl(a_attrs.names(), b_attrs.names())) return false;

        // Only do the rest of the attr checks if pointers do not match
        if (unwrap(a_attrs) != unwrap(b_attrs)){
            r_vec<r_str_view> names1 = a_attrs.names();
            r_vec<r_str_view> names2 = b_attrs.names();
            if (!identical_impl(names1, names2)) return false;

            for (r_size_t i = 0; i < a_attrs.length(); ++i){
                if (!identical_impl(a_attrs.view(i), b_attrs.view(i))) return false;
            }
        }
    }

    r_size_t n = a.length();
    for (r_size_t i = 0; i < n; ++i){
        if (!identical_impl(a.view(i), b.view(i))){
            return false;
        }
    } 
    return true;
}

template<>
inline bool identical_impl<r_factors>(const r_factors& a, const r_factors& b) {
    return identical_impl(a.value, b.value);
}

template<>
inline bool identical_impl<r_df>(const r_df& a, const r_df& b) {
    return identical_impl(a.value, b.value);
}

inline bool identical_impl(const r_sexp& a, const r_sexp& b) {
    if (internal::ptrs_identical(a, b)) return true;
    if (a.is_null() || b.is_null()) return false; // If true it would have been caught by above ptr comparison
    return internal::view_sexp(a, [&b]<typename vec1_t>(const vec1_t& vec1) -> bool {
        if constexpr (is<vec1_t, r_sexp>){
            return R_compute_identical(vec1, b, 16);
        } else {
            return internal::view_sexp(b, [&vec1]<typename vec2_t>(const vec2_t& vec2) -> bool {
                if constexpr (!is<vec1_t, vec2_t>){
                    return false;
                } else {
                    return identical_impl(vec1, vec2);
                }
            });
        }
    });
}

}

} 

#endif
