#ifndef CPPALLY_R_STR_H
#define CPPALLY_R_STR_H

#include <cppally/r_setup.h>
#include <cppally/r_concepts.h>
#include <cppally/r_sexp.h>
#include <cppally/r_sexp_types.h>
#include <cppally/r_lazy.h>

namespace cppally {

// Alias type for CHARSXP
// r_str must never be converted to `SEXP`/`r_sexp`
// all templates assume that `SEXP`/`r_sexp` is reserved for objects that can safely fit into an R list vector
// Furthermore CHARSXP is a special case because it is essentially the only SEXP that already fits into a non-list vector: a character vector
struct r_str {
  r_sexp value;
  using value_type = r_sexp;
  r_str() : value{internal::lazy_str_impl<"">(), internal::view_tag{}} {}
  // Explicit SEXP/const char* -> r_str
  explicit r_str(SEXP x) : value{x} {
    internal::check_valid_construction<r_str>(value);
  }
  explicit r_str(SEXP x, internal::view_tag) : value(x, internal::view_tag{}) {
    internal::check_valid_construction<r_str>(value);
  }
  explicit r_str(r_sexp x) : value(std::move(x)) {
    internal::check_valid_construction<r_str>(value);
  }
  explicit r_str(const char *x) : value(Rf_mkCharCE(x, CE_UTF8)) {}
  // Implicit r_str -> SEXP 
  operator SEXP() const noexcept { return value; }

  // Explicit r_str_view -> r_str
  explicit r_str(r_str_view x);

  const char *c_str() const noexcept {
    return CHAR(value);
  }

  std::string_view cpp_str() const {
    return std::string_view{c_str()};
  }
  
  // Explicit conversions
  explicit operator const char*() const noexcept { return c_str(); }
  explicit operator std::string_view() const { return cpp_str(); }
};

inline r_str r_sexp::address() const {
  char buf[1000];
  std::snprintf(buf, 1000, "%p", static_cast<void*>(value));
  return r_str(buf);
}

// Unsafe (but fast) r_str type
// Similar to std::string_view, it is a view of an r_str/CHARSXP whose lifetime must be shorter than the object it's viewing
struct r_str_view {
  SEXP value; 
  using value_type = SEXP;

  // Constructors
  r_str_view() : value{static_cast<SEXP>(internal::lazy_str_impl<"">())} {}
  explicit r_str_view(SEXP x) : value{x} {
    internal::check_valid_construction<r_str_view>(value);
  }
  explicit r_str_view(SEXP x, internal::view_tag) : value(x) {
    internal::check_valid_construction<r_str_view>(value);
  }
  // explicit r_str_view(const char *x) : value(Rf_mkCharCE(x, CE_UTF8)) {}
  // Implicit r_str_view -> SEXP
  operator SEXP() const noexcept { return value; }
  
  // Implicit r_str -> r_str_view
  r_str_view(const r_str& x) : value(static_cast<SEXP>(x)) {}
  
  const char* c_str() const { return CHAR(value); }
  std::string_view cpp_str() const noexcept { return std::string_view{c_str()}; }


  // Explicit conversions
  explicit operator const char*() const { return c_str(); }
  explicit operator std::string_view() const { return cpp_str(); }
};

inline r_str::r_str(r_str_view x) : value(static_cast<SEXP>(x)) {}

// NA
inline const r_str_view na_str = r_str_view(NA_STRING);

template <internal::name T>
inline r_str cached_str() {
    return r_str(internal::lazy_str_impl<T>());
}

}

#endif
