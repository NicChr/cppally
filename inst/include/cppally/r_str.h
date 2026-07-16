#ifndef CPPALLY_R_STR_H
#define CPPALLY_R_STR_H

#include <cppally/r_setup.h>
#include <cppally/r_concepts.h>
#include <cppally/r_sexp.h>
#include <cppally/r_sexp_types.h>
#include <cppally/r_lazy.h>
#include <string_view>

namespace cppally {

// R String - cppally version of CHARSXP
// r_str must never be converted to `SEXP`/`r_sexp` except where cppally returns `r_str` to R.
// All templates assume that `SEXP`/`r_sexp` is reserved for objects that can safely fit into an R list vector.
// Furthermore CHARSXP is a special case because it is essentially the only SEXP that already fits into a non-list vector: a character vector.
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
  explicit r_str(r_sexp x, internal::no_checks_tag) : value(std::move(x)) {}
  explicit r_str(SEXP x, internal::no_checks_tag) : value{x} {}
  explicit r_str(SEXP x, internal::view_tag, internal::no_checks_tag) : value(x, internal::view_tag{}) {}

  explicit r_str(const char *x) : value(Rf_mkCharCE(x, CE_UTF8)) {}

  // Implicit r_str -> SEXP 
  operator SEXP() const noexcept { return value; }

  // Explicit r_str_view -> r_str
  explicit r_str(r_str_view x);

  const char *c_str() const noexcept {
    return CHAR(value);
  }

  std::string_view cpp_str() const noexcept {
    return std::string_view{c_str()};
  }

  // Explicit conversions
  explicit operator const char*() const noexcept { return c_str(); }
  explicit operator std::string_view() const noexcept { return cpp_str(); }

  static r_str na() noexcept {
    return r_str(NA_STRING, internal::view_tag{}, internal::no_checks_tag{});
  }

  bool is_na() const noexcept {
    return value.value == NA_STRING;
  }

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
  explicit r_str_view(SEXP x, internal::no_checks_tag) : value{x} {}
  explicit r_str_view(SEXP x, internal::view_tag, internal::no_checks_tag) : value(x) {}
  // Can't construct `r_str_view` from `const char*` — use `r_str` instead
  explicit r_str_view(const char *x) = delete;
  explicit r_str_view(std::string_view x) = delete;
  // Implicit r_str_view -> SEXP
  operator SEXP() const noexcept { return value; }
  
  // Implicit r_str -> r_str_view
  r_str_view(const r_str& x) noexcept : value(static_cast<SEXP>(x)) {}
  
  const char* c_str() const noexcept { return CHAR(value); }
  std::string_view cpp_str() const noexcept { return std::string_view{c_str()}; }


  // Explicit conversions
  explicit operator const char*() const noexcept { return c_str(); }
  explicit operator std::string_view() const noexcept { return cpp_str(); }

  static r_str_view na() noexcept {
    return r_str_view(NA_STRING, internal::no_checks_tag{});
  }

  bool is_na() const noexcept {
    return value == NA_STRING;
  }

};

inline r_str::r_str(r_str_view x) : value(static_cast<SEXP>(x)) {}

template <internal::name T>
inline r_str cached_str() {
    return r_str(internal::lazy_str_impl<T>(), internal::no_checks_tag{});
}

// Memory address
inline r_str address(SEXP x) {
    return r_sexp(x, internal::view_tag{}).address();
}

namespace internal {

// Result is UNPROTECTED! Ensure the result is immediately protected
// e.g. by setting it as an element to r_vec<r_str_view>
// Otherwise just use `r_str()` or `as<r_str>` 
inline r_str_view c_str_to_r_str_view(const char* x){
  return r_str_view(Rf_mkCharCE(x, CE_UTF8), internal::no_checks_tag{});
}

// inline r_str str_concat(std::initializer_list<const char*> parts, const char* sep = ""){
//   std::size_t sep_len = std::strlen(sep), n = parts.size() > 1 ? (parts.size() - 1) * sep_len : 0;
//   for (const char* p : parts){ n += std::strlen(p); }
//   char* buf = R_alloc(n + 1, 1);
//   char* q = buf;
//   bool first = true;
//   for (const char* p : parts){
//     if (!first){ std::memcpy(q, sep, sep_len); q += sep_len; }
//     std::size_t k = std::strlen(p);
//     std::memcpy(q, p, k); q += k;
//     first = false;
//   }
//   *q = '\0';
//   return r_str(Rf_mkCharLenCE(buf, static_cast<int>(n), CE_UTF8), internal::no_checks_tag{});
// }

// NA
inline const r_str na_str = r_str::na();
}

}

#endif
