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

#include <cppally/r_sexp_types.h>
#include <cppally/r_coerce.h>
#include <cstdint> // For uint32_t and similar
#include <tuple>
#include <utility>
#include <array>
#include <limits>
#include <exception>           // for std::exception

// Buffer size for error messages (matches cpp11 default)
#define CPPALLY_ERROR_BUFSIZE 8192

#define BEGIN_CPPALLY                   \
  SEXP err = R_NilValue;              \
  char buf[CPPALLY_ERROR_BUFSIZE] = ""; \
  try {

#define END_CPPALLY                                               \
  }                                                             \
  catch (cppally::internal::unwind_exception & e) {                         \
    err = e.token;                                              \
  }                                                             \
  catch (std::exception & e) {                                  \
    strncpy(buf, e.what(), sizeof(buf) - 1);                    \
    buf[sizeof(buf) - 1] = '\0';                                \
  }                                                             \
  catch (...) {                                                 \
    strncpy(buf, "C++ error (unknown cause)", sizeof(buf) - 1); \
    buf[sizeof(buf) - 1] = '\0';                                \
  }                                                             \
  if (buf[0] != '\0') {                                         \
    Rf_errorcall(R_NilValue, "%s", buf);                        \
  } else if (err != R_NilValue) {                               \
    R_ContinueUnwind(err);                                      \
  }                                                             \
  return R_NilValue;


