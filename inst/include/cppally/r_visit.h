#ifndef CPPALLY_R_VISIT_H
#define CPPALLY_R_VISIT_H

#include <cppally/r_vec.h>
#include <cppally/r_factor.h>
#include <cppally/r_sexp_types.h>
#include <cppally/r_df.h>

namespace cppally {

// A cleaner lambda-based alternative to
// using the canonical switch(TYPEOF(x))

template <class F>
decltype(auto) visit_sexp(SEXP x, F&& f) {
switch (internal::CPPALLY_TYPEOF(x)) {
    case LGLSXP:                            return f(r_vec<r_lgl>(x));
    case INTSXP:                            return f(r_vec<r_int>(x));
    case internal::CPPALLY_INT64SXP:        return f(r_vec<r_int64>(x));
    case REALSXP:                           return f(r_vec<r_dbl>(x));
    case STRSXP:                            return f(r_vec<r_str>(x));
    case VECSXP:                            return f(r_vec<r_sexp>(x));
    case CPLXSXP:                           return f(r_vec<r_cplx>(x));
    case RAWSXP:                            return f(r_vec<r_raw>(x));
    case NILSXP:                            return f(r_vec<r_sexp>(r_null));
    case internal::CPPALLY_REALDATESXP:     return f(r_vec<r_date>(x));
    case internal::CPPALLY_REALPSXTSXP:     return f(r_vec<r_psxct>(x));
    case internal::CPPALLY_FCTSXP:          return f(r_factors(x));
    case SYMSXP:                            return f(r_sym(x));
    case internal::CPPALLY_DFSXP:           return f(r_df(x));
    default:                                return f(r_sexp(x));
}
}

template <class F>
decltype(auto) visit_sexp(const r_sexp& x, F&& f) {
switch (internal::CPPALLY_TYPEOF(x)) {
    case LGLSXP:                        return f(r_vec<r_lgl>(x));
    case INTSXP:                        return f(r_vec<r_int>(x));
    case internal::CPPALLY_INT64SXP:      return f(r_vec<r_int64>(x));
    case REALSXP:                       return f(r_vec<r_dbl>(x));
    case STRSXP:                        return f(r_vec<r_str>(x));
    case VECSXP:                        return f(r_vec<r_sexp>(x));
    case CPLXSXP:                       return f(r_vec<r_cplx>(x));
    case RAWSXP:                        return f(r_vec<r_raw>(x));
    case NILSXP:                            return f(r_vec<r_sexp>(r_null));
    case internal::CPPALLY_REALDATESXP:   return f(r_vec<r_date>(x));
    case internal::CPPALLY_REALPSXTSXP:   return f(r_vec<r_psxct>(x));
    case internal::CPPALLY_FCTSXP:        return f(r_factors(x));
    case SYMSXP:                        return f(r_sym(x));
    case internal::CPPALLY_DFSXP:           return f(r_df(x));
    default:                            return f(r_sexp(x));
}
}

template <class F>
decltype(auto) visit_vector(SEXP x, F&& f) {
switch (internal::CPPALLY_TYPEOF(x)) {
    case LGLSXP:                        return f(r_vec<r_lgl>(x));
    case INTSXP:                        return f(r_vec<r_int>(x));
    case internal::CPPALLY_INT64SXP:      return f(r_vec<r_int64>(x));
    case REALSXP:                       return f(r_vec<r_dbl>(x));
    case STRSXP:                        return f(r_vec<r_str>(x));
    case VECSXP:                        return f(r_vec<r_sexp>(x));
    case CPLXSXP:                       return f(r_vec<r_cplx>(x));
    case RAWSXP:                        return f(r_vec<r_raw>(x));
    case NILSXP:                            return f(r_vec<r_sexp>(r_null));
    case internal::CPPALLY_REALDATESXP:   return f(r_vec<r_date>(x));
    case internal::CPPALLY_REALPSXTSXP:   return f(r_vec<r_psxct>(x));
    default:                            abort("`x` must be a vector to be instantiated from an `r_sexp`");
}
}

template <class F>
decltype(auto) visit_vector(const r_sexp& x, F&& f) {
switch (internal::CPPALLY_TYPEOF(x)) {
    case LGLSXP:                        return f(r_vec<r_lgl>(x));
    case INTSXP:                        return f(r_vec<r_int>(x));
    case internal::CPPALLY_INT64SXP:      return f(r_vec<r_int64>(x));
    case REALSXP:                       return f(r_vec<r_dbl>(x));
    case STRSXP:                        return f(r_vec<r_str>(x));
    case VECSXP:                        return f(r_vec<r_sexp>(x));
    case CPLXSXP:                       return f(r_vec<r_cplx>(x));
    case RAWSXP:                        return f(r_vec<r_raw>(x));
    case NILSXP:                            return f(r_vec<r_sexp>(r_null));
    case internal::CPPALLY_REALDATESXP:   return f(r_vec<r_date>(x));
    case internal::CPPALLY_REALPSXTSXP:   return f(r_vec<r_psxct>(x));
    default:                            abort("`x` must be a vector to be instantiated from an `r_sexp`");
}
}

template <class F>
decltype(auto) view_sexp(const r_sexp& x, F&& f) {
switch (internal::CPPALLY_TYPEOF(x)) {
    case LGLSXP:                        return f(r_vec<r_lgl>(x, internal::view_tag{}));
    case INTSXP:                        return f(r_vec<r_int>(x, internal::view_tag{}));
    case internal::CPPALLY_INT64SXP:      return f(r_vec<r_int64>(x, internal::view_tag{}));
    case REALSXP:                       return f(r_vec<r_dbl>(x, internal::view_tag{}));
    case STRSXP:                        return f(r_vec<r_str>(x, internal::view_tag{}));
    case VECSXP:                        return f(r_vec<r_sexp>(x, internal::view_tag{}));
    case CPLXSXP:                       return f(r_vec<r_cplx>(x, internal::view_tag{}));
    case RAWSXP:                        return f(r_vec<r_raw>(x, internal::view_tag{}));
    case NILSXP:                            return f(r_vec<r_sexp>(r_null, internal::view_tag{}));
    case internal::CPPALLY_REALDATESXP:   return f(r_vec<r_date>(x, internal::view_tag{}));
    case internal::CPPALLY_REALPSXTSXP:   return f(r_vec<r_psxct>(x, internal::view_tag{}));
    case internal::CPPALLY_FCTSXP:        return f(r_factors(x, internal::view_tag{}));
    case SYMSXP:                        return f(r_sym(x, internal::view_tag{}));
    case internal::CPPALLY_DFSXP:                return f(r_df(x, internal::view_tag{}));
    default:                            return f(r_sexp(x, internal::view_tag{}));
}
}

template <class F>
decltype(auto) view_sexp(SEXP x, F&& f) {
switch (internal::CPPALLY_TYPEOF(x)) {
    case LGLSXP:                        return f(r_vec<r_lgl>(x, internal::view_tag{}));
    case INTSXP:                        return f(r_vec<r_int>(x, internal::view_tag{}));
    case internal::CPPALLY_INT64SXP:      return f(r_vec<r_int64>(x, internal::view_tag{}));
    case REALSXP:                       return f(r_vec<r_dbl>(x, internal::view_tag{}));
    case STRSXP:                        return f(r_vec<r_str>(x, internal::view_tag{}));
    case VECSXP:                        return f(r_vec<r_sexp>(x, internal::view_tag{}));
    case CPLXSXP:                       return f(r_vec<r_cplx>(x, internal::view_tag{}));
    case RAWSXP:                        return f(r_vec<r_raw>(x, internal::view_tag{}));
    case NILSXP:                            return f(r_vec<r_sexp>(r_null, internal::view_tag{}));
    case internal::CPPALLY_REALDATESXP:   return f(r_vec<r_date>(x, internal::view_tag{}));
    case internal::CPPALLY_REALPSXTSXP:   return f(r_vec<r_psxct>(x, internal::view_tag{}));
    case internal::CPPALLY_FCTSXP:        return f(r_factors(x, internal::view_tag{}));
    case SYMSXP:                        return f(r_sym(x, internal::view_tag{}));
    case internal::CPPALLY_DFSXP:                return f(r_df(x, internal::view_tag{}));
    default:                            return f(r_sexp(x, internal::view_tag{}));
}
}

// In-place mutation helper for `mutate_sexp`.
//
// We *move* `x` into the typed wrapper rather than viewing it: the move carries
// x's ref without bumping the count, so the wrapper is sole owner exactly when x
// was. Necessary when copy-on-modify is enabled.

namespace internal {

template <class V, class F>
inline void mutate_as(r_sexp& x, F&& f) {
    V v(std::move(x)); // v will go out of scope at function end
    f(v);
    x = r_sexp(v);
}

}

// visit sexp and mutate underlying object in-place - for methods like free-function `fill()`
template <class F>
void mutate_sexp(r_sexp& x, F&& f) {
switch (internal::CPPALLY_TYPEOF(static_cast<SEXP>(x))) {
    case LGLSXP:                          internal::mutate_as<r_vec<r_lgl>>(x, f);    break;
    case INTSXP:                          internal::mutate_as<r_vec<r_int>>(x, f);    break;
    case internal::CPPALLY_INT64SXP:      internal::mutate_as<r_vec<r_int64>>(x, f);  break;
    case REALSXP:                         internal::mutate_as<r_vec<r_dbl>>(x, f);    break;
    case STRSXP:                          internal::mutate_as<r_vec<r_str>>(x, f);    break;
    case VECSXP:                          internal::mutate_as<r_vec<r_sexp>>(x, f);   break;
    case CPLXSXP:                         internal::mutate_as<r_vec<r_cplx>>(x, f);   break;
    case RAWSXP:                          internal::mutate_as<r_vec<r_raw>>(x, f);    break;
    case NILSXP:                          internal::mutate_as<r_vec<r_sexp>>(x, f);   break;
    case internal::CPPALLY_REALDATESXP:   internal::mutate_as<r_vec<r_date>>(x, f);   break;
    case internal::CPPALLY_REALPSXTSXP:   internal::mutate_as<r_vec<r_psxct>>(x, f);  break;
    case internal::CPPALLY_FCTSXP:        internal::mutate_as<r_factors>(x, f);       break;
    case SYMSXP:                          internal::mutate_as<r_sym>(x, f);           break;
    case internal::CPPALLY_DFSXP:         internal::mutate_as<r_df>(x, f);            break;
    default:                              internal::mutate_as<r_sexp>(x, f);          break;
}
}

// Helper that disambiguates r_sexp type via view_sexp and then calls the named function
// If there is no defined specialisation or overload then this is caught in the last branch
// If the visited type can't be disambiguated, this is caught in the first branch
#define CPPALLY_VIEW_AND_APPLY(x, ret, fn, ...)                                 \
    view_sexp(x, [&](const auto& x_) -> ret {                                   \
        if constexpr (is<decltype(x_), r_sexp>) {                               \
            abort("Unsupported SEXP type in `" #fn "()`");                      \
        } else if constexpr (requires { fn(x_ __VA_OPT__(,) __VA_ARGS__); }) {  \
            return fn(x_ __VA_OPT__(,) __VA_ARGS__);                            \
        } else {                                                                \
            abort("No available method for type %s in `" #fn "()`",             \
                internal::type_str<std::remove_cvref_t<decltype(x_)>>());       \
        }                                                                       \
    })

#define CPPALLY_VISIT_AND_APPLY(x, ret, fn, ...)                                 \
    visit_sexp(x, [&](const auto& x_) -> ret {                                   \
        if constexpr (is<decltype(x_), r_sexp>) {                                \
            abort("Unsupported SEXP type in `" #fn "()`");                       \
        } else if constexpr (requires { fn(x_ __VA_OPT__(,) __VA_ARGS__); }) {   \
            return fn(x_ __VA_OPT__(,) __VA_ARGS__);                             \
        } else {                                                                 \
            abort("No available method for type %s in `" #fn "()`",              \
                internal::type_str<std::remove_cvref_t<decltype(x_)>>());        \
        }                                                                        \
    })

    // Double dispatch — handles (r_sexp, r_sexp), (r_sexp, V), and (V, r_sexp).
    #define CPPALLY_VIEW_PAIR_AND_APPLY(x, y, ret, fn, ...)                                             \
        [&]() -> ret {                                                                                  \
            using x_in_t = std::remove_cvref_t<decltype(x)>;                                            \
            using y_in_t = std::remove_cvref_t<decltype(y)>;                                            \
            static_assert(is<x_in_t, r_sexp> || is<y_in_t, r_sexp>,                                     \
                          "CPPALLY_VIEW_PAIR_AND_APPLY: at least one of x, y must be r_sexp");          \
            if constexpr (is<x_in_t, r_sexp> && is<y_in_t, r_sexp>) {                                   \
                return view_sexp(x, [&](const auto& x_) -> ret {                                        \
                    using x_t = std::remove_cvref_t<decltype(x_)>;                                      \
                    if constexpr (is<x_t, r_sexp>) {                                                    \
                        abort("Unsupported SEXP type in `" #fn "()`");                                  \
                    } else {                                                                            \
                        return view_sexp(y, [&](const auto& y_) -> ret {                                \
                            using y_t = std::remove_cvref_t<decltype(y_)>;                              \
                            if constexpr (is<y_t, r_sexp>) {                                            \
                                abort("Unsupported SEXP type in `" #fn "()`");                          \
                            } else if constexpr (requires { fn(x_, y_ __VA_OPT__(,) __VA_ARGS__); }) {  \
                                return fn(x_, y_ __VA_OPT__(,) __VA_ARGS__);                            \
                            } else {                                                                    \
                                abort("No available method for types %s and %s in `" #fn "()`",         \
                                    internal::type_str<x_t>(),                                          \
                                    internal::type_str<y_t>());                                         \
                            }                                                                           \
                        });                                                                             \
                    }                                                                                   \
                });                                                                                     \
            } else if constexpr (is<x_in_t, r_sexp>) {                                                  \
                return view_sexp(x, [&](const auto& x_) -> ret {                                        \
                    using x_t = std::remove_cvref_t<decltype(x_)>;                                      \
                    if constexpr (is<x_t, r_sexp>) {                                                    \
                        abort("Unsupported SEXP type in `" #fn "()`");                                  \
                    } else if constexpr (requires { fn(x_, y __VA_OPT__(,) __VA_ARGS__); }) {           \
                        return fn(x_, y __VA_OPT__(,) __VA_ARGS__);                                     \
                    } else {                                                                            \
                        abort("No available method for types %s and %s in `" #fn "()`",                 \
                            internal::type_str<x_t>(),                                                  \
                            internal::type_str<y_in_t>());                                              \
                    }                                                                                   \
                });                                                                                     \
            } else {                                                                                    \
                return view_sexp(y, [&](const auto& y_) -> ret {                                        \
                    using y_t = std::remove_cvref_t<decltype(y_)>;                                      \
                    if constexpr (is<y_t, r_sexp>) {                                                    \
                        abort("Unsupported SEXP type in `" #fn "()`");                                  \
                    } else if constexpr (requires { fn(x, y_ __VA_OPT__(,) __VA_ARGS__); }) {           \
                        return fn(x, y_ __VA_OPT__(,) __VA_ARGS__);                                     \
                    } else {                                                                            \
                        abort("No available method for types %s and %s in `" #fn "()`",                 \
                            internal::type_str<x_in_t>(),                                               \
                            internal::type_str<y_t>());                                                 \
                    }                                                                                   \
                });                                                                                     \
            }                                                                                           \
        }()

}

#endif
