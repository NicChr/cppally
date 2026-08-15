#ifndef CPPALLY_R_DISPATCH_H
#define CPPALLY_R_DISPATCH_H

// Runtime dispatch for templated C++ functions registered to R, driven by a modified
// version of R's TYPEOF plus a map of C++20 types to SEXP tags.
//
// Each template parameter is deduced from the runtime type of its arguments, then the
// first instantiation whose constraints are satisfied is called. Where the SEXP/C++
// mapping is one-to-many, both the vector and the scalar form are offered as candidates.
//
// Non-templated registrations bypass all of this - R/register.R emits a direct
// r_to_cpp/cpp_to_r call for those

#include <cppally/r_sexp/r_sexp_types.h>
#include <cppally/coerce.h>
#include <array>
#include <cstdint>
#include <cstring>
#include <exception>
#include <limits>
#include <tuple>
#include <type_traits>
#include <utility>

// attribute_hidden opts a vague-linkage symbol out of STB_GNU_UNIQUE binding on ELF,
// keeping it private to its shared library - see per_functor_dispatch_table
#include <R_ext/Visibility.h>

// Matches the cpp11 default
#ifndef CPPALLY_ERROR_BUFSIZE
#define CPPALLY_ERROR_BUFSIZE 8192
#endif

#define BEGIN_CPPALLY                           \
  SEXP cppally_err_ = R_NilValue;               \
  char cppally_buf_[CPPALLY_ERROR_BUFSIZE];     \
  cppally_buf_[0] = '\0';                       \
  try {

#define END_CPPALLY                                                                         \
  }                                                                                         \
  catch (cppally::internal::unwind_exception& e) { cppally_err_ = e.token; }                 \
  catch (std::exception& e) { cppally::internal::copy_error(cppally_buf_, e.what()); }        \
  catch (...) { cppally::internal::copy_error(cppally_buf_, "C++ error (unknown cause)"); }   \
  if (cppally_buf_[0] != '\0') {                                                            \
    Rf_errorcall(R_NilValue, "%s", cppally_buf_);                                           \
  } else if (cppally_err_ != R_NilValue) {                                                  \
    R_ContinueUnwind(cppally_err_);                                                         \
  }                                                                                         \
  return R_NilValue;