namespace cppally {


namespace internal {


template <typename> struct fn_traits;


template <typename Ret, typename... Args>
struct fn_traits<Ret(*)(Args...)> {
    using return_type = Ret;
    using args_tuple = std::tuple<Args...>;
    static constexpr size_t arity = sizeof...(Args);
};

// The reverse of above
// Special case - never return CHARSXP
template <typename T>
SEXP cpp_to_sexp(const T& x) {
    if constexpr (RStringType<T>){
        return unwrap(as<r_vec<r_str>>(x));
    } else {
        return as<SEXP>(x);
    }
}


using r_types = std::tuple<
    r_lgl, 
    r_int, 
    r_int64, 
    r_dbl, 
    r_str, 
    r_cplx, 
    r_raw, 
    r_date, 
    r_psxct,
    r_sexp // Catch-all
>;

using r_classed_vector_types = std::tuple<r_factors>;


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


// template <typename T>
// inline void check_r_cpp_mapping(SEXP x){
//     using data_t = std::remove_cvref_t<T>;
//     if constexpr (is_sexp<data_t>) return;
//     if (r_cpp_boundary_map_v<data_t> != CPPALLY_TYPEOF(x)){
//         abort("Expected input type: %s", type_str<data_t>());
//     }
// }


// ArgToTemplateMap maps argument positions to template parameter indices
// e.g., {0, 0, 1} means args 0 and 1 share template param T, arg 2 uses U.
// -1 means the argument is not templated (fixed type)
// This function finds the first argument that "drives" template param TemplateParamIdx
template <size_t TemplateParamIdx, size_t NumArgs, auto ArgToTemplateMap>
constexpr size_t first_arg_for_template() {
    for (size_t i = 0; i < NumArgs; ++i)
        if (ArgToTemplateMap[i] == static_cast<int>(TemplateParamIdx)) return i;
    return NumArgs;
}


// When multiple arguments share the same template parameter (e.g., f(T x, T y)),
// they must all have the same R TYPEOF at runtime
template <size_t TemplateParamIdx, size_t NumArgs, auto ArgToTemplateMap>
void check_template_homogeneity(uint16_t expected_type, SEXP* args) {
    for (size_t i = 0; i < NumArgs; ++i) {
        if (ArgToTemplateMap[i] == static_cast<int>(TemplateParamIdx)) {
            if (CPPALLY_TYPEOF(args[i]) != expected_type) {
                abort(
                    "R type: %s for arg %zu does not match the first instance: %s for this template arg",
                    r_type_to_str(CPPALLY_TYPEOF(args[i])), i + 1, r_type_to_str(expected_type)
                );
            }
        }
    }
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
// Crucially, the final call is through a function pointer.
//
// Type erasure: combo_invoker::invoke takes void* instead of Functor&.
// This allows type_table to be keyed on NumTemplateParams alone (not Functor),
// so it is built once and shared across all registered functions with the same
// number of template parameters. Only dispatch_table remains per-Functor,
// since it requires is_combo_callable<Functor, ...> to determine valid entries.


// Classed types first (highest priority in linear scan), then vectors, then scalars
// r_sexp (catch-all) is last in r_types, so it is always tried last
using all_candidate_types = decltype(std::tuple_cat(
    std::declval<r_classed_vector_types>(),
    std::declval<r_vector_types>(),
    std::declval<r_types>()
));
constexpr size_t N_CANDIDATES = std::tuple_size_v<all_candidate_types>;

static_assert(N_CANDIDATES > 0, "`N_CANDIDATES` must be > 0");


constexpr size_t static_pow(size_t base, size_t exp) {
    size_t r = 1;
    for (size_t i = 0; i < exp; ++i) r *= base;
    return r;
}


// Maps a flat index I to an N-tuple of candidate types by treating I as a
// base-N_CANDIDATES number. Each "digit" selects one type from all_candidate_types.
//
// Example with N=2, N_CANDIDATES=27, I=42:
//   digit 0 = 42 % 27 = 15  → all_candidate_types[15]
//   digit 1 = 42 / 27 =  1  → all_candidate_types[1]
//   result  = tuple<type_15, type_1>
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


// Type table helpers: standalone constexpr functions are more reliably
// evaluated than lambdas inside constexpr contexts
template <size_t I, size_t NumTemplateParams, size_t K>
constexpr uint32_t type_entry_element() {
    using T = std::tuple_element_t<K, combo_t<I, NumTemplateParams>>;
    // uint32_t max sentinel is outside uint16_t range, so never collides
    // with any CPPALLY_TYPEOF value
    if constexpr (is_sexp<T>) return std::numeric_limits<uint32_t>::max();
    else return static_cast<uint32_t>(r_cpp_boundary_map_v<T>);
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


// type_table: NOT per-Functor — depends only on NumTemplateParams and the candidate
// type list. Hoisted into a struct so the static constexpr member is shared across
// all Functor instantiations with the same NumTemplateParams, rather than being
// re-instantiated once per registered function.
template <size_t NumTemplateParams>
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
    static constexpr auto dispatch_table =
        make_dispatch_table<NumTemplateParams, NumArgs, F>(std::make_index_sequence<Total>{});
    // type_table: shared across ALL Functor types with the same NumTemplateParams
    constexpr const auto& type_table = shared_type_table<NumTemplateParams>::value;


    // Collect runtime types — plain loops to avoid Clang 22 ICE
    // with NTTP std::array forwarded through nested templates
    uint32_t runtime_types[NumTemplateParams > 0 ? NumTemplateParams : 1]{};
    for (size_t k = 0; k < NumTemplateParams; ++k) {
        size_t first_arg = NumArgs;
        for (size_t i = 0; i < NumArgs; ++i) {
            if (ArgToTemplateMap[i] == static_cast<int>(k)) {
                first_arg = i;
                break;
            }
        }
        runtime_types[k] = static_cast<uint32_t>(CPPALLY_TYPEOF(args[first_arg]));
        for (size_t i = 0; i < NumArgs; ++i) {
            if (ArgToTemplateMap[i] == static_cast<int>(k)) {
                if (CPPALLY_TYPEOF(args[i]) != static_cast<uint16_t>(runtime_types[k])) {
                    abort(
                        "R type: %s for arg %zu does not match the first instance: %s for this template arg",
                        r_type_to_str(CPPALLY_TYPEOF(args[i])), i + 1,
                        r_type_to_str(static_cast<uint16_t>(runtime_types[k]))
                    );
                }
            }
        }
    }


    // Linear scan — one indirect call through void*, no inlining possible
    for (size_t I = 0; I < Total; ++I) {
        erased_fn_t fn = dispatch_table[I];
        if (!fn) continue;


        bool match = true;
        for (size_t K = 0; K < NumTemplateParams; ++K) {
            if (type_table[I][K] != std::numeric_limits<uint32_t>::max() && type_table[I][K] != runtime_types[K]) {
                match = false;
                break;
            }
        }
        if (match) return fn(static_cast<void*>(&functor), args);
    }


    abort("No matching template instantiation found for input types");
    return nullptr;
}

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC pop_options
#endif

template <auto Fn, typename Ret, typename... Args, size_t... Is>
SEXP invoke_impl(SEXP* sexp_args, std::index_sequence<Is...>) {
    if constexpr (std::is_void_v<Ret>) {
        Fn(as<Args>(sexp_args[Is])...);
        return R_NilValue;
    } else {
        return cpp_to_sexp(Fn(as<Args>(sexp_args[Is])...));
    }
}


} // namespace internal


template <auto Fn, typename... SexpArgs>
SEXP dispatch(SexpArgs... args) {
    using Traits   = internal::fn_traits<decltype(Fn)>;
    using ArgsTuple = typename Traits::args_tuple;
    static_assert(sizeof...(SexpArgs) == Traits::arity, "Argument count mismatch");
    static_assert((is<SexpArgs, SEXP> && ...), "dispatch<Fn>: all arguments must be SEXP");


    SEXP arg_array[] = { args... };
    return []<typename... Args>(SEXP* arr, std::tuple<Args...>*) {
        return internal::invoke_impl<Fn, typename Traits::return_type, Args...>(
            arr, std::make_index_sequence<sizeof...(Args)>{}
        );
    }(arg_array, static_cast<ArgsTuple*>(nullptr));
}


}


#endif
