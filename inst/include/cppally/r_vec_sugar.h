#ifndef CPPALLY_R_VEC_SUGAR_H
#define CPPALLY_R_VEC_SUGAR_H

// General free functions

#include <cppally/r_vec.h>
#include <cppally/r_attrs.h>
#include <cppally/r_visit.h>
#include <cppally/r_length.h>

namespace cppally {

// Generic SEXP functions

// Equal to `r_null`?
inline bool is_null(SEXP x) noexcept {
    return x == R_NilValue;
}
inline bool is_altrep(SEXP x) noexcept {
    return r_sexp(x, internal::view_tag{}).is_altrep();
}
// Memory address
inline r_str address(SEXP x) {
    return r_sexp(x, internal::view_tag{}).address();
}

// Vector and vector-based functions

inline bool is_long(SEXP x){
    return length(r_sexp(x, internal::view_tag{})) > static_cast<r_size_t>(std::numeric_limits<int>::max());
}

inline r_vec<r_int> lengths(const r_vec<r_sexp>& x){
    return x.lengths();
}
inline r_vec<r_int> lengths(const r_df& x){
    return x.value.lengths();
}

template <RVal T>
r_size_t count(const r_vec<T>&x, const T& value){
    return x.count(value);
}

inline r_size_t count(const r_factors&x, r_str value){
    r_vec<r_str_view> lvls = x.levels();
    r_vec<r_int> code_loc = x.levels().find(value);
    if (code_loc.length() == 0){
        return 0;
    }
    int code = code_loc.get(0);
    return x.value.count(r_int(code + 1));
}

template <typename T>
r_size_t count(const r_sexp& x, const T& value){
    return view_sexp(x, [&](const auto& x_) -> r_size_t {
        using x_t = std::remove_cvref_t<decltype(x_)>;
        if constexpr (is<x_t, r_sexp>){
            abort("Unsupported SEXP type in `count()`");
        } else if constexpr (requires { count(x_, value); }){
            return count(x_, value);
        } else {
            abort("No available method for type %s in `count()`", internal::type_str<std::remove_cvref_t<decltype(x_)>>());
        }
      });
}


// Fns left to do:
// find
// remove
// replace
// na_count
// any_na
// all_na

}

#endif
