#ifndef CPPALLY_R_STR_H
#define CPPALLY_R_STR_H

#include <cppally/r_setup.h>
#include <cppally/r_concepts.h>
#include <cppally/r_sexp/r_sexp.h>
#include <cppally/r_sexp/r_sexp_types.h>
#include <cppally/string/string_literal.h>
#include <limits>
#include <string>
#include <string_view>
#include <cstdio> // For snprintf

namespace cppally {

enum class str_encoding {
  native = CE_NATIVE,
  utf8 = CE_UTF8,
  latin1 = CE_LATIN1,
  bytes = CE_BYTES,
  unsupported = 127
};

namespace internal {

inline constexpr std::size_t str_length(const char* x) noexcept {
  return std::string_view(x).size();
}

inline bool str_has_embedded_nul(std::string_view x) noexcept {
  return x.find('\0') != std::string_view::npos;
}

inline SEXP mk_char(const char* x, std::size_t n, str_encoding enc = str_encoding::utf8) {
  if (n > static_cast<std::size_t>(std::numeric_limits<int>::max())) [[unlikely]] {
    abort("string is too large (>= 2^31 bytes long)");
  }
  if (enc == str_encoding::unsupported) [[unlikely]] {
    abort("Unsupported string encoding, supported encodings: UTF-8, latin1, native, and bytes");
  }
  return Rf_mkCharLenCE(x, static_cast<int>(n), static_cast<cetype_t>(enc));
}

inline SEXP mk_char(std::string_view x, str_encoding enc = str_encoding::utf8, bool check_for_embedded_nul = true) {
  if (check_for_embedded_nul && str_has_embedded_nul(x)) [[unlikely]] {
    abort("string contains an embedded nul");
  }
  return mk_char(x.data(), x.size(), enc);
}

}

// Unprotected read-only view of r_str.
// Similar to std::string_view, it is a view of an r_str/CHARSXP whose lifetime must be shorter than the object it's viewing.
struct r_str_view {
  SEXP value; 
  using value_type = SEXP;

  // Constructors
  r_str_view() : value{static_cast<SEXP>(internal::lazy_str_impl<"">())} {}
  explicit r_str_view(SEXP x) : value{x} {
    internal::check_valid_storage<r_str_view>(value);
  }
  explicit r_str_view(SEXP x, internal::view_tag) : r_str_view(x) {}
  explicit r_str_view(SEXP x, internal::no_checks_tag) noexcept : value{x} {}
  explicit r_str_view(SEXP x, internal::view_tag, internal::no_checks_tag) noexcept : r_str_view(x, internal::no_checks_tag{}) {}
  // Can't construct `r_str_view` from `const char*` - use `r_str` instead
  explicit r_str_view(const char *x) = delete;
  explicit r_str_view(std::string_view x) = delete;
  // Implicit r_str_view -> SEXP
  operator SEXP() const noexcept { return value; }
  
  // Implicit r_str -> r_str_view
  r_str_view(const r_str& x) noexcept;
  
  const char* c_str() const noexcept { return CHAR(value); }
  std::string_view cpp_str() const noexcept { return std::string_view{c_str(), static_cast<std::size_t>(n_bytes())}; }

  // Explicit conversions
  explicit operator const char*() const noexcept { return c_str(); }
  explicit operator std::string_view() const noexcept { return cpp_str(); }

  static r_str_view na() noexcept {
    return r_str_view(NA_STRING, internal::no_checks_tag{});
  }

  bool is_na() const noexcept {
    return unwrap(*this) == unwrap(na());
  }

  bool is_empty() const noexcept {
    return unwrap(*this) == R_BlankString;
  }

  int n_bytes() const noexcept {
    return LENGTH(unwrap(*this));
  }

  str_encoding encoding() const noexcept {
    auto r_encoding = Rf_getCharCE(unwrap(*this));
    // Default to unsupported so that even if R adds new encodings, our encoding() returns unspported until they are introduced.
    switch (r_encoding) {
      case CE_NATIVE: return str_encoding::native;
      case CE_UTF8: return str_encoding::utf8;
      case CE_BYTES: return str_encoding::bytes;
      case CE_LATIN1: return str_encoding::latin1;
      default: return str_encoding::unsupported;
    }
  }

