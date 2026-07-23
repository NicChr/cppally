#ifndef CPPALLY_R_SEXP_METHODS_H
#define CPPALLY_R_SEXP_METHODS_H

// Type-safe methods for r_sexp

#include <cppally/r_visit.h>
#include <cppally/r_coerce.h>
#include <cppally/r_identical.h>
#include <cppally/sugar/r_rep.h>
#include <cppally/sugar/r_subset.h>
#include <cppally/sugar/r_sort.h>
#include <cppally/sugar/r_unique.h>
#include <cppally/sugar/r_n_unique.h>
#include <cppally/sugar/r_groups.h>
#include <cppally/sugar/r_match.h>
#include <cppally/sugar/r_replace_at.h>

namespace cppally {

inline r_sexp rep_len(const r_sexp& x, r_size_t n) {
    // return r_sexp_view(x, CPPALLY_MAKE_VISITOR(r_sexp, rep_len, n)); // Earlier pattern, dont use
    return r_sexp_view(x, CPPALLY_MAKE_VISITOR(r_sexp, v, rep_len(v, n)));
}

inline r_sexp resize(const r_sexp& x, r_size_t n) {
    return r_sexp_view(x, CPPALLY_MAKE_VISITOR(r_sexp, v, resize(v, n)));
}

inline r_sexp rep(const r_sexp& x, const r_vec<r_int>& times) {
    return r_sexp_view(x, CPPALLY_MAKE_VISITOR(r_sexp, v, rep(v, times)));
}

template <typename A>
requires (is<A, r_sexp> || RComposite<A>)
inline r_sexp rep(const A& x, const r_sexp& times) {
    return r_sexp_view(times, CPPALLY_MAKE_VISITOR(r_sexp, v, rep(x, v)));
}

inline r_sexp rep_each(const r_sexp& x, const r_vec<r_int>& each) {
    return r_sexp(CPPALLY_VIEW_AND_APPLY(x, /*return_type = */ SEXP, /*fn = */ rep_each, each));
}

template <internal::RSubscript U>
inline r_sexp subset(const r_sexp& x, const r_vec<U>& indices, bool invert, bool check) {
    return r_sexp(CPPALLY_VIEW_AND_APPLY(x, /*return_type = */ SEXP, /*fn = */ subset, /*rest of args = */ indices, invert, check));
}

inline r_vec<r_int> order(const r_sexp& x, bool preserve_ties) {
    return CPPALLY_VIEW_AND_APPLY(x, /*return_type = */ r_vec<r_int>, /*fn = */ order, /*rest of args = */ preserve_ties);
}

inline r_vec<r_lgl> duplicated(const r_sexp& x, bool all) {
    return r_sexp_view(x, CPPALLY_MAKE_VISITOR(r_vec<r_lgl>, v, duplicated(v, all)));
}

inline r_size_t n_unique(const r_sexp& x) {
    return r_sexp_view(x, CPPALLY_MAKE_VISITOR(r_size_t, v, n_unique(v)));
}

inline groups make_groups(const r_sexp& x, bool ordered) {
    return CPPALLY_VIEW_AND_APPLY(x, /*return_type = */ groups, /*fn = */ make_groups, /*rest of args = */ ordered);
}

template <typename T, typename U>
requires (is<T, r_sexp> || is<U, r_sexp>)
inline r_vec<r_int> match(const T& x, const U& y, r_int no_match) {
    return CPPALLY_VIEW_PAIR_AND_APPLY(x, y, r_vec<r_int>, match, no_match);
}

template <internal::RSubscript U, typename V>
void fill(r_sexp& x, const r_vec<U>& where, const V& with) {
    r_sexp_mutate(x, [&]<typename x_t> requires (!is<x_t, r_sexp>) (x_t& x_){
        if constexpr (requires { fill(x_, where, with); }){
            fill(x_, where, with);
        } else {
            abort("No available method for type %s in `fill()`", internal::type_str<x_t>());
        }
    });
}

template <internal::RSubscript U>
void fill(r_sexp& x, const r_vec<U>& where, const r_sexp& with) {
    internal::visit_sexp(with, [&]<typename with_t>(const with_t& with_) {
        if constexpr (is<with_t, r_sexp>) {
            abort("fill: unsupported `with` type %s", internal::type_str<with_t>());
        } else {
            fill(x, where, with_);
        }
    });
}

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
