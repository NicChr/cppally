#ifndef CPPALLY_R_DISPATCH_H
#define CPPALLY_R_DISPATCH_H

// Runtime dispatch for templated and non-templated C++ functions registered to R
// The dispatch is mainly driven by a modified version of R's TYPEOF
// This is done by providing a map of C++20 types to SEXP tag-types
//
// For non-templated functions, inputs are simply coerced to the specified type
// For templated functions, templated arguments are verified by first applying the SEXP/C++ mapping and
// then checking that the constraints of the template are satisfied.
// Where there are one-to-many mappings, vector and scalars are both used to check if either of them can satisfy the constraints

#include <cppally/r_sexp/r_sexp_types.h>
#include <cppally/r_coerce.h>
#include <cstdint> // For uint32_t and similar
#include <tuple>
#include <utility>
#include <array>
#include <limits>
#include <cstring>             // for strncpy
#include <exception>           // for std::exception

// Buffer size for error messages (matches cpp11 default)
#define CPPALLY_ERROR_BUFSIZE 8192

// Opts a vague-linkage symbol out of STB_GNU_UNIQUE binding on ELF, keeping it
// private to its shared library.
#include <R_ext/Visibility.h>
#define CPPALLY_HIDDEN attribute_hidden

// The locals carry a cppally_ prefix so they cannot collide with a registered
// function's parameter names, which share the outermost block with them
#define BEGIN_CPPALLY                             \
  SEXP cppally_err_ = R_NilValue;               \
  char cppally_buf_[CPPALLY_ERROR_BUFSIZE];     \
  cppally_buf_[0] = '\0';                       \
  try {

#define END_CPPALLY                                                             \
  }                                                                           \
  catch (cppally::internal::unwind_exception & e) {                           \
    cppally_err_ = e.token;                                                   \
  }                                                                           \
  catch (std::exception & e) {                                                \
    strncpy(cppally_buf_, e.what(), sizeof(cppally_buf_) - 1);                \
    cppally_buf_[sizeof(cppally_buf_) - 1] = '\0';                            \
  }                                                                           \
  catch (...) {                                                               \
    strncpy(cppally_buf_, "C++ error (unknown cause)", sizeof(cppally_buf_) - 1); \
    cppally_buf_[sizeof(cppally_buf_) - 1] = '\0';                            \
  }                                                                           \
  if (cppally_buf_[0] != '\0') {                                              \
    Rf_errorcall(R_NilValue, "%s", cppally_buf_);                             \
  } else if (cppally_err_ != R_NilValue) {                                    \
    R_ContinueUnwind(cppally_err_);                                           \
  }                                                                           \
  return R_NilValue;