  // Is string UTF-8?
  // True for ASCII, UTF-8 marked strings, and native (unmarked) strings in a UTF-8 locale.
  // Does not check that the bytes are valid UTF-8.
  bool is_utf8() const noexcept {
    return static_cast<bool>(Rf_charIsUTF8(unwrap(*this)));
  }

  // Is string ASCII? The bytes ARE checked, so if true, they are also valid UTF-8 and Latin-1.
  bool is_ascii() const noexcept {
    return static_cast<bool>(Rf_charIsASCII(unwrap(*this)));
  }

  // Is string Latin-1?
  // True for ASCII, Latin-1 marked strings, and native (unmarked) strings in a Latin-1 locale.
  // Bytes are not checked, only the mark and locale decide the result.
  bool is_latin1() const noexcept {
    return static_cast<bool>(Rf_charIsLatin1(unwrap(*this)));
  }
  // Would converting to UTF-8 (via `as_utf8()`) change the bytes?
  // NA and strings with `str_encoding::bytes` are not translated.
  bool needs_utf8_translation() const noexcept {
    return !is_na() && !is_utf8() && encoding() != str_encoding::bytes;
  }

};

namespace internal {

// UTF-8 text of x as a std::string, called by r_str::as_utf8()
CPPALLY_NOINLINE inline std::string utf8_string(r_str_view x) {
  if (!x.needs_utf8_translation()) {
    return std::string(x.cpp_str());
  }
  // Latin-1 mapping is a (most of the time) fixed, locale-independent byte transform
  if (x.is_latin1()) {
    const unsigned char *p = reinterpret_cast<const unsigned char *>(x.c_str());
    const int n = x.n_bytes();
    std::string out;
    // Most Latin-1 text is mostly ASCII, so n is a good guess; grows only if high bytes are dense
    out.reserve(static_cast<std::size_t>(n));
    int i = 0;
    for (; i < n; ++i) {
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
        // and R may interpret CE_LATIN1 as CP1252 (notably Windows). Defer to R below.
        break;
      }
    }
    if (i == n) {
      return out;
    }
  }
  // translateCharUTF8 allocates with R_alloc so release it once the bytes are copied
  const void *vmax = vmaxget();
  std::string out(safe[Rf_translateCharUTF8](unwrap(x)));
  vmaxset(vmax);
  return out;
}

}

// R String - cppally version of CHARSXP
// r_str must never be converted to `SEXP`/`r_sexp` except where cppally returns `r_str` to R.
// All templates assume that `SEXP`/`r_sexp` is reserved for objects that can safely fit into an R list vector.
// Furthermore CHARSXP is a special case because it is essentially the only SEXP that already fits into a non-list vector: a character vector.
struct r_str {
  r_sexp value;
  using value_type = r_sexp;
  r_str() : value{internal::lazy_str_impl<"">(), internal::view_tag{}} {}

  explicit r_str(r_sexp x) : value(std::move(x)) {
    internal::check_valid_storage<r_str>(unwrap(value));
  }

  // Explicit SEXP -> r_str
  explicit r_str(SEXP x) : r_str(r_sexp(x)){}
  explicit r_str(SEXP x, internal::view_tag) : r_str(r_sexp(x, internal::view_tag{})) {}
  explicit r_str(r_sexp x, internal::no_checks_tag) : value(std::move(x)) {}
  explicit r_str(SEXP x, internal::no_checks_tag) : r_str(r_sexp(x), internal::no_checks_tag{}) {}
  explicit r_str(SEXP x, internal::view_tag, internal::no_checks_tag) noexcept : value(x, internal::view_tag{}) {}

  explicit r_str(const char* x, str_encoding enc = str_encoding::utf8) :
    r_str(internal::mk_char(x, internal::str_length(x), enc), internal::no_checks_tag{}) {}
  
