#ifndef CPPALLY_R_VISIT_H
#define CPPALLY_R_VISIT_H

#include <cppally/r_vec.h>
#include <cppally/r_factor.h>
#include <cppally/r_sexp_types.h>
#include <cppally/r_df.h>
#include <cppally/r_function.h>
#include <utility>
#include <string>

namespace cppally {

namespace internal {

// In-place mutation helper for the mutate dispatchers
//
// We std::move x into the typed wrapper rather than viewing it: the move carries
// x's ref without bumping the count, so the wrapper is sole owner exactly when x
// was. Necessary when copy-on-modify is enabled.

template <class V, class F>
inline void mutate_as(r_sexp& x, F&& f) {
    V v(std::move(x)); // v will go out of scope at function end
    f(v);
    x = r_sexp(v);
}

// Lambda-based dispatchers over TYPEOF(x), so call sites don't hand-roll the switch.

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
    A(CPPALLY_FUNCTIONSXP,             r_function)          \
    A(CPPALLY_DFSXP,                   r_df)

#define CPPALLY_CASE_OWNING(C, W)  case C: return f(W(x, no_checks_tag{}));
#define CPPALLY_CASE_VIEWING(C, W) case C: return f(W(x, view_tag{}, no_checks_tag{}));
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

// Comma-join the names of the candidate wrappers F accepts
template <class F, class... Cs>
inline std::string join_accepted() {
    std::string out;
    ([&]{
        if constexpr (std::invocable<F, Cs&>) {
            if (!out.empty()) out += ", ";
            out += type_str<Cs>();
        }
    }(), ...);
    return out;
}

// The wrapped types F accepts, as a printable list. Used when a constrained
// visit/view/mutate meets a runtime type its visitor rejects.
#define CPPALLY_CASE_TYPE(C, W) W,
template <class F>
inline std::string accepted_types() {
    static std::string s = join_accepted<F, CPPALLY_ALL_CASES(CPPALLY_CASE_TYPE) r_sexp>();
    return s.empty() ? "(none)" : s;
}
#undef CPPALLY_CASE_TYPE

// Terminal for a constrained dispatcher that meets a type its visitor rejects.
// [[noreturn]] is load-bearing: in the guarded switches below the reject arms
// carry no return statement, so they drop out of return-type deduction and the
// dispatcher deduces its type from the accepted arms alone
template <class F>
[[noreturn]] void reject(const char* got) {
    abort("`r_sexp` visitor cannot accept the value's type %s;\n"
          "Accepted types that satisfy the constraints: %s",
          got, accepted_types<F&>().c_str());
}

// Guarded arms: hand `f` the wrapper only if it accepts it, else reject.
#define CPPALLY_CASE_OWNING_G(C, W)                                          \
    case C: if constexpr (std::invocable<F&, W>) return f(W(x, no_checks_tag{}));             \
            else internal::reject<F>(internal::type_str<W>());
#define CPPALLY_CASE_VIEWING_G(C, W)                                         \
    case C: if constexpr (std::invocable<F&, W>) return f(W(x, view_tag{}, no_checks_tag{})); \
            else internal::reject<F>(internal::type_str<W>());
#define CPPALLY_CASE_MUTATE_G(C, W)                                          \
    case C: if constexpr (std::invocable<F&, W&>) { mutate_as<W>(x, f); break; } \
            else internal::reject<F>(internal::type_str<W>());

// Constrained owning visit: dispatch to `f` only for the types it accepts.
template <class F>
decltype(auto) visit_constrained(const r_sexp& x, F&& f) {
    switch (CPPALLY_TYPEOF(x)) {
        CPPALLY_ALL_CASES(CPPALLY_CASE_OWNING_G)
        default: if constexpr (std::invocable<F&, r_sexp>) return f(r_sexp(x));
                 else internal::reject<F>(internal::type_str<r_sexp>());
    }
}

// Constrained viewing visit
template <class F>
decltype(auto) view_constrained(const r_sexp& x, F&& f) {
    switch (CPPALLY_TYPEOF(x)) {
        CPPALLY_ALL_CASES(CPPALLY_CASE_VIEWING_G)
        default: if constexpr (std::invocable<F&, r_sexp>) return f(r_sexp(x, view_tag{}));
                 else internal::reject<F>(internal::type_str<r_sexp>());
    }
}

// Constrained in-place mutation (move-in / write-back per arm via mutate_as).
template <class F>
void mutate_constrained(r_sexp& x, F&& f) {
    switch (CPPALLY_TYPEOF(static_cast<SEXP>(x))) {
        CPPALLY_ALL_CASES(CPPALLY_CASE_MUTATE_G)
        default: if constexpr (std::invocable<F&, r_sexp&>) { mutate_as<r_sexp>(x, f); break; }
                 else internal::reject<F>(internal::type_str<r_sexp>());
    }
}

#undef CPPALLY_CASE_OWNING_G
#undef CPPALLY_CASE_VIEWING_G
#undef CPPALLY_CASE_MUTATE_G

#undef CPPALLY_CASE_OWNING
#undef CPPALLY_CASE_VIEWING
#undef CPPALLY_CASE_MUTATE
#undef CPPALLY_ALL_CASES
#undef CPPALLY_VECTOR_CASES

}

// Constrained visit/view: dispatch to `f` only for the wrapped types it accepts,
// e.g. r_sexp_visit(x, [&]<RVector V>(const V& v){ ... }) — the concept rides on the
// lambda's template parameter. r_sexp_visit produces owning SEXP wrappers and r_sexp_view produces view-only SEXP wrappers.
// Aborts at runtime if x's type isn't one the visitor accepts.
template <class F>
decltype(auto) r_sexp_visit(const r_sexp& x, F&& f) {
    return internal::visit_constrained(x, std::forward<F>(f));
}
template <class F>
decltype(auto) r_sexp_view(const r_sexp& x, F&& f) {
    return internal::view_constrained(x, std::forward<F>(f));
}

// Constrained in-place mutation — the mutating sibling of r_sexp_visit/r_sexp_view. `f`
// receives a sole-owning, mutable wrapper (move-in / write-back), e.g.
// r_sexp_mutate(x, []<RVector V>(V& v){ ... }). Aborts at runtime if x's type isn't
// one the visitor accepts. Takes x by r_sexp& — write-back needs ownership.
template <class F>
void r_sexp_mutate(r_sexp& x, F&& f) {
    internal::mutate_constrained(x, std::forward<F>(f));
}

// Runtime predicate to check if r_sexp is visitable as a non-r_sexp
inline bool is_visitable(const r_sexp& x){
    return internal::view_sexp(x, []<typename T>(const T&) -> bool { return !is<T, r_sexp>; });
}

template <typename T>
requires (!is<T, r_sexp>)
bool is_visitable(const r_sexp& x){
    return internal::view_sexp(x, []<typename v>(const v&) -> bool { return is<v, T>; });
}

// // Visit the static-type that r_sexp holds, abort if it doesn't match.
// // Distinct from constructing T from r_sexp directly because this verifies class + storage whereas 
// // construction from r_sexp verifies only storage.
template <RComposite T>
T visit_as(const r_sexp& x){
    return r_sexp_visit(x, []<typename v>(const v& obj) requires (is<v, T>) {
        return obj;
    });
}
// Same as visit_as<> but returns a short-lifetime view-only object
template <RComposite T>
T view_as(const r_sexp& x){
    return r_sexp_view(x, []<typename v>(const v& obj) requires (is<v, T>) {
        return obj;
    });
}

// Builds a visitor (for r_sexp_visit) that applies the named
// overload set `fn` with the given bound args, converting each arm's result to `ret`.
// The requires-clause excludes unvisitable r_sexp types
#define CPPALLY_MAKE_VISITOR(ret, v, ...)                                                   \
    [&]<typename cppally_visited_t> requires (!is<cppally_visited_t, r_sexp>)               \
        (const cppally_visited_t& v) -> decltype(static_cast<ret>(__VA_ARGS__))             \
    { return static_cast<ret>(__VA_ARGS__); }

// Helper that disambiguates r_sexp type via view_sexp and then calls the named function
// If there is no defined specialisation or overload then this is caught in the last branch
// If the visited type can't be disambiguated, this is caught in the first branch
#define CPPALLY_VIEW_AND_APPLY(x, ret, fn, ...)                                 \
    internal::view_sexp(x, [&]<typename x_type_t>(const x_type_t& x_) -> ret {                                   \
        if constexpr (is<x_type_t, r_sexp>) {                               \
            abort("Unsupported SEXP type in `" #fn "()`");                      \
        } else if constexpr (requires { fn(x_ __VA_OPT__(,) __VA_ARGS__); }) {  \
            return fn(x_ __VA_OPT__(,) __VA_ARGS__);                            \
        } else {                                                                \
            abort("No available method for type %s in `" #fn "()`",             \
                internal::type_str<std::remove_cvref_t<x_type_t>>());       \
        }                                                                       \
    })

#define CPPALLY_VISIT_AND_APPLY(x, ret, fn, ...)                                 \
    internal::visit_sexp(x, [&]<typename x_type_t>(const x_type_t& x_) -> ret {                                   \
        if constexpr (is<x_type_t, r_sexp>) {                                \
            abort("Unsupported SEXP type in `" #fn "()`");                       \
        } else if constexpr (requires { fn(x_ __VA_OPT__(,) __VA_ARGS__); }) {   \
            return fn(x_ __VA_OPT__(,) __VA_ARGS__);                             \
        } else {                                                                 \
            abort("No available method for type %s in `" #fn "()`",              \
                internal::type_str<std::remove_cvref_t<x_type_t>>());        \
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
                return internal::view_sexp(x, [&](const auto& x_) -> ret {                                        \
                    using x_t = std::remove_cvref_t<decltype(x_)>;                                      \
                    if constexpr (is<x_t, r_sexp>) {                                                    \
                        abort("Unsupported SEXP type in `" #fn "()`");                                  \
                    } else {                                                                            \
                        return internal::view_sexp(y, [&](const auto& y_) -> ret {                                \
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
                return internal::view_sexp(x, [&](const auto& x_) -> ret {                                        \
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
                return internal::view_sexp(y, [&](const auto& y_) -> ret {                                        \
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

// Returns a length-0 prototype r_sexp whose type is the common type
inline r_sexp common_ptype(const r_vec<r_sexp>& vecs) {
    r_size_t k = vecs.length();
    if (k == 0){
        return r_null;
    }

    // Prototype of the first r_sexp
    r_sexp out = r_sexp_view(vecs.view(0), []<RComposite A>(const A&) -> r_sexp {
        return static_cast<r_sexp>(A());
    });

    // Roll the common type pairwise
    for (r_size_t j = 1; j < k; ++j) {
        out = r_sexp_view(out, [&]<RComposite A>(const A&) -> r_sexp {
            return r_sexp_view(vecs.view(j), [&]<RComposite B>(const B&) -> r_sexp {
                return static_cast<r_sexp>(common_r_t<A, B>());
            });
        });
    }
    return out;
}

}

#endif
