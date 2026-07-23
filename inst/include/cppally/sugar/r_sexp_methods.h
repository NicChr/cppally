#ifndef CPPALLY_R_SEXP_METHODS_H
#define CPPALLY_R_SEXP_METHODS_H

// Type-safe methods for r_sexp

#include <cppally/r_visit.h>
#include <cppally/r_coerce.h>
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

} 

#endif