  // check_for_embedded_nul = false skips checking for NUL bytes. Only use if you are certain the string is clean.
  explicit r_str(std::string_view x, str_encoding enc = str_encoding::utf8, bool check_for_embedded_nul = true) :
    r_str(internal::mk_char(x, enc, check_for_embedded_nul), internal::no_checks_tag{}) {}

  // Implicit r_str -> SEXP 
  operator SEXP() const noexcept { return value; }

  // Explicit r_str_view -> r_str
  explicit r_str(r_str_view x) : value(static_cast<SEXP>(x)) {}

  const char *c_str() const noexcept {
    return r_str_view(*this).c_str();
  }

  std::string_view cpp_str() const noexcept {
    return r_str_view(*this).cpp_str();
  }

  // Explicit conversions
  explicit operator const char*() const noexcept { return c_str(); }
  explicit operator std::string_view() const noexcept { return cpp_str(); }

  static r_str na() noexcept {
    return r_str(r_str_view::na(), internal::view_tag{}, internal::no_checks_tag{});
  }

  bool is_na() const noexcept {
    return r_str_view(*this).is_na();
  }

  bool is_empty() const noexcept {
    return r_str_view(*this).is_empty();
  }

  int n_bytes() const noexcept {
    return r_str_view(*this).n_bytes();
  }

  // Is string UTF-8?
  // True for ASCII, UTF-8 marked strings and native (unmarked) strings in a UTF-8 locale.
  // Does not check that the bytes are valid UTF-8.
  bool is_utf8() const noexcept {
    return r_str_view(*this).is_utf8();
  }

  // Is string ASCII? The bytes ARE checked, so if true, they are also valid UTF-8 and Latin-1.
  bool is_ascii() const noexcept {
    return r_str_view(*this).is_ascii();
  }

  // Is string Latin-1?
  // True for ASCII, Latin-1 marked strings, and native (unmarked) strings in a Latin-1 locale.
  // Bytes are not checked, only the mark and locale decide the result.
  bool is_latin1() const noexcept {
    return r_str_view(*this).is_latin1();
  }

  // Would converting to UTF-8 (via `as_utf8()`) change the bytes?
  // NA and strings with `str_encoding::bytes` are not translated.
  bool needs_utf8_translation() const noexcept {
    return r_str_view(*this).needs_utf8_translation();
  }

  str_encoding encoding() const noexcept {
    return r_str_view(*this).encoding();
  }

  // Mark string with a new encoding, keeping the bytes, so the text can change.
  // To convert the text to UTF-8, use as_utf8().
  [[nodiscard]] r_str with_encoding(str_encoding new_encoding) const {
    if (is_na() || new_encoding == encoding()){
      return *this;
    } else {
      return r_str(cpp_str(), new_encoding, /*check_for_embedded_nul=*/ false);
    }
  }

  // Convert to UTF-8. This matches `base::enc2utf8()`.
  // ASCII and bytes are left as-is.
  [[nodiscard]] r_str as_utf8() const {
    if (is_na() || is_ascii()) {
      return *this;
    }
    str_encoding enc = encoding();
    if (enc == str_encoding::utf8 || enc == str_encoding::bytes) {
      return *this;
    }

    if (is_utf8()) {
      // Construct utf8 marked str directly instead of using with_encoding() to avoid extra NA & encoding check branch
      return r_str(cpp_str(), str_encoding::utf8, /*check_for_embedded_nul=*/ false);
    }
    return r_str(internal::utf8_string(*this), str_encoding::utf8, /*check_for_embedded_nul=*/ false);
  }

};

inline r_str_view::r_str_view(const r_str& x) noexcept : value(static_cast<SEXP>(x)) {}

inline r_str r_sexp::address() const {
  char buf[32];
  std::snprintf(buf, 32, "%p", static_cast<void*>(value));
  return r_str(buf);
}

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
  return r_str_view(mk_char(x, str_length(x)), internal::no_checks_tag{});
}

}

}

#endif
