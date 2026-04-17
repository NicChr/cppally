#ifndef CPP20_R_SYM_H
#define CPP20_R_SYM_H

#include <cpp20/r_setup.h>
#include <cpp20/r_concepts.h>
#include <cpp20/r_sexp.h>
#include <cpp20/r_sexp_types.h>
#include <cpp20/r_str.h>

namespace cpp20 {

namespace internal {
template <std::size_t N>
struct sym_name {
    char data[N];
    constexpr sym_name(const char (&s)[N]) {
      for (std::size_t i = 0; i < N; ++i) data[i] = s[i];
    }
};

// Meyers-singleton method to cache R symbols
template <sym_name name>
inline SEXP lazy_sym_impl() {
    static SEXP s = Rf_installChar(Rf_mkCharCE(name.data, CE_UTF8));
    return s;
}
}

// Alias type for SYMSXP
struct r_sym {
  SEXP value;
  using value_type = r_sexp;

  r_sym() : value(internal::lazy_sym_impl<"NA">()){}
  explicit r_sym(SEXP x) : value{x} {
    internal::check_valid_construction<r_sym>(value);
  }
  explicit r_sym(SEXP x, internal::view_tag) : value(x) {
    internal::check_valid_construction<r_sym>(value);
  }
  explicit r_sym(const char *x) : value(Rf_installChar(Rf_mkCharCE(x, CE_UTF8))) {}
  explicit r_sym(const r_str_view& x) : value(x.value == NA_STRING ? internal::lazy_sym_impl<"NA">() : Rf_installChar(x.value)){}
  explicit r_sym(const r_str& x) : r_sym(r_str_view(x)){}
  operator SEXP() const noexcept { return value; }
};

template <internal::sym_name name>
inline r_sym lazy_sym() {
    return r_sym(internal::lazy_sym_impl<name>());
}

namespace symbol {

inline const r_sym class_sym = r_sym(R_ClassSymbol);
inline const r_sym names_sym = r_sym(R_NamesSymbol);
inline const r_sym row_names_sym = r_sym(R_RowNamesSymbol);
inline const r_sym levels_sym = r_sym(R_LevelsSymbol);
inline r_sym tag(SEXP x){
    return r_sym(TAG(x));
}


// If we find out eager initialisation of R symbols is a problem, use the method below

// Lazy loading (Meyers Singleton method)
// template <SEXP* sym_ptr>
// inline const r_sym& lazy_get_global_sym() {
//     static const r_sym s(*sym_ptr, internal::view_tag{});
//     return s;
// }

// inline const r_sym& class_sym()           { return internal::lazy_get_global_sym<&R_ClassSymbol>(); }
// inline const r_sym& names_sym()           { return internal::lazy_get_global_sym<&R_NamesSymbol>(); }
// inline const r_sym& dim_sym()             { return internal::lazy_get_global_sym<&R_DimSymbol>(); }
// inline const r_sym& dim_names_sym()       { return internal::lazy_get_global_sym<&R_DimNamesSymbol>(); }
// inline const r_sym& row_names_sym()       { return internal::lazy_get_global_sym<&R_RowNamesSymbol>(); }
// inline const r_sym& levels_sym()          { return internal::lazy_get_global_sym<&R_LevelsSymbol>(); }
// inline const r_sym& double_colon_sym()    { return internal::lazy_get_global_sym<&R_DoubleColonSymbol>(); }
// inline const r_sym& triple_colon_sym()    { return internal::lazy_get_global_sym<&R_TripleColonSymbol>(); }
// inline const r_sym& dollar_sym()          { return internal::lazy_get_global_sym<&R_DollarSymbol>(); }
// inline const r_sym& bracket_sym()         { return internal::lazy_get_global_sym<&R_BracketSymbol>(); }
// inline const r_sym& double_brackets_sym() { return internal::lazy_get_global_sym<&R_Bracket2Symbol>(); }
// inline const r_sym& brace_sym()           { return internal::lazy_get_global_sym<&R_BraceSymbol>(); }
// inline const r_sym& dots_sym()            { return internal::lazy_get_global_sym<&R_DotsSymbol>(); }
// inline const r_sym& tsp_sym()             { return internal::lazy_get_global_sym<&R_TspSymbol>(); }
// inline const r_sym& name_sym()            { return internal::lazy_get_global_sym<&R_NameSymbol>(); }
// inline const r_sym& base_sym()            { return internal::lazy_get_global_sym<&R_BaseSymbol>(); }
// inline const r_sym& quote_sym()           { return internal::lazy_get_global_sym<&R_QuoteSymbol>(); }
// inline const r_sym& function_sym()        { return internal::lazy_get_global_sym<&R_FunctionSymbol>(); }
// inline const r_sym& namespace_env_sym()   { return internal::lazy_get_global_sym<&R_NamespaceEnvSymbol>(); }
// inline const r_sym& package_sym()         { return internal::lazy_get_global_sym<&R_PackageSymbol>(); }
// inline const r_sym& seeds_sym()           { return internal::lazy_get_global_sym<&R_SeedsSymbol>(); }
// inline const r_sym& na_rm_sym()           { return internal::lazy_get_global_sym<&R_NaRmSymbol>(); }
// inline const r_sym& source_sym()          { return internal::lazy_get_global_sym<&R_SourceSymbol>(); }
// inline const r_sym& mode_sym()            { return internal::lazy_get_global_sym<&R_ModeSymbol>(); }
// inline const r_sym& device_sym()          { return internal::lazy_get_global_sym<&R_DeviceSymbol>(); }
// inline const r_sym& last_value_sym()      { return internal::lazy_get_global_sym<&R_LastvalueSymbol>(); }
// inline const r_sym& spec_sym()            { return internal::lazy_get_global_sym<&R_SpecSymbol>(); }
// inline const r_sym& previous_sym()        { return internal::lazy_get_global_sym<&R_PreviousSymbol>(); }
// inline const r_sym& sort_list_sym()       { return internal::lazy_get_global_sym<&R_SortListSymbol>(); }
// inline const r_sym& eval_sym()            { return internal::lazy_get_global_sym<&R_EvalSymbol>(); }
// inline const r_sym& drop_sym()            { return internal::lazy_get_global_sym<&R_DropSymbol>(); }

}

}

#endif