namespace cppally {


namespace internal {

// RScalar -> RVector
// Everything else -> SEXP
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


template <typename> struct fn_traits;


template <typename Ret, typename... Args>
struct fn_traits<Ret(*)(Args...)> {
    using return_type = Ret;
    using args_tuple = std::tuple<Args...>;
    static constexpr size_t arity = sizeof...(Args);
};

// ── DISPATCH CANDIDATE SET ────────────────────────────────────────────────────
//
// Package-wide compile-time policy, normally set by `use_template_dispatch_candidates()`.
// Narrowing it cuts the instantiations the dispatcher emits, at the cost of
// narrowing which R types a registered templated function accepts at runtime.
//
// Every TU in a package MUST agree on these macros. The tables are vague-linkage
// symbols, so two TUs disagreeing is an ODR violation the linker will not
// diagnose. Setting them via PKG_CPPFLAGS guarantees agreement. Across shared
// libraries disagreement is fine: `shared_type_table` carries the candidate set
// in its symbol name (see its Config parameter) and `per_functor_dispatch_table`
// is hidden, so it never crosses a library boundary.
//
// Unrelated to the visitor list in r_visit.h, which is a linear switch over a
// different set of types and is deliberately not narrowed by these macros.

// The complete list is kept separately from the active one so the dispatcher can
// tell "excluded by use_template_dispatch_candidates()" apart from "not a cppally type"
#define CPPALLY_DEFAULT_DISPATCH_CANDIDATES \
    r_lgl, r_int, r_int64, r_dbl, r_str, r_cplx, r_raw, r_date, r_psxct

#ifndef CPPALLY_DISPATCH_CANDIDATES
#define CPPALLY_DISPATCH_CANDIDATES CPPALLY_DEFAULT_DISPATCH_CANDIDATES
#endif

using r_scalar_types = std::tuple<CPPALLY_DISPATCH_CANDIDATES>;

// Removes r_sexp and r_vec<r_sexp>
#ifdef CPPALLY_NO_SEXP_CANDIDATE
using r_sexp_candidate = std::tuple<>;
#else
using r_sexp_candidate = std::tuple<r_sexp>;
#endif

using r_types = decltype(std::tuple_cat(
    std::declval<r_scalar_types>(),
    std::declval<r_sexp_candidate>()
));

#if defined(CPPALLY_NO_FACTOR_CANDIDATE) && defined(CPPALLY_NO_DF_CANDIDATE)
using r_classed_vector_types = std::tuple<>;
#elif defined(CPPALLY_NO_FACTOR_CANDIDATE)
using r_classed_vector_types = std::tuple<r_df>;
#elif defined(CPPALLY_NO_DF_CANDIDATE)
using r_classed_vector_types = std::tuple<r_factors>;
#else
using r_classed_vector_types = std::tuple<r_factors, r_df>;
#endif


template<typename Tuple> struct to_r_vec_tuple_impl;
template<typename... Ts>
struct to_r_vec_tuple_impl<std::tuple<Ts...>> {
    using type = std::tuple<r_vec<Ts>...>;
};
using r_vector_types = typename to_r_vec_tuple_impl<r_types>::type;


// ── TYPE BOUNDARY MAP ──


template <typename T> constexpr uint16_t r_cpp_boundary_map_v = r_typeof<T>;


// Essentially make it so that scalars (that have natural vector extensions) can be mapped to from R
// r_sym for example doesn't have a natural vector extension, only a list (VECSXP) can hold it and VECSXP already maps to r_vec<r_sexp>
// To summarise: this specialisation enables users to write scalar inputs to functions (like `r_int` or `r_sym`)
template <RScalar T>
inline constexpr uint16_t r_cpp_boundary_map_v<T> = r_cpp_boundary_map_v<r_vec<T>>;

// Pure C/C++ types that are constructible to an RScalar
template <CastableToRScalar T>
requires (CppScalar<T>)
inline constexpr uint16_t r_cpp_boundary_map_v<T> = r_cpp_boundary_map_v<as_r_scalar_t<T>>;


// ── EXCLUDED TYPES ────────────────────────────────────────────────────────────

using r_default_candidate_types = decltype(std::tuple_cat(
    std::declval<std::tuple<CPPALLY_DEFAULT_DISPATCH_CANDIDATES>>(),
    std::declval<std::tuple<r_factors, r_df>>()
));

using r_active_candidate_types = decltype(std::tuple_cat(
    std::declval<std::tuple<CPPALLY_DISPATCH_CANDIDATES>>(),
    std::declval<r_classed_vector_types>()
));

// r_sexp is deliberately absent from both: it is the wildcard, so it has no
// meaningful boundary code and can never be excluded
template <typename Tuple> struct boundary_codes;
template <typename... Ts>
struct boundary_codes<std::tuple<Ts...>> {
    static constexpr std::array<uint32_t, sizeof...(Ts)> value{
        static_cast<uint32_t>(r_cpp_boundary_map_v<Ts>)...
    };
};

// Code equality is treated as type identity throughout the dispatcher (type
// table matching, exclusion, shared_type_table's Config), so every candidate
// must map to its own code. A future type missing its r_typeof specialisation
// would inherit the uint16_t max default and silently alias another candidate
static_assert([]{
    constexpr auto codes = boundary_codes<r_default_candidate_types>::value;
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

constexpr auto excluded_codes_pair = []{
    constexpr auto all = boundary_codes<r_default_candidate_types>::value;
    constexpr auto active = boundary_codes<r_active_candidate_types>::value;
    std::array<uint32_t, all.size()> out{};
    size_t n = 0;
    for (size_t i = 0; i < all.size(); ++i) {
        bool found = false;
        for (size_t j = 0; j < active.size(); ++j) {
            if (active[j] == all[i]) {
                found = true;
                break;
            }
        }
        if (!found) {
            out[n] = all[i];
            ++n;
        }
    }
    return std::pair<std::array<uint32_t, all.size()>, size_t>{out, n};
}();

constexpr size_t N_EXCLUDED = excluded_codes_pair.second;
constexpr auto excluded_codes = excluded_codes_pair.first;

// static: reads excluded_codes, which has internal linkage, so an
// external-linkage inline definition would be an ODR violation across TUs
static constexpr bool is_excluded_code(uint32_t code) {
    for (size_t i = 0; i < N_EXCLUDED; ++i) {
        if (excluded_codes[i] == code) {
            return true;
        }
    }
    return false;
}


// ── FLAT FUNCTION POINTER TABLE ───────────────────────────────────────────────

// The dispatcher pre-builds two flat arrays at compile time, indexed by a
// flat combo index I (0 to N_CANDIDATES^NumTemplateParams - 1):
//
//   dispatch_table[I] — nullptr if the combination is invalid for the lambda,
//                        otherwise a pointer to combo_invoker<...>::invoke
//   type_table[I][K]  — the expected CPPALLY_TYPEOF for template param K in combo I
//
// At runtime, a linear scan finds the first entry where:
//   - dispatch_table[I] is non-null (combination is valid for the lambda's concepts)
//   - type_table[I][K] matches the actual CPPALLY_TYPEOF of the K-th template arg
//
// ArgToTemplateMap maps argument positions to template parameter indices
// e.g., {0, 0, 1} means args 0 and 1 share template param T, arg 2 uses U.
// -1 means the argument is not templated (fixed type)
//
// NULL (NILSXP) never drives deduction: a template param's runtime type comes from
// its first non-NULL argument. A param whose args are all NULL is undeduced and acts
// as a wildcard in a second scan pass (the runtime mirror of the r_sexp sentinel),
// landing on the first instantiation that satisfies the constraints — with the NULL
// itself preserved by the r_to_cpp boundary conversion. The wildcard follows
// candidate order, so a constraint admitting both classed and plain types hands an
// all-NULL param to the classed type first (r_factors before r_vec<r_lgl>).
//
// Crucially, the final call is through a function pointer.
//
// Type erasure: combo_invoker::invoke takes void* instead of Functor&.
// This allows type_table to be keyed on NumTemplateParams alone (not Functor),
// so it is built once and shared across all registered functions with the same
// number of template parameters. Only dispatch_table remains per-Functor,
// since it requires is_combo_callable<Functor, ...> to determine valid entries.

// Classed types first (highest priority in linear scan), then vectors, then
// scalars, then r_sexp - the catch-all is its own trailing block, so it is
// always tried last no matter which of the other switches are set

#ifdef CPPALLY_NO_SCALAR_CANDIDATES
using r_scalar_candidates = std::tuple<>;
#else
using r_scalar_candidates = r_scalar_types;
#endif

#ifdef CPPALLY_NO_VECTOR_CANDIDATES
using r_vector_candidates = std::tuple<>;
#else
using r_vector_candidates = r_vector_types;
#endif

using all_candidate_types = decltype(std::tuple_cat(
    std::declval<r_classed_vector_types>(),
    std::declval<r_vector_candidates>(),
    std::declval<r_scalar_candidates>(),
    std::declval<r_sexp_candidate>()
));
constexpr size_t N_CANDIDATES = std::tuple_size_v<all_candidate_types>;

static_assert(
    N_CANDIDATES > 0,
    "No dispatch candidates left:"
    "Re-enable at least one via `use_template_dispatch_candidates()`"
);

static constexpr size_t static_pow(size_t base, size_t exp) {
    size_t r = 1;
    for (size_t i = 0; i < exp; ++i) r *= base;
    return r;
}


// Maps a flat index I to an N-tuple of candidate types by treating I as a
// base-N_CANDIDATES number. Each "digit" selects one type from all_candidate_types.
//
// Example with N=2, N_CANDIDATES=22, I=42:
//   digit 0 = 42 % 22 = 20  → all_candidate_types[20]
//   digit 1 = 42 / 22 =  1  → all_candidate_types[1]
//   result  = tuple<type_1, type_20>
// Digits are prepended as the recursion unwinds, so digit 0 lands in the LAST
// tuple position - the last template param cycles through the candidates
// fastest as I increments
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


// SFINAE check: "Is this lambda callable with these N types and NumArgs SEXP arguments?"
// std::void_t<decltype(...)> puts the entire expression in a deduction context,
// so a failed concept/requires on the lambda becomes a soft substitution failure
// (is_combo_callable = false) rather than a hard compiler error
//
// The ((void)Is, std::declval<SEXP>())... trick:
//   - std::declval<SEXP>() is NOT a pack, so it can't be expanded directly
//   - The comma operator discards each Is value but uses it to drive pack expansion,
//     producing exactly sizeof...(Is) copies of std::declval<SEXP>()
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


// Type-erased function pointer — stores void* instead of Functor&
// This is the key to sharing type_table across all Functor instantiations
using erased_fn_t = SEXP(*)(void*, SEXP*);

// Small invoker: each valid combination becomes a tiny, separate function.
// GCC cannot inline across function pointer calls.
// void* erases the Functor type — cast back inside invoke() to call correctly.
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

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC push_options
#pragma GCC optimize("O1")
#endif

// Uses a template struct (not a constexpr function) to store the pointer value.
// Lambda types are non-literal (non-trivially destructible), making a constexpr
// function templated on Functor illegal in constant expression contexts in GCC.
// A static constexpr member of a template struct has no such restriction.
// Primary: invalid combo → nullptr. Partial specialisation: valid → invoker address.
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


// The code the type table stores for one candidate: the CPPALLY_TYPEOF it is
// matched against, or the wildcard sentinel for r_sexp. uint32_t max is outside
// uint16_t range, so the sentinel never collides with any CPPALLY_TYPEOF value
template <typename T>
constexpr uint32_t candidate_code() {
    if constexpr (is_sexp<T>) {
        return std::numeric_limits<uint32_t>::max();
    } else {
        return static_cast<uint32_t>(r_cpp_boundary_map_v<T>);
    }
}

// The active candidate set as the codes the type table is built from - this is
// the table's identity, used to key shared_type_table's symbol name
template <typename Tuple> struct candidate_codes;
template <typename... Ts>
struct candidate_codes<std::tuple<Ts...>> {
    static constexpr std::array<uint32_t, sizeof...(Ts)> value{
        candidate_code<Ts>()...
    };
};

// Type table helpers: standalone constexpr functions are more reliably
// evaluated than lambdas inside constexpr contexts
template <size_t I, size_t NumTemplateParams, size_t K>
constexpr uint32_t type_entry_element() {
    using T = std::tuple_element_t<K, combo_t<I, NumTemplateParams>>;
    return candidate_code<T>();
}


template <size_t I, size_t NumTemplateParams, size_t... Ks>
constexpr std::array<uint32_t, NumTemplateParams> make_type_entry_impl(std::index_sequence<Ks...>) {
    return { type_entry_element<I, NumTemplateParams, Ks>()... };
}


// dispatch_table: per-Functor — validity depends on is_combo_callable<Functor, ...>
template <size_t NumTemplateParams, size_t NumArgs, typename Functor, size_t... Is>
constexpr auto make_dispatch_table(std::index_sequence<Is...>) {
    return std::array<erased_fn_t, sizeof...(Is)>{
        dispatch_entry_impl<Is, NumTemplateParams, NumArgs, Functor>::value...
    };
}

// CPPALLY_HIDDEN because the Functor key alone does not make this symbol unique
// process-wide: a rebuilt shared library of the same name reuses its lambdas'
// mangled names, and GNU-unique binding would pin the FIRST build's table (the
// old library is marked NODELETE, so unloading cannot evict it). The reloaded
// library would then dispatch through stale function pointers - running the
// previous build's code, or scanning a mis-sized table if the candidate flags
// changed between builds. Hidden visibility keeps each library on its own table
template <size_t NumTemplateParams, size_t NumArgs, typename Functor>
struct CPPALLY_HIDDEN per_functor_dispatch_table {
    static constexpr auto value = make_dispatch_table<NumTemplateParams, NumArgs, Functor>(
        std::make_index_sequence<static_pow(N_CANDIDATES, NumTemplateParams)>{}
    );
};


// type_table: NOT per-Functor — depends only on NumTemplateParams and the candidate
// type list. Hoisted into a struct so the static constexpr member is shared across
// all Functor instantiations with the same NumTemplateParams, rather than being
// re-instantiated once per registered function.
//
// Config pins the candidate set into the symbol name and must be left defaulted.
// On Linux, GCC emits `value` as STB_GNU_UNIQUE and glibc resolves such symbols
// process-wide, ignoring RTLD_LOCAL. Keyed on NumTemplateParams alone, two
// shared libraries built with different dispatch candidates would silently
// share whichever table loaded first: the later library then scans a table
// that disagrees with its own dispatch_table, wrongly rejecting valid types,
// dispatching to the wrong instantiation, or reading past the end of a smaller
// table. With Config in the name, two configs share a symbol iff their tables
// are byte-identical, which is exactly when sharing is harmless.
template <size_t NumTemplateParams,
          auto Config = candidate_codes<all_candidate_types>::value>
struct shared_type_table {
    static constexpr size_t Total = static_pow(N_CANDIDATES, NumTemplateParams);
    static constexpr auto value = []<size_t... Is>(std::index_sequence<Is...>) {
        return std::array<std::array<uint32_t, NumTemplateParams>, sizeof...(Is)>{
            make_type_entry_impl<Is, NumTemplateParams>(
                std::make_index_sequence<NumTemplateParams>{}
            )...
        };
    }(std::make_index_sequence<Total>{});
};


// ── DISPATCH ENTRY POINT ─────────────────────────────────────────────────────


template <size_t NumTemplateParams, size_t NumArgs, std::array<int, NumArgs> ArgToTemplateMap,
          typename Functor, typename... SexpArgs>
SEXP dispatch_template_impl(Functor&& functor, SexpArgs&&... sexp_args) {
    static_assert(sizeof...(SexpArgs) == NumArgs, "Argument count mismatch");


    SEXP args[NumArgs > 0 ? NumArgs : 1] = { static_cast<SEXP>(sexp_args)... };
    using F = std::remove_reference_t<Functor>;


    constexpr size_t Total = static_pow(N_CANDIDATES, NumTemplateParams);


    // dispatch_table: built once per unique (Functor type x NumTemplateParams x NumArgs)
    constexpr const auto& dispatch_table =
        per_functor_dispatch_table<NumTemplateParams, NumArgs, F>::value;
    // type_table: shared across ALL Functor types with the same NumTemplateParams
    constexpr const auto& type_table = shared_type_table<NumTemplateParams>::value;


    // Collect runtime types — plain loops to avoid Clang 22 ICE
    // with NTTP std::array forwarded through nested templates
    // NULL args are skipped for both deduction and the homogeneity check
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
        // Reject before the scan: an excluded type has no entry of its own, so
        // the r_sexp wildcard would otherwise claim it and reinterpret the value
        if constexpr (N_EXCLUDED > 0) {
            if (param_type != NILSXP && is_excluded_code(static_cast<uint32_t>(param_type))) {
                abort(
                    "Argument %zu is of R type %s, which this package excludes from its "
                    "dispatch candidates. Restore it with `use_template_dispatch_candidates()`",
                    param_arg + 1, r_type_to_str(param_type)
                );
            }
        }
    }


    // Linear scan — one indirect call through void*, no inlining possible
    auto find_match = [&](bool null_wildcard) -> erased_fn_t {
        for (size_t I = 0; I < Total; ++I) {
            erased_fn_t fn = dispatch_table[I];
            if (!fn) {
                continue;
            }
            bool match = true;
            for (size_t K = 0; K < NumTemplateParams; ++K) {
                if (type_table[I][K] == std::numeric_limits<uint32_t>::max()) {
                    continue;
                }
                if (null_wildcard && runtime_types[K] == NILSXP) {
                    continue;
                }
                if (type_table[I][K] != runtime_types[K]) {
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


    if (erased_fn_t fn = find_match(false)) {
        return fn(static_cast<void*>(&functor), args);
    }
    // Second pass so that an r_sexp instantiation, when the constraints admit one,
    // still claims NULL in pass 1
    if (has_undeduced) {
        if (erased_fn_t fn = find_match(true)) {
            return fn(static_cast<void*>(&functor), args);
        }
    }


    // Find the first template param whose type no valid instantiation accepts
    // at that position; if every param is individually acceptable, the types
    // only fail in combination
    for (size_t K = 0; K < NumTemplateParams; ++K) {
        if (runtime_types[K] == NILSXP) {
            continue;
        }
        bool satisfiable = false;
        for (size_t I = 0; I < Total && !satisfiable; ++I) {
            satisfiable = dispatch_table[I] != nullptr &&
                (type_table[I][K] == std::numeric_limits<uint32_t>::max() ||
                 type_table[I][K] == runtime_types[K]);
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

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC pop_options
#endif

template <auto Fn, typename Ret, typename... Args, size_t... Is>
SEXP invoke_impl(SEXP* sexp_args, std::index_sequence<Is...>) {
    if constexpr (std::is_void_v<Ret>) {
        Fn(r_to_cpp<Args>(sexp_args[Is])...);
        return R_NilValue;
    } else {
        return cpp_to_r(Fn(r_to_cpp<Args>(sexp_args[Is])...));
    }
}


} // namespace internal


template <auto Fn, typename... SexpArgs>
SEXP dispatch(SexpArgs... args) {
    using Traits   = internal::fn_traits<decltype(Fn)>;
    using ArgsTuple = typename Traits::args_tuple;
    static_assert(sizeof...(SexpArgs) == Traits::arity, "Argument count mismatch");
    static_assert((is<SexpArgs, SEXP> && ...), "dispatch<Fn>: all arguments must be SEXP");


    // Sized to at least 1 so a zero-arg Fn does not form a zero-size array
    SEXP arg_array[sizeof...(SexpArgs) > 0 ? sizeof...(SexpArgs) : 1] = { args... };
    return []<typename... Args>(SEXP* arr, std::tuple<Args...>*) {
        return internal::invoke_impl<Fn, typename Traits::return_type, Args...>(
            arr, std::make_index_sequence<sizeof...(Args)>{}
        );
    }(arg_array, static_cast<ArgsTuple*>(nullptr));
}


}


#endif
