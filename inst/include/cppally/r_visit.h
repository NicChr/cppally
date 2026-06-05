#ifndef CPPALLY_R_VISIT_H
#define CPPALLY_R_VISIT_H

#include <cppally/r_vec.h>
#include <cppally/r_factor.h>
#include <cppally/r_sexp_types.h>
#include <cppally/r_df.h>
#include <utility>

namespace cppally {

namespace internal {

// In-place mutation helper for `mutate_sexp`.
//
// We *move* `x` into the typed wrapper rather than viewing it: the move carries
// x's ref without bumping the count, so the wrapper is sole owner exactly when x
// was. Necessary when copy-on-modify is enabled.

template <class V, class F>
inline void mutate_as(r_sexp& x, F&& f) {
    V v(std::move(x)); // v will go out of scope at function end
    f(v);
    x = r_sexp(v);
}

// A cleaner lambda-based alternative to the canonical switch(TYPEOF(x)).

// (code, wrapper) entries shared by every dispatcher
#define CPPALLY_VECTOR_CASES(A)                             \
    A(LGLSXP,                          r_vec<r_lgl>)        \
    A(INTSXP,                          r_vec<r_int>)        \
    A(CPPALLY_INT64SXP,                r_vec<r_int64>)      \
    A(REALSXP,                         r_vec<r_dbl>)        \
    A(STRSXP,                          r_vec<r_str>)        \
    A(VECSXP,                          r_vec<r_sexp>)       \
    A(CPLXSXP,                         r_vec<r_cplx>)       \
    A(RAWSXP,                          r_vec<r_raw>)        \
    A(NILSXP,                          r_vec<r_sexp>)       \
    A(CPPALLY_REALDATESXP,             r_vec<r_date>)       \
    A(CPPALLY_REALPSXTSXP,             r_vec<r_psxct>)

#define CPPALLY_ALL_CASES(A)                                \
    CPPALLY_VECTOR_CASES(A)                                 \
    A(CPPALLY_FCTSXP,                  r_factors)           \
    A(SYMSXP,                          r_sym)               \
    A(CPPALLY_DFSXP,                   r_df)

#define CPPALLY_CASE_OWNING(C, W)  case C: return f(W(x));
#define CPPALLY_CASE_VIEWING(C, W) case C: return f(W(x, view_tag{}));
#define CPPALLY_CASE_MUTATE(C, W)  case C: mutate_as<W>(x, f); break;  

template <class F>
decltype(auto) visit_sexp(const r_sexp& x, F&& f) {
    switch (CPPALLY_TYPEOF(x)) {
        CPPALLY_ALL_CASES(CPPALLY_CASE_OWNING)
        default: return f(r_sexp(x));
    }
}

template <class F>
decltype(auto) view_sexp(const r_sexp& x, F&& f) {
    switch (CPPALLY_TYPEOF(x)) {
        CPPALLY_ALL_CASES(CPPALLY_CASE_VIEWING)
        default: return f(r_sexp(x, view_tag{}));
    }
}

// visit sexp and mutate underlying object in-place - for methods like free-function `fill()`
template <class F>
void mutate_sexp(r_sexp& x, F&& f) {
    switch (CPPALLY_TYPEOF(static_cast<SEXP>(x))) {
        CPPALLY_ALL_CASES(CPPALLY_CASE_MUTATE)
        default: mutate_as<r_sexp>(x, f); break;
    }
}

// Sentinel: F is invocable with none of the candidate types.
struct unmatched {};

// Return type of the first candidate F accepts (`unmatched` if none). Lazy:
// invoke_result is only taken on the invocable branch.
template <class F, class C, class... Rest>
consteval auto first_result() {
    if constexpr (std::invocable<F, C>){
        return std::type_identity<std::invoke_result_t<F, C>>{};
    } else if constexpr (sizeof...(Rest) > 0){
        return first_result<F, Rest...>();
    } else {
        return std::type_identity<unmatched>{};
    }
}

template <class... Cs> struct type_list {};

template <class F, class L> struct visit_traits;
template <class F, class... Cs>
struct visit_traits<F, type_list<Cs...>> {
    using return_t = typename decltype(first_result<F, Cs...>())::type;
    // static constexpr bool accepts_any = !std::is_same_v<return_t, unmatched>;
};

// The wrapped types visit_sexp can produce, plus the r_sexp fallback —
// generated from the same case list as the dispatchers above.
#define CPPALLY_CASE_TYPE(C, W) W,
using r_visitable = type_list<CPPALLY_ALL_CASES(CPPALLY_CASE_TYPE) r_sexp>;
#undef CPPALLY_CASE_TYPE

template <class F>
using visit_info = visit_traits<F, r_visitable>;

// accepts_any for mutation: mutate visitors take `V&` (lvalue), so the check
// must use lvalue refs — the prvalue-based visit_info would reject them.
// template <class F, class L> inline constexpr bool mutate_accepts_v = false;
// template <class F, class... Cs>
// inline constexpr bool mutate_accepts_v<F, type_list<Cs...>> = (std::invocable<F&, Cs&> || ...);

#undef CPPALLY_CASE_OWNING
#undef CPPALLY_CASE_VIEWING
#undef CPPALLY_CASE_MUTATE
#undef CPPALLY_ALL_CASES
#undef CPPALLY_VECTOR_CASES


// Shared constraint-filtering for r_visit / r_view: forward to `f` only the
// wrapped types it accepts; abort at runtime on any it does not.
template <class F, class Raw>
decltype(auto) dispatch_constrained(F&& f, Raw&& raw) {
    using Info = visit_info<F&>;
    // static_assert(Info::accepts_any,
    //     "visitor accepts no wrapped R type — check the concept on the lambda's template parameter");
    using Ret = typename Info::return_t;
    return raw([&]<typename U>(U&& elem) -> Ret {
        if constexpr (std::invocable<F&, U>) {
            return std::forward<F>(f)(std::forward<U>(elem));
        } else {
            abort("constraints not satisfied for supplied type %s",
                  type_str<std::remove_cvref_t<U>>());
        }
    });
}

}

// Constrained visit/view: dispatch to `f` only for the wrapped types it accepts,
// e.g. r_visit(x, []<RVector V>(const V& v){ ... }) — the concept rides on the
// lambda's template parameter. r_view hands `f` views (no copy); r_visit hands
// owning wrappers. Aborts at runtime if x's type isn't one the visitor accepts.
template <class F>
decltype(auto) r_visit(const r_sexp& x, F&& f) {
    return internal::dispatch_constrained(std::forward<F>(f),
        [&](auto&& vis) -> decltype(auto) { return internal::visit_sexp(x, std::forward<decltype(vis)>(vis)); });
}

template <class F>
decltype(auto) r_view(const r_sexp& x, F&& f) {
    return internal::dispatch_constrained(std::forward<F>(f),
        [&](auto&& vis) -> decltype(auto) { return internal::view_sexp(x, std::forward<decltype(vis)>(vis)); });
}

// Constrained in-place mutation — the mutating sibling of r_visit/r_view. `f`
// receives a sole-owning, mutable wrapper (move-in / write-back), e.g.
// r_mutate(x, []<RVector V>(V& v){ ... }). Aborts at runtime if x's type isn't
// one the visitor accepts. Takes x by r_sexp& — write-back needs ownership.
template <class F>
void r_mutate(r_sexp& x, F&& f) {
    // static_assert(internal::mutate_accepts_v<F, internal::r_visitable>,
    //     "r_mutate: visitor accepts no wrapped R type — check the concept on the lambda's template parameter");
    internal::mutate_sexp(x, [&]<typename U>(U& elem) {
        if constexpr (std::invocable<F&, U&>) {
            std::forward<F>(f)(elem);
        } else {
            abort("constraints not satisfied for supplied type %s",
                  internal::type_str<std::remove_cvref_t<U>>());
        }
    });
}

// Runtime predicate to check if r_sexp is visitable as a non-r_sexp
inline bool is_visitable(const r_sexp& x){
    return r_visit(x, []<typename T>(const T&) -> bool { return !is<T, r_sexp>; });
}

// Helper that disambiguates r_sexp type via view_sexp and then calls the named function
// If there is no defined specialisation or overload then this is caught in the last branch
// If the visited type can't be disambiguated, this is caught in the first branch
#define CPPALLY_VIEW_AND_APPLY(x, ret, fn, ...)                                                                                         \
    r_view(x, [&]<typename x_type_t> requires (!is<x_type_t, r_sexp>) (const x_type_t& x_) -> ret {                                     \
        if constexpr (requires { fn(x_ __VA_OPT__(,) __VA_ARGS__); }) {                                                                 \
            return fn(x_ __VA_OPT__(,) __VA_ARGS__);                                                                                    \
        } else {                                                                                                                        \
            abort("No available method for type %s in `" #fn "()`",                                                                     \
                internal::type_str<std::remove_cvref_t<x_type_t>>());                                                                   \
        }                                                                                                                               \
    })

#define CPPALLY_VISIT_AND_APPLY(x, ret, fn, ...)                                                                                        \
    r_visit(x, [&]<typename x_type_t> requires (!is<x_type_t, r_sexp>) (const x_type_t& x_) -> ret {                                    \
        if constexpr (requires { fn(x_ __VA_OPT__(,) __VA_ARGS__); }) {                                                                 \
            return fn(x_ __VA_OPT__(,) __VA_ARGS__);                                                                                    \
        } else {                                                                                                                        \
            abort("No available method for type %s in `" #fn "()`",                                                                     \
                internal::type_str<std::remove_cvref_t<x_type_t>>());                                                                   \
        }                                                                                                                               \
    })

