#ifndef CPPALLY_R_SEXP_METHODS_H
#define CPPALLY_R_SEXP_METHODS_H

// Type-safe methods for r_sexp

#include <cppally/r_visit.h>
#include <cppally/r_length.h>
#include <cppally/r_coerce.h>
#include <cppally/sugar/r_rep.h>
#include <cppally/sugar/r_subset.h>
#include <cppally/sugar/r_sort.h>
#include <cppally/sugar/r_unique.h>
#include <cppally/sugar/r_n_unique.h>
#include <cppally/sugar/r_hash.h>
#include <cppally/sugar/r_groups.h>
#include <cppally/sugar/r_match.h>
#include <cppally/sugar/r_find.h>
#include <cppally/sugar/r_remove.h>
#include <cppally/sugar/r_fill.h>
#include <cppally/sugar/r_replace.h>
#include <cppally/r_copy.h>
#include <cppally/r_identical.h>

namespace cppally {

inline r_size_t length(const r_sexp& x) {
    return CPPALLY_VIEW_AND_APPLY(x, /*return_type = */ r_size_t, /*fn = */ length);
}

inline r_sexp rep_len(const r_sexp& x, r_size_t n) {
    return r_sexp(CPPALLY_VIEW_AND_APPLY(x, /*return_type = */ SEXP, /*fn = */ rep_len, n));
}

inline r_sexp resize(const r_sexp& x, r_size_t n) {
    return r_sexp(CPPALLY_VIEW_AND_APPLY(x, /*return_type = */ SEXP, /*fn = */ resize, n));
}

inline r_sexp rep(const r_sexp& x, const r_vec<r_int>& times) {
    return r_sexp(CPPALLY_VIEW_AND_APPLY(x, /*return_type = */ SEXP, /*fn = */ rep, times));
}

inline r_sexp rep_each(const r_sexp& x, const r_vec<r_int>& each) {
    return r_sexp(CPPALLY_VIEW_AND_APPLY(x, /*return_type = */ SEXP, /*fn = */ rep_each, each));
}

template <internal::RSubscript U>
inline r_sexp subset(const r_sexp& x, const r_vec<U>& indices, bool check, bool invert) {
    return r_sexp(CPPALLY_VIEW_AND_APPLY(x, /*return_type = */ SEXP, /*fn = */ subset, /*rest of args = */ indices, check, invert));
}

inline r_vec<r_int> order(const r_sexp& x, bool preserve_ties) {
    return CPPALLY_VIEW_AND_APPLY(x, /*return_type = */ r_vec<r_int>, /*fn = */ order, /*rest of args = */ preserve_ties);
}

inline r_vec<r_lgl> duplicated(const r_sexp& x, bool all) {
    return CPPALLY_VIEW_AND_APPLY(x, /*return_type = */ r_vec<r_lgl>, /*fn = */ duplicated, /*rest of args = */ all);
}

inline r_size_t n_unique(const r_sexp& x) {
    return CPPALLY_VIEW_AND_APPLY(x, /*return_type = */ r_size_t, /*fn = */ n_unique);
}

inline groups make_groups(const r_sexp& x, bool ordered) {
    return CPPALLY_VIEW_AND_APPLY(x, /*return_type = */ groups, /*fn = */ make_groups, /*rest of args = */ ordered);
}

template <typename T, typename U>
requires (is<T, r_sexp> || is<U, r_sexp>)
inline r_vec<r_int> match(const T& x, const U& y, r_int no_match) {
    return CPPALLY_VIEW_PAIR_AND_APPLY(x, y, r_vec<r_int>, match, no_match);
}

template <typename T, typename U>
requires (is<T, r_sexp> || is<U, r_sexp>)
inline T remove(const T& x, const U& values) {
    return as<T>(CPPALLY_VIEW_PAIR_AND_APPLY(x, values, SEXP, remove));
}

template <typename T, typename U, internal::RNumericSubscript V>
requires (is<T, r_sexp> || is<U, r_sexp>)
inline r_vec<V> find(const T& x, const U& values, bool invert) {
    return CPPALLY_VIEW_PAIR_AND_APPLY(x, values, r_vec<V>, find, invert);
}

template <internal::RSubscript U, typename V>
void fill(r_sexp& x, const r_vec<U>& where, const V& with) {
    mutate_sexp(x, [&](auto& x_) {
        using x_t = std::remove_cvref_t<decltype(x_)>;
        if constexpr (is<x_t, r_sexp>){
            abort("Unsupported SEXP type in `fill()`");
        } else if constexpr (requires { fill(x_, where, with); }){
            fill(x_, where, with);
        } else {
            abort("No available method for type %s in `fill()`", internal::type_str<x_t>());
        }
    });
}

template <internal::RSubscript U>
void fill(r_sexp& x, const r_vec<U>& where, const r_sexp& with) {
    visit_sexp(with, [&](const auto& with_) {
        fill(x, where, with_);
    });
}

template <typename U>
inline void replace(r_sexp& x, const U& old_values, const U& new_values) {
    mutate_sexp(x, [&](auto& x_) {
        using x_t = std::remove_cvref_t<decltype(x_)>;
        x_t oldv = x_t(static_cast<SEXP>(old_values));
        x_t newv = x_t(static_cast<SEXP>(new_values));
        if constexpr (is<x_t, r_sexp>){
            abort("Unsupported SEXP type in `replace()`");
        } else if constexpr (requires { replace(x_, oldv, newv); }){
            replace(x_, oldv, newv);
        } else {
            abort("No available method for type %s in `replace`", internal::type_str<x_t>());
        }
    });
}

template<>
inline r_sexp deep_copy(const r_sexp& x) {
    return view_sexp(x, [](const auto& vec) -> r_sexp {
        if constexpr (!is<decltype(vec), r_sexp>){
            return as<r_sexp>(deep_copy(vec));
        } else {
            return r_sexp(safe[Rf_duplicate](vec));
        }
    });
}

template<>
inline r_sexp shallow_copy(const r_sexp& x) {
    return view_sexp(x, [](const auto& vec) -> r_sexp {
        if constexpr (!is<decltype(vec), r_sexp>){
            return as<r_sexp>(shallow_copy(vec));
        } else {
            return r_sexp(safe[Rf_shallow_duplicate](vec));
        }
    });
}

namespace internal {

inline bool identical_impl(const r_sexp& a, const r_sexp& b) {
    SEXP x = unwrap(a);
    SEXP y = unwrap(b);
    if (x == y) return true;
    return view_sexp(a, [&b](const auto& vec1) -> bool {
        using vec1_t = decltype(vec1);
        if constexpr (is<vec1_t, r_sexp>){
            return R_compute_identical(vec1, b, 16);
        } else {
            return view_sexp(b, [&vec1](const auto& vec2) -> bool {
                using vec2_t = decltype(vec2);
                if constexpr (!is<vec1_t, vec2_t>){
                    return false;
                } else {
                    return identical_impl(vec1, vec2);
                }
            });
        }
    });
}

inline uint64_t r_hash_impl(const r_sexp& x) noexcept {
    return CPPALLY_VIEW_AND_APPLY(x, /*return_type = */ uint64_t, /*fn = */ r_hash_impl);
}

}

} 

#endif