namespace cppally {

namespace internal {

inline void copy_error(char (&buf)[CPPALLY_ERROR_BUFSIZE], const char* msg) noexcept {
    strncpy(buf, msg, CPPALLY_ERROR_BUFSIZE - 1);
    buf[CPPALLY_ERROR_BUFSIZE - 1] = '\0';
}


// ── BOUNDARY CONVERSIONS ──────────────────────────────────────────────────────

// RScalar -> RVector, everything else -> SEXP
template <typename T>
SEXP cpp_to_r(const T& x) {
    if constexpr (RScalar<T>){
      return static_cast<SEXP>(r_vec<T>(1, x));
    } else {
      return as<SEXP>(x);
    }
}

// NULL is left alone where RComposite is concerned to allow passing optional arguments
template <typename T>
auto r_to_cpp(SEXP x) {
    using out_t = std::remove_cvref_t<T>;
    if constexpr (RComposite<out_t>){
        if (x == R_NilValue){
            return out_t(r_null);
        }
    }
    return as<out_t>(x);
}


// ── TUPLE HELPERS ─────────────────────────────────────────────────────────────

template <typename... Tuples>
using tuple_cat_t = decltype(std::tuple_cat(std::declval<Tuples>()...));

template <bool Enabled, typename Tuple>
using keep_if = std::conditional_t<Enabled, Tuple, std::tuple<>>;

template <typename Tuple> struct as_r_vecs;

template <typename... Ts>
struct as_r_vecs<std::tuple<Ts...>> {
    using type = std::tuple<r_vec<Ts>...>;
};
template <typename Tuple>
using as_r_vecs_t = typename as_r_vecs<Tuple>::type;


// ── DISPATCH CANDIDATE SET ────────────────────────────────────────────────────
//
// Package-wide compile-time policy, normally set by `use_template_dispatch_candidates()`.
// Narrowing it cuts the instantiations the dispatcher emits, at the cost of
// narrowing which R types a registered templated function accepts at runtime.
//
// Every TU in a package MUST agree on these macros. The tables are vague-linkage
// symbols, so two TUs disagreeing is an ODR violation the linker will not diagnose.
// Setting them via PKG_CPPFLAGS guarantees agreement. Across shared libraries
// disagreement is fine: see shared_type_table's Config and attribute_hidden below.
//
// Unrelated to the visitor list in r_visit.h, which is a linear switch over a
// different set of types and is deliberately not narrowed by these macros.

#define CPPALLY_DEFAULT_DISPATCH_CANDIDATES \
    r_lgl, r_int, r_int64, r_dbl, r_str, r_cplx, r_raw, r_date, r_psxct

#ifndef CPPALLY_DISPATCH_CANDIDATES
#define CPPALLY_DISPATCH_CANDIDATES CPPALLY_DEFAULT_DISPATCH_CANDIDATES
#endif

// use_template_dispatch_candidates() defines these (valueless) via PKG_CPPFLAGS.
#ifdef CPPALLY_NO_SCALAR_CANDIDATES
constexpr bool with_scalars = false;
#else
constexpr bool with_scalars = true;
#endif

#ifdef CPPALLY_NO_VECTOR_CANDIDATES
constexpr bool with_vectors = false;
#else
constexpr bool with_vectors = true;
#endif

#ifdef CPPALLY_NO_FACTOR_CANDIDATE
constexpr bool with_factors = false;
#else
constexpr bool with_factors = true;
#endif

#ifdef CPPALLY_NO_DF_CANDIDATE
constexpr bool with_df = false;
#else
constexpr bool with_df = true;
#endif

// Drops both r_sexp and r_vec<r_sexp>
#ifdef CPPALLY_NO_SEXP_CANDIDATE
constexpr bool with_sexp = false;
#else
constexpr bool with_sexp = true;
#endif

// Two tiers. with_factors, with_df and with_sexp gate an element, so they are baked
// into the plain names below - with_sexp has to be, since dropping r_sexp must drop
// r_vec<r_sexp> with it. with_scalars and with_vectors gate a whole form, which is
// what the enabled_ tier applies
using scalar_types         = std::tuple<CPPALLY_DISPATCH_CANDIDATES>;
using sexp_types           = keep_if<with_sexp, std::tuple<r_sexp>>;
using vector_types         = as_r_vecs_t<tuple_cat_t<scalar_types, sexp_types>>;
using special_vector_types = tuple_cat_t<keep_if<with_factors, std::tuple<r_factors>>,
                                         keep_if<with_df,      std::tuple<r_df>>>;

using enabled_scalar_types = keep_if<with_scalars, scalar_types>;
using enabled_vector_types = keep_if<with_vectors, vector_types>;

// Candidate order is scan order: special vector types first, then vectors, then scalars.
// The r_sexp catch-all is its own trailing block, so it is always tried last no
// matter which of the other switches are set.
using all_candidate_types = tuple_cat_t<special_vector_types,
                                        enabled_vector_types,
                                        enabled_scalar_types,
                                        sexp_types>;

constexpr size_t N_CANDIDATES = std::tuple_size_v<all_candidate_types>;

static_assert(
    N_CANDIDATES > 0,
    "No dispatch candidates left:"
    "Re-enable at least one via `use_template_dispatch_candidates()`"
);


// ── TYPE CODES ────────────────────────────────────────────────────────────────

template <typename T> constexpr uint16_t r_cpp_boundary_map_v = r_typeof<T>;

// Lets users write scalar inputs to functions (like `r_int`)
template <RScalar T>
inline constexpr uint16_t r_cpp_boundary_map_v<T> = r_cpp_boundary_map_v<r_vec<T>>;

// Pure C/C++ types that are constructible to an RScalar
template <CppScalar T>
requires (CastableToRScalar<T>)
inline constexpr uint16_t r_cpp_boundary_map_v<T> = r_cpp_boundary_map_v<as_r_scalar_t<T>>;

// r_sexp is the wildcard: it matches any runtime type. uint32_t max is outside
// uint16_t range, so the sentinel never collides with any CPPALLY_TYPEOF value
inline constexpr uint32_t wildcard_code = std::numeric_limits<uint32_t>::max();

template <typename T>
constexpr uint32_t code_of() {
    if constexpr (is_sexp<T>) {
        return wildcard_code;
    } else {
        return static_cast<uint32_t>(r_cpp_boundary_map_v<T>);
    }
}

template <typename Tuple> struct codes_of_impl;
template <typename... Ts>
struct codes_of_impl<std::tuple<Ts...>> {
    static constexpr std::array<uint32_t, sizeof...(Ts)> value{ code_of<Ts>()... };
};

// A candidate set expressed as codes. Keyed on the tuple type, so two shared
// libraries with different candidate sets get distinct specialisations
template <typename Tuple>
inline constexpr auto codes_of = codes_of_impl<Tuple>::value;


// ── EXCLUDED TYPES ────────────────────────────────────────────────────────────

// Neither of these is the candidate list - that is all_candidate_types. They feed
// is_excluded_code only, which matches on codes, not types: known = every code cppally
// can name, selected = the ones this build kept. A closure or environment is in neither,
// so it falls through to the r_sexp wildcard rather than being reported as excluded.
//
// One entry per code, vector form throughout: a scalar borrows its vector's code
// (r_int and r_vec<r_int> are both INTSXP) so either would do for those, but only
// r_vec<r_sexp> carries VECSXP. Listing both forms would collide and trip the assert
using known_types = tuple_cat_t<as_r_vecs_t<std::tuple<CPPALLY_DEFAULT_DISPATCH_CANDIDATES, r_sexp>>,
                                std::tuple<r_factors, r_df>>;
// vector_types, not enabled_vector_types: selection is about which types you named,
// not which forms you left on
using selected_types = tuple_cat_t<vector_types, special_vector_types>;

template <size_t N>
constexpr bool contains_code(const std::array<uint32_t, N>& codes, uint32_t code) {
    for (uint32_t c : codes) {
        if (c == code) {
            return true;
        }
    }
    return false;
}

// Code equality is treated as type identity throughout the dispatcher (type
// table matching, exclusion, shared_type_table's Config), so every candidate
// must map to its own code. A future type missing its r_typeof specialisation
// would inherit the uint16_t max default and silently alias another candidate
static_assert([]{
    constexpr auto codes = codes_of<known_types>;
    for (size_t i = 0; i < codes.size(); ++i) {
        if (codes[i] == std::numeric_limits<uint16_t>::max()) {
            return false;
        }
        for (size_t j = i + 1; j < codes.size(); ++j) {
            if (codes[i] == codes[j]) {
                return false;
            }
        }
    }
    return true;
}(), "Every cppally candidate type must map to a distinct CPPALLY_TYPEOF code");

// Excluded = cppally knows this type, but this build switched it off. Deliberately
// ignores with_scalars/with_vectors: dropping a whole form while keeping r_sexp is a
// "send it all to the catch-all" build, so the wildcard should still claim the value.
// Naming a type out of CPPALLY_DISPATCH_CANDIDATES is the opposite - there the silent
// reroute to r_sexp is exactly what we want to report.
// static: config-dependent and not a template, so internal linkage is what keeps two
// shared libraries with different candidate sets off each other's definition
static constexpr bool is_excluded_code(uint32_t code) {
    return contains_code(codes_of<known_types>, code) &&
          !contains_code(codes_of<selected_types>, code);
}

// Lets the dispatcher drop the exclusion check entirely when nothing is excluded
static constexpr bool has_exclusions = []{
    for (uint32_t c : codes_of<known_types>) {
        if (!contains_code(codes_of<selected_types>, c)) {
            return true;
        }
    }
    return false;
}();


// ── CANDIDATE COMBINATIONS ────────────────────────────────────────────────────
//
// The dispatcher pre-builds two flat arrays at compile time, indexed by a flat
// combo index I (0 to N_CANDIDATES^NumTemplateParams - 1):
//
//   dispatch_table[I] - nullptr if the combination is invalid for the lambda,
//                       otherwise a pointer to combo_invoker<...>::invoke
//   type_table[I][K]  - the CPPALLY_TYPEOF expected for template param K in combo I
//
// At runtime a linear scan takes the first I whose entry is non-null and whose codes
// all match the actual argument types. The final call is through a function pointer,
// so GCC cannot inline across it.
//
// type_table is keyed on NumTemplateParams alone rather than on Functor - that is what
// combo_invoker's void* erasure buys - so it is built once and shared across every
// registered function with the same number of template parameters. Only dispatch_table
// stays per-Functor, since its validity depends on is_combo_callable<Functor, ...>

static constexpr size_t static_pow(size_t base, size_t exp) {
    size_t r = 1;
    for (size_t i = 0; i < exp; ++i) {
        r *= base;
    }
    return r;
}

// Maps a flat index I to an N-tuple of candidates by treating I as a base-N_CANDIDATES
// number, each "digit" selecting one type. Digits are prepended as the recursion
// unwinds, so digit 0 lands in the LAST tuple position - the last template param
// cycles through the candidates fastest as I increments
template <size_t Val, size_t N, size_t... Is>
struct extract_combo {
    using type = typename extract_combo<Val / N_CANDIDATES, N - 1, Val % N_CANDIDATES, Is...>::type;
};
template <size_t Val, size_t... Is>  // N == 0 base case
struct extract_combo<Val, 0, Is...> {
    using type = std::tuple<std::tuple_element_t<Is, all_candidate_types>...>;
};
template <size_t I, size_t N>
using combo_t = typename extract_combo<I, N>::type;

// SFINAE check: is this lambda callable with these N types and NumArgs SEXP arguments?
// std::void_t<decltype(...)> puts the whole expression in a deduction context, so a
// failed concept on the lambda is a soft substitution failure rather than a hard error.
// ((void)Is, declval<SEXP>())... discards each Is but uses it to drive pack expansion,
// producing exactly sizeof...(Is) copies of an expression that is not itself a pack
template <typename Functor, typename ComboTuple, typename IndexSeq, typename = void>
struct is_combo_callable : std::false_type {};

template <typename Functor, typename... Ts, size_t... Is>
struct is_combo_callable<
    Functor,
    std::tuple<Ts...>,
    std::index_sequence<Is...>,
    std::void_t<decltype(
        std::declval<Functor>().template operator()<Ts...>(((void)Is, std::declval<SEXP>())...)
    )>
> : std::true_type {};

// void* in place of Functor& is what lets type_table be shared across Functors
using erased_fn_t = SEXP(*)(void*, SEXP*);

// Each valid combination becomes its own tiny function; invoke() casts the Functor back
template <typename Functor, size_t NumArgs, typename ComboTuple>
struct combo_invoker;

template <typename Functor, size_t NumArgs, typename... Ts>
struct combo_invoker<Functor, NumArgs, std::tuple<Ts...>> {
    static SEXP invoke(void* f, SEXP* args) {
        auto& functor = *static_cast<Functor*>(f);
        return [&]<size_t... Is>(std::index_sequence<Is...>) {
            return functor.template operator()<Ts...>(args[Is]...);
        }(std::make_index_sequence<NumArgs>{});
    }
};


// ── DISPATCH TABLES ───────────────────────────────────────────────────────────

// A template struct rather than a constexpr function: lambda types are non-literal
// (non-trivially destructible), which makes a constexpr function templated on Functor
// illegal in constant expression contexts in GCC. A static constexpr member of a
// template struct has no such restriction.
// Primary: invalid combo -> nullptr. Partial specialisation: valid -> invoker address
template <size_t I, size_t NumTemplateParams, size_t NumArgs, typename Functor,
          bool Valid = is_combo_callable<
              Functor, combo_t<I, NumTemplateParams>, std::make_index_sequence<NumArgs>
          >::value>
struct dispatch_entry_impl {
    static constexpr erased_fn_t value = nullptr;
};

template <size_t I, size_t NumTemplateParams, size_t NumArgs, typename Functor>
struct dispatch_entry_impl<I, NumTemplateParams, NumArgs, Functor, true> {
    static constexpr erased_fn_t value =
        &combo_invoker<Functor, NumArgs, combo_t<I, NumTemplateParams>>::invoke;
};

template <size_t NumTemplateParams, size_t NumArgs, typename Functor, size_t... Is>
constexpr auto make_dispatch_table(std::index_sequence<Is...>) {
    return std::array<erased_fn_t, sizeof...(Is)>{
        dispatch_entry_impl<Is, NumTemplateParams, NumArgs, Functor>::value...
    };
}

template <size_t I, size_t NumTemplateParams, size_t... Ks>
constexpr std::array<uint32_t, NumTemplateParams> make_type_entry(std::index_sequence<Ks...>) {
    return { code_of<std::tuple_element_t<Ks, combo_t<I, NumTemplateParams>>>()... };
}

// attribute_hidden because the Functor key alone does not make this symbol unique
// process-wide: a rebuilt shared library of the same name reuses its lambdas'
// mangled names, and GNU-unique binding would pin the FIRST build's table (the
// old library is marked NODELETE, so unloading cannot evict it). The reloaded
// library would then dispatch through stale function pointers - running the
// previous build's code, or scanning a mis-sized table if the candidate flags
// changed between builds. Hidden visibility keeps each library on its own table
template <size_t NumTemplateParams, size_t NumArgs, typename Functor>
struct attribute_hidden per_functor_dispatch_table {
    static constexpr auto value = make_dispatch_table<NumTemplateParams, NumArgs, Functor>(
        std::make_index_sequence<static_pow(N_CANDIDATES, NumTemplateParams)>{}
    );
};

// Config pins the candidate set into the symbol name and must be left defaulted.
// On Linux, GCC emits `value` as STB_GNU_UNIQUE and glibc resolves such symbols
// process-wide, ignoring RTLD_LOCAL. Keyed on NumTemplateParams alone, two shared
// libraries built with different dispatch candidates would silently share whichever
// table loaded first: the later library then scans a table that disagrees with its
// own dispatch_table, wrongly rejecting valid types, dispatching to the wrong
// instantiation, or reading past the end of a smaller table. With Config in the name,
// two configs share a symbol iff their tables are byte-identical, which is exactly
// when sharing is harmless
template <size_t NumTemplateParams, auto Config = codes_of<all_candidate_types>>
struct shared_type_table {
    static constexpr size_t Total = static_pow(N_CANDIDATES, NumTemplateParams);
    static constexpr auto value = []<size_t... Is>(std::index_sequence<Is...>) {
        return std::array<std::array<uint32_t, NumTemplateParams>, sizeof...(Is)>{
            make_type_entry<Is, NumTemplateParams>(
                std::make_index_sequence<NumTemplateParams>{}
            )...
        };
    }(std::make_index_sequence<Total>{});
};


// ── DISPATCH ENTRY POINT ──────────────────────────────────────────────────────
//
// ArgToTemplateMap maps argument positions to template parameter indices,
// e.g. {0, 0, 1} means args 0 and 1 share template param T, arg 2 uses U.
// -1 means the argument is not templated (fixed type)
//
// NULL (NILSXP) never drives deduction: a template param's runtime type comes from
// its first non-NULL argument. A param whose args are all NULL is undeduced and acts
// as a wildcard in a second scan pass (the runtime mirror of the r_sexp sentinel),
// landing on the first instantiation that satisfies the constraints - with the NULL
// itself preserved by the r_to_cpp boundary conversion. The wildcard follows candidate
// order, so a constraint admitting both classed and plain types hands an all-NULL param
// to the classed type first (r_factors before r_vec<r_lgl>)

template <size_t NumTemplateParams, size_t NumArgs, std::array<int, NumArgs> ArgToTemplateMap,
          typename Functor, typename... SexpArgs>
SEXP dispatch_template_impl(Functor&& functor, SexpArgs&&... sexp_args) {
    static_assert(sizeof...(SexpArgs) == NumArgs, "Argument count mismatch");

    using F = std::remove_reference_t<Functor>;
    SEXP args[NumArgs > 0 ? NumArgs : 1] = { static_cast<SEXP>(sexp_args)... };

    constexpr size_t Total = static_pow(N_CANDIDATES, NumTemplateParams);
    constexpr const auto& dispatch_table =
        per_functor_dispatch_table<NumTemplateParams, NumArgs, F>::value;
    constexpr const auto& type_table = shared_type_table<NumTemplateParams>::value;

    // Deduce each param from its first non-NULL argument. Plain loops to avoid a
    // Clang 22 ICE with NTTP std::array forwarded through nested templates
    uint32_t runtime_types[NumTemplateParams > 0 ? NumTemplateParams : 1]{};
    bool has_undeduced = false;
    for (size_t k = 0; k < NumTemplateParams; ++k) {
        uint16_t param_type = NILSXP;
        size_t param_arg = 0;
        for (size_t i = 0; i < NumArgs; ++i) {
            if (ArgToTemplateMap[i] != static_cast<int>(k)) {
                continue;
            }
            uint16_t arg_type = static_cast<uint16_t>(CPPALLY_TYPEOF(args[i]));
            if (arg_type == NILSXP) {
                continue;
            }
            if (param_type == NILSXP) {
                param_type = arg_type;
                param_arg = i;
            } else if (arg_type != param_type) {
                abort(
                    "R type: %s for arg %zu does not match the first instance: %s for this template arg",
                    r_type_to_str(static_cast<SEXPTYPE>(arg_type)), i + 1,
                    r_type_to_str(static_cast<SEXPTYPE>(param_type))
                );
            }
        }
        runtime_types[k] = static_cast<uint32_t>(param_type);
        if (param_type == NILSXP) {
            has_undeduced = true;
        }
        // Reject before the scan: an excluded type has no candidate of its own, so
        // the r_sexp wildcard would otherwise silently claim it
        if constexpr (has_exclusions) {
            if (param_type != NILSXP && is_excluded_code(static_cast<uint32_t>(param_type))) {
                abort(
                    "Argument %zu is of R type %s, which this package excludes from its "
                    "dispatch candidates. Restore it with `use_template_dispatch_candidates()`",
                    param_arg + 1, r_type_to_str(param_type)
                );
            }
        }
    }

    // The one match rule, shared by the scan and the error report below
    auto accepts = [&](size_t I, size_t K, bool null_wildcard) {
        return type_table[I][K] == wildcard_code
            || (null_wildcard && runtime_types[K] == NILSXP)
            || type_table[I][K] == runtime_types[K];
    };

    // Linear scan - one indirect call through void*, no inlining possible
    auto find_match = [&](bool null_wildcard) -> erased_fn_t {
        for (size_t I = 0; I < Total; ++I) {
            erased_fn_t fn = dispatch_table[I];
            if (!fn) {
                continue;
            }
            bool match = true;
            for (size_t K = 0; K < NumTemplateParams; ++K) {
                if (!accepts(I, K, null_wildcard)) {
                    match = false;
                    break;
                }
            }
            if (match) {
                return fn;
            }
        }
        return nullptr;
    };

    // The second pass runs only after the first, so that an r_sexp instantiation,
    // when the constraints admit one, still claims NULL in pass 1
    erased_fn_t fn = find_match(false);
    if (!fn && has_undeduced) {
        fn = find_match(true);
    }
    if (fn) {
        return fn(static_cast<void*>(&functor), args);
    }

    // Report the first template param whose type no valid instantiation accepts at
    // that position; if every param is individually acceptable, the types only fail
    // in combination
    for (size_t K = 0; K < NumTemplateParams; ++K) {
        if (runtime_types[K] == NILSXP) {
            continue;
        }
        bool satisfiable = false;
        for (size_t I = 0; I < Total && !satisfiable; ++I) {
            satisfiable = dispatch_table[I] != nullptr && accepts(I, K, false);
        }
        if (!satisfiable) {
            size_t arg = 0;
            for (size_t i = 0; i < NumArgs; ++i) {
                if (ArgToTemplateMap[i] == static_cast<int>(K)) {
                    arg = i;
                    break;
                }
            }
            abort(
                "Argument %zu of type %s does not satisfy the template constraints",
                arg + 1, r_type_to_str(static_cast<SEXPTYPE>(runtime_types[K]))
            );
        }
    }
    abort("Supplied types do not satisfy the template constraints in combination");
    return nullptr;
}

} // namespace internal

} // namespace cppally


#endif