    // Double dispatch — handles (r_sexp, r_sexp), (r_sexp, V), and (V, r_sexp).
    #define CPPALLY_VIEW_PAIR_AND_APPLY(x, y, ret, fn, ...)                                             \
        [&]() -> ret {                                                                                  \
            using x_in_t = std::remove_cvref_t<decltype(x)>;                                            \
            using y_in_t = std::remove_cvref_t<decltype(y)>;                                            \
            static_assert(is<x_in_t, r_sexp> || is<y_in_t, r_sexp>,                                     \
                          "CPPALLY_VIEW_PAIR_AND_APPLY: at least one of x, y must be r_sexp");          \
            if constexpr (is<x_in_t, r_sexp> && is<y_in_t, r_sexp>) {                                   \
                return r_view(x, [&](const auto& x_) -> ret {                                           \
                    using x_t = std::remove_cvref_t<decltype(x_)>;                                      \
                    if constexpr (is<x_t, r_sexp>) {                                                    \
                        abort("Unsupported SEXP type in `" #fn "()`");                                  \
                    } else {                                                                            \
                        return r_view(y, [&](const auto& y_) -> ret {                                   \
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
                return r_view(x, [&](const auto& x_) -> ret {                                           \
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
                return r_view(y, [&](const auto& y_) -> ret {                                           \
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
