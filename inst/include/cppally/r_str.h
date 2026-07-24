#ifndef CPPALLY_R_STR_H
#define CPPALLY_R_STR_H

#include <cppally/r_setup.h>
#include <cppally/r_concepts.h>
#include <cppally/r_sexp.h>
#include <cppally/r_sexp_types.h>
#include <cppally/r_lazy.h>
#include <string>
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
    return static_cast<SEXP>(*this) == NA_STRING;
  }

  bool is_utf8() const noexcept {
    return static_cast<bool>(Rf_charIsUTF8(*this) || Rf_charIsASCII(*this));
  }

  r_str as_utf8() const {
    // Rf_translateCharUTF8 does indeed check for UTF8-ness BUT
    // We still need the CHARSXP and so we avoid the overhead of Rf_mkChar directly
    if (is_na() || is_utf8()) {
      return *this;
    }
    // Latin-1 mapping is a (most of the time) fixed, locale-independent byte transform
    // In the case that there is divergence, we fall back to Rf_translateCharUTF8
    if (static_cast<bool>(Rf_charIsLatin1(*this))) {
      const SEXP s = *this;
      const unsigned char *p = reinterpret_cast<const unsigned char *>(CHAR(s));
      const int n = Rf_length(s);
      std::string out;
      // Most Latin-1 text is mostly ASCII, so n is a good guess; grows only if high bytes are dense
      out.reserve(static_cast<std::size_t>(n));
      for (int i = 0; i < n; ++i) {
        const unsigned char b = p[i];
        // In Latin-1 the byte value IS the Unicode code point, so 0x00-0x7F is already UTF-8
        if (b < 0x80) {
          out.push_back(static_cast<char>(b));
        } else if (b >= 0xA0) {
          // 0xA0-0xFF: ISO-8859-1 and Windows CP1252 agree, byte == code point.
          // 2-byte UTF-8: 110xxxxx 10xxxxxx (never wider, since max code point is 0xFF)
          out.push_back(static_cast<char>(0xC0 | (b >> 6)));
          out.push_back(static_cast<char>(0x80 | (b & 0x3F)));
        } else {
          // 0x80-0x9F: ISO-8859-1 (C1 controls) and CP1252 (€, smart quotes, dashes...) diverge,
          // and R may interpret CE_LATIN1 as CP1252 (R >= 3.5.0, notably Windows). Defer to R
          // so the platform-correct semantics are used rather than guessing.
          return r_str(safe[Rf_translateCharUTF8](*this));
        }
      }
      return r_str(Rf_mkCharLenCE(out.data(), static_cast<int>(out.size()), CE_UTF8), internal::no_checks_tag{});
    }
    return r_str(safe[Rf_translateCharUTF8](*this));
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

template <string_literal T>
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

// NA
inline const r_str na_str = r_str::na();
}

}

#endif
