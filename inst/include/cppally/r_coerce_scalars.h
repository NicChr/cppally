#ifndef CPPALLY_R_COERCE_SCALARS_H
#define CPPALLY_R_COERCE_SCALARS_H

#include <cppally/r_setup.h>
#include <cppally/r_utils.h>
#include <cppally/r_types.h>
#include <cppally/r_limits.h>
#include <cppally/r_nas.h>
#include <cppally/r_vec_utils.h>
#include <limits>
#include <charconv> // For to_chars
#include <cstring> // For strcmp
#include <cstdlib> // For strtod
#include <cerrno>  // For errno
#include <clocale> // For setlocale

namespace cppally {

// Forward declarations of main coercion template as<>
template <typename T, typename U>
std::remove_cvref_t<T> as(const U& x);

namespace internal {

// RScalar -> RVector
// Everything else -> r_sexp
template <typename T>
r_sexp as_list_element(const T& x) {
    if constexpr (RScalar<T>){
      return r_vec<T>(1, x).value;
    } else {
      return as<r_sexp>(x);
    }
}

// Bit of duplication here but is done to avoid unncecessary protections when dealing with e.g. r_str & r_str_view

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

// Assumes no NAs at all
template<typename T>
inline constexpr bool can_be_int(T const& x){
  constexpr int max_int = std::numeric_limits<int>::max();
  constexpr int min_int = -max_int; // Doesn't include lowest int (reserved for NA)

  if constexpr (can_definitely_be_int<T>()){
    return true;
  } else if constexpr (MathType<T>){
    // This should be a 'practical' way to get the wider type of the 2
    using common_t = std::common_type_t<unwrap_t<T>, int>;
    return between_impl<common_t>(unwrap(x), min_int, max_int);
  } else {
    return false;
  }
}
template<typename T>
inline constexpr bool can_be_int64(T const& x){
  constexpr int64_t max_int64 = std::numeric_limits<int64_t>::max();
  constexpr int64_t min_int64 = -max_int64; // Doesn't include lowest int (reserved for NA)

  if constexpr (can_definitely_be_int64<T>()){
    return true;
  } else if constexpr (MathType<T>){
    using common_t = std::common_type_t<unwrap_t<T>, int64_t>;
    return between_impl<common_t>(unwrap(x), min_int64, max_int64);
  } else {
    return false;
  }
}

inline bool parse(const char* s, double& out) {
  const char* end = s + std::strlen(s);
  #if defined(__cpp_lib_to_chars_floating_point) || \
    (!defined(_LIBCPP_VERSION) && defined(__GLIBCXX__))
    auto [ptr, ec] = std::from_chars(s, end, out);
    return ec == std::errc{} && ptr == end;
  #else
    // libc++ < 17 does not implement std::from_chars for floating-point types.
    // Use strtod with the "C" locale to match from_chars locale-independence.
    char saved_locale[64];
    const char* saved = std::setlocale(LC_NUMERIC, nullptr);
    std::snprintf(saved_locale, sizeof(saved_locale), "%s", saved ? saved : "C");
    std::setlocale(LC_NUMERIC, "C");
    errno = 0;
    char* p = nullptr;
    out = std::strtod(s, &p);
    bool ok = p == end && !(errno == ERANGE && std::isinf(out));
    std::setlocale(LC_NUMERIC, saved_locale);
    return ok;
  #endif
}

r_dbl parse_double(const char* x){
  double out;
  if (!parse(x, out)){
    return na<r_dbl>();
  }
  return r_dbl(out);
}

// Coerce functions that account for NA
template <RScalar T>
inline r_lgl as_bool(T const& x){
  if constexpr (is<unwrap_t<T>, int>){
    return unwrap(x) == 0 ? r_false : (is_na(x) ? r_na : r_true);
  } else if constexpr (RMathType<T>){
    return is_na(x) ? na<r_lgl>() : r_lgl(static_cast<bool>(unwrap(x)));
  } else if constexpr (RStringType<T>){
    const char* str = x.c_str();
    if (std::strcmp(str, "TRUE") == 0){
      return r_true;
    } else if ( std::strcmp(str, "FALSE") == 0){
      return r_false;
    } else {
      return as_bool(parse_double(x.c_str()));
    }
  } else {
    return na<r_lgl>();
  }
}
template <RScalar T>
inline r_int as_int(T const& x){
  if constexpr (is<unwrap_t<T>, int>){
    return r_int(unwrap(x));
  } else if constexpr (RMathType<T>){
    return is_na(x) || !can_be_int(x) ? na<r_int>() : r_int(static_cast<int>(unwrap(x)));
  } else if constexpr (RStringType<T>){
    return as_int(parse_double(x.c_str()));
  } else {
    return na<r_int>();
  }
}
template <RScalar T>
inline r_int64 as_int64(T const& x){
  if constexpr (is<unwrap_t<T>, int64_t>){
    return r_int64(unwrap(x));
  } else if constexpr (RMathType<T>){
    return is_na(x) || !can_be_int64(x) ? na<r_int64>() : r_int64(static_cast<int64_t>(unwrap(x)));
  } else if constexpr (RStringType<T>){
    return as_int64(parse_double(x.c_str()));
  } else {
    return na<r_int64>();
  }
}
template <RScalar T>
inline r_dbl as_double(T const& x){
  if constexpr (is<unwrap_t<T>, double>){
    return r_dbl(unwrap(x));
  } else if constexpr (RMathType<T>){
    return is_na(x) ? na<r_dbl>() : r_dbl(static_cast<double>(unwrap(x)));
  } else if constexpr (RStringType<T>){
    return parse_double(x.c_str());
  } else {
    return na<r_dbl>();
  }
}
template <RScalar T>
inline r_cplx as_complex(T const& x){
  if constexpr (is<unwrap_t<T>, std::complex<double>>){
    return r_cplx(unwrap(x));
  } else if constexpr (RMathType<T>){
    return r_cplx{as_double(x), r_dbl(0.0)};
  } else {
    return na<r_cplx>();
  }
}
template <RScalar T>
inline r_raw as_raw(T const& x){
  if constexpr (is<unwrap_t<T>, unsigned char>){
    return r_raw(unwrap(x));
  } else if constexpr (RMathType<T>){
    using r_t = unwrap_t<T>;
    return is_na(x) || !between_impl(unwrap(x), r_t(0), r_t(255)) ? na<r_raw>() : r_raw(static_cast<unsigned char>(unwrap(x)));
  } else {
    return na<r_raw>();
  }
}

inline r_str_view c_str_to_r_str_view(const char* x){
  return r_str_view(Rf_mkCharCE(x, CE_UTF8));
}

// As CHARSXP
template <RScalar T>
inline r_str_view as_r_string(T const& x){
  if constexpr (RStringType<T>){
    return r_str_view(x);
  } else if constexpr (is<T, r_lgl>){
    if (is_na(x)){
      return na<r_str_view>();
    } else if (x.is_true()){
      return cached_str<"TRUE">();
    } else {
      return cached_str<"FALSE">();
    }
  } else if constexpr (RIntegerType<T>){
    if (is_na(x)){
      return na<r_str_view>();
    }
    // return as_r_string(std::to_string(unwrap(x)).c_str()); // C++ one-liner
    char buffer[32];
    auto result = std::to_chars(buffer, buffer + sizeof(buffer), unwrap(x));
    if (result.ec != std::errc{}) {
      abort("Internal error, increase buffer size for string conversion");
    }
    *result.ptr = '\0';  // Null-terminate
    return c_str_to_r_str_view(static_cast<const char *>(buffer));
  } else if constexpr (RFloatType<T>){
    if (is_na(x)){
      return na<r_str_view>();
    }
    char buffer[48];
    auto result = std::to_chars(buffer, buffer + sizeof(buffer), unwrap(x) + unwrap_t<T>(0));
    if (result.ec != std::errc{}) {
      abort("Internal error, increase buffer size for string conversion");
    }
    *result.ptr = '\0';  // Null-terminate
    return c_str_to_r_str_view(static_cast<const char *>(buffer));
  } else if constexpr (RComplexType<T>){
    if (is_na(x)){
      return na<r_str_view>();
    }
    double re = static_cast<double>(unwrap(x).real()) + 0.0;
    double im = static_cast<double>(unwrap(x).imag()) + 0.0;

    char buffer[96];
    if (im >= 0){
      snprintf(buffer, sizeof(buffer), "%g+%gi", re, im);
    } else {
      snprintf(buffer, sizeof(buffer), "%g%gi", re, im);
    }
    return c_str_to_r_str_view(static_cast<const char *>(buffer));
  } else if constexpr (is<T, r_raw>){
    char buffer[8];
    snprintf(buffer, sizeof(buffer), "%02x", x.value);
    return c_str_to_r_str_view(static_cast<const char *>(buffer));
  } else if constexpr (RDateType<T>){
    return x.date_str();
  } else if constexpr (RPsxctType<T>){
    return x.datetime_str();
  } else {
    return na<r_str_view>();
  }
}

template <RScalar T, RScalar U>
inline T scalar_coerce_impl(const U& x) {
  if constexpr (is<T, r_lgl>){
    return as_bool(x);
  } else if constexpr (is<T, r_int>){
    return as_int(x);
  } else if constexpr (is<T, r_int64>){
    return as_int64(x);
  } else if constexpr (is<T, r_dbl>){
    return as_double(x);
  } else if constexpr (is<T, r_cplx>){
    return as_complex(x);
  } else if constexpr (RStringType<T>){
    return T(as_r_string(x));
  } else if constexpr (is<T, r_raw>){
    return as_raw(x);
  } else if constexpr (RTimeType<T>){
    using inherited_t = inherited_type_t<T>;
    if constexpr (RDateType<T> && RPsxctType<U>){
      double days = std::floor(static_cast<double>(unwrap(x)) / 86400.0);
      return T(scalar_coerce_impl<inherited_t, r_dbl>(r_dbl(days)));
    } else if constexpr (RPsxctType<T> && RDateType<U>){
      auto seconds = unwrap(x) * 86400;
      using scalar_t = as_r_scalar_t<decltype(seconds)>;
      return T(scalar_coerce_impl<inherited_t, scalar_t>(scalar_t(seconds)));
    } else if constexpr (RTimeType<U>){
      using scalar_t = as_r_scalar_t<unwrap_t<U>>;
      return T(scalar_coerce_impl<inherited_t, scalar_t>(scalar_t(unwrap(x))));
    } else {
      return T(scalar_coerce_impl<inherited_t, U>(x));
    }
  } else {
    static_assert(always_false<T>);
    return T();
  }
}

template <RScalar T, RScalar U>
inline T scalar_coerce(const U& x) {
  if constexpr (is<U, T>){
    return x;
  } else {
    T out = scalar_coerce_impl<T, U>(x);
    if (is_na(out) && !is_na(x)) [[unlikely]] {
      abort(
        "Implicit NA coercion detected from %s to %s, please ensure data can be coerced without complete loss of information", 
        type_str<U>(), type_str<T>()
      );
    }
    return out;
  }
}

}

}

#endif
