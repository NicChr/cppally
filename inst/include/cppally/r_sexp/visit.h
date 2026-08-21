#ifndef CPPALLY_R_VISIT_H
#define CPPALLY_R_VISIT_H

#include <cppally/vector/r_vector.h>
#include <cppally/factor/r_factors.h>
#include <cppally/r_sexp/r_sexp_types.h>
#include <cppally/data_frame/r_df.h>
#include <cppally/r_function.h>
#include <utility>
#include <cstddef>
#include <cstdint>
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

// (case labels, wrapper) entries shared by every dispatcher. One entry per wrapper,
// so codes that share a wrapper share an arm rather than emitting the visitor twice.
#define CPPALLY_VECTOR_CASES(A)                                       \
    A(case LGLSXP:,                    r_vec<r_lgl>)                  \
    A(case INTSXP:,                    r_vec<r_int>)                  \
    A(case CPPALLY_INT64SXP:,          r_vec<r_int64>)                \
    A(case REALSXP:,                   r_vec<r_dbl>)                  \
    A(case STRSXP:,                    r_vec<r_str>)                  \
    A(case VECSXP: case NILSXP:,       r_vec<r_sexp>)                 \
    A(case CPLXSXP:,                   r_vec<r_cplx>)                 \
    A(case RAWSXP:,                    r_vec<r_raw>)                  \
    A(case CPPALLY_REALDATESXP:,       r_vec<r_date>)                 \
    A(case CPPALLY_REALPSXTSXP:,       r_vec<r_psxct>)

#define CPPALLY_ALL_CASES(A)                                          \
    CPPALLY_VECTOR_CASES(A)                                           \
    A(case CPPALLY_FCTSXP:,            r_factors)                     \
    A(case SYMSXP:,                    r_sym)                         \
    A(case CPPALLY_FUNCTIONSXP:,       r_function)                    \
    A(case CPPALLY_DFSXP:,             r_df)

// LABELS expands to the arm's `case ...:` labels, so these bodies add only the payload
#define CPPALLY_CASE_OWNING(LABELS, W)  LABELS return f(W(x, no_checks_tag{}));
#define CPPALLY_CASE_VIEWING(LABELS, W) LABELS return f(W(x, view_tag{}, no_checks_tag{}));
#define CPPALLY_CASE_MUTATE(LABELS, W)  LABELS mutate_as<W>(x, f); break;

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

// The reject path is deliberately not templated on F. F is a lambda, so it is unique
// per call site; anything instantiated on it is emitted once per visit in the package.
// Instead the set of wrappers F accepts is folded into a bitmask (probing a concept
// emits no code) and handed to one shared, type-erased reject().

// Candidate names, indexed to match the bit order of accepted_mask below.
#define CPPALLY_CASE_NAME(LABELS, W) type_str<W>(),
inline const char* candidate_name(int i) {
    static const char* const names[] = { CPPALLY_ALL_CASES(CPPALLY_CASE_NAME) type_str<r_sexp>() };
    return names[i];
}
#undef CPPALLY_CASE_NAME

template <class F, class... Cs>
inline uint32_t mask_of() noexcept {
    uint32_t m = 0;
    int i = 0;
    ((((requires { std::declval<F>()(std::declval<Cs&>()); }) ? (m |= (1u << i)) : 0u), ++i), ...);
    return m;
}

// One bit per candidate wrapper, set where F accepts it.
#define CPPALLY_CASE_TYPE(LABELS, W) W,
template <class F>
inline uint32_t accepted_mask() noexcept {
    return mask_of<F, CPPALLY_ALL_CASES(CPPALLY_CASE_TYPE) r_sexp>();
}
#undef CPPALLY_CASE_TYPE

// Terminal for a constrained dispatcher that meets a type its visitor rejects.
// [[noreturn]] is load-bearing: in the guarded switches below the reject arms
// carry no return statement, so they drop out of return-type deduction and the
// dispatcher deduces its type from the accepted arms alone
[[noreturn]] inline void reject(const char* got, uint32_t accepted) {
    std::string out;
    for (int i = 0; (accepted >> i) != 0u; ++i) {
        if ((accepted & (1u << i)) == 0u) {
            continue;
        }
        if (!out.empty()) {
            out += ", ";
        }
        out += candidate_name(i);
    }

    abort("r_sexp visitor cannot accept the value's type: %s\n"
          "Accepted types that satisfy the constraints: %s",
          got, out.empty() ? "(none)" : out.c_str());
}

// Guarded arms: hand `f` the wrapper only if it accepts it, else reject.
#define CPPALLY_CASE_OWNING_G(LABELS, W)                                     \
    LABELS if constexpr (requires { f(W(x, no_checks_tag{})); }) return f(W(x, no_checks_tag{}));             \
           else internal::reject(internal::type_str<W>(), internal::accepted_mask<F&>());
#define CPPALLY_CASE_VIEWING_G(LABELS, W)                                    \
    LABELS if constexpr (requires { f(W(x, view_tag{}, no_checks_tag{})); }) return f(W(x, view_tag{}, no_checks_tag{})); \
           else internal::reject(internal::type_str<W>(), internal::accepted_mask<F&>());
#define CPPALLY_CASE_MUTATE_G(LABELS, W)                                     \
    LABELS if constexpr (requires (W& w) { f(w); }) { mutate_as<W>(x, f); break; } \
           else internal::reject(internal::type_str<W>(), internal::accepted_mask<F&>());

// Constrained owning visit: dispatch to `f` only for the types it accepts.
template <class F>
decltype(auto) visit_constrained(const r_sexp& x, F&& f) {
    switch (CPPALLY_TYPEOF(x)) {
        CPPALLY_ALL_CASES(CPPALLY_CASE_OWNING_G)
        default: if constexpr (requires { f(r_sexp(x)); }) return f(r_sexp(x));
                 else internal::reject(internal::type_str<r_sexp>(), internal::accepted_mask<F&>());
    }
}

// Constrained viewing visit
template <class F>
decltype(auto) view_constrained(const r_sexp& x, F&& f) {
    switch (CPPALLY_TYPEOF(x)) {
        CPPALLY_ALL_CASES(CPPALLY_CASE_VIEWING_G)
        default: if constexpr (requires { f(r_sexp(x, view_tag{})); }) return f(r_sexp(x, view_tag{}));
                 else internal::reject(internal::type_str<r_sexp>(), internal::accepted_mask<F&>());
    }
}

// Constrained in-place mutation (move-in / write-back per arm via mutate_as).
template <class F>
void mutate_constrained(r_sexp& x, F&& f) {
    switch (CPPALLY_TYPEOF(static_cast<SEXP>(x))) {
        CPPALLY_ALL_CASES(CPPALLY_CASE_MUTATE_G)
        default: if constexpr (requires (r_sexp& s) { f(s); }) { mutate_as<r_sexp>(x, f); break; }
                 else internal::reject(internal::type_str<r_sexp>(), internal::accepted_mask<F&>());
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
template <CppallyType T>
requires (!RScalar<T> && !is<T, r_sexp>)
T visit_as(const r_sexp& x){
    return r_sexp_visit(x, []<typename v>(const v& obj) requires (is<v, T>) {
        return obj;
    });
}
// Same as visit_as<> but returns a short-lifetime view-only object
template <CppallyType T>
requires (!RScalar<T> && !is<T, r_sexp>)
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
