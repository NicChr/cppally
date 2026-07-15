#ifndef CPPALLY_R_SYM_H
#define CPPALLY_R_SYM_H

#include <cppally/r_setup.h>
#include <cppally/r_concepts.h>
#include <cppally/r_sexp.h>
#include <cppally/r_sexp_types.h>
#include <cppally/r_str.h>
#include <cppally/r_lazy.h>

namespace cppally {

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
  explicit r_sym(SEXP x, internal::no_checks_tag) : value(x) {}
  explicit r_sym(SEXP x, internal::view_tag, internal::no_checks_tag) : value(x) {}

  explicit r_sym(const char *x) : value(internal::unwind_protect([&] { return Rf_installChar(Rf_mkCharCE(x, CE_UTF8)); })) {}
  explicit r_sym(const r_str_view& x) : value(x.is_na() ? internal::lazy_sym_impl<"NA">() : safe[Rf_installChar](x.value)){}
  explicit r_sym(const r_str& x) : r_sym(r_str_view(x)){}
  
  explicit operator r_sexp() const noexcept { return r_sexp(value, internal::view_tag{}); }
  operator SEXP() const noexcept { return value; }

  // Coercion to r_str
  r_str name() const {
    return r_str(PRINTNAME(value), internal::view_tag{}, internal::no_checks_tag{}); 
  }
  explicit operator r_str_view() const {
    return r_str_view(name());
  }
  explicit operator r_str() const {
    return name();
  }

};

template <internal::name T>
inline r_sym cached_sym() {
    return r_sym(internal::lazy_sym_impl<T>(), internal::no_checks_tag{});
}

namespace symbol {

inline const r_sym class_sym = cached_sym<"class">();
inline const r_sym names_sym = cached_sym<"names">();
inline const r_sym row_names_sym = cached_sym<"row.names">();
inline const r_sym levels_sym = cached_sym<"levels">();

inline r_sym tag(SEXP x){
    return r_sym(TAG(x));
}

}

}

#endif

