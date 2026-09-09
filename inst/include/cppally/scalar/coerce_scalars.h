#ifndef CPPALLY_R_COERCE_SCALARS_H
#define CPPALLY_R_COERCE_SCALARS_H

#include <cppally/r_setup.h>
#include <cppally/utils.h>
#include <cppally/scalar/scalars.h>
#include <cppally/scalar/arithmetic_ops.h>
#include <cppally/scalar/r_limits.h>
#include <cppally/na.h>
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

inline r_dbl parse_double(const char* x){
  double out;
  if (!parse(x, out)){
    return na<r_dbl>();
  }
  return r_dbl(out);
}

inline char* write_r_double(char* first, char* last, double x){
  if (r_dbl(x).is_infinite()){
    const char* word = x > 0 ? "Inf" : "-Inf";
    while (*word != '\0'){
      *first++ = *word++;
    }
    return first;
  }
  return std::to_chars(first, last, x).ptr;
}

// Coercion functions that account for NA
// Important: all internal::as_* helpers assume that T != U since this identity is already checked by scalar_coerce()

template <RScalar T>
inline r_lgl as_bool(const T& x) {
  
  using unwrapped_t = unwrap_t<T>;

  if constexpr (MathType<unwrapped_t>){
    return is_na(x) ? r_na : r_lgl(static_cast<bool>(unwrap(x)));
  } else if constexpr (RStringType<T>){
    if (x.is_na()){
      return r_na;
    } else if ( (x == cached_str<"TRUE">()).is_true()){
      return r_true;
    } else if ( (x == cached_str<"FALSE">()).is_true()){
      return r_false;
    } else {
      return as_bool(parse_double(x.c_str()));
    }
  } else {
    return r_na;
  }
}
template <RScalar T>
inline r_int as_int(const T& x){

  using unwrapped_t = unwrap_t<T>;

  if constexpr (MathType<unwrapped_t>){
    return is_na(x) || !numeric_can_be_cast_without_complete_loss<int>(unwrap(x)) ? na<r_int>() : r_int(static_cast<int>(unwrap(x)));
  } else if constexpr (RStringType<T>){
    return as_int(parse_double(x.c_str()));
  } else {
    return na<r_int>();
  }
}
template <RScalar T>
inline r_int64 as_int64(const T& x){

  using unwrapped_t = unwrap_t<T>;

  if constexpr (MathType<unwrapped_t>){
    return is_na(x) || !numeric_can_be_cast_without_complete_loss<int64_t>(unwrap(x)) ? na<r_int64>() : r_int64(static_cast<int64_t>(unwrap(x)));
  } else if constexpr (RStringType<T>){
    return as_int64(parse_double(x.c_str()));
  } else {
    return na<r_int64>();
  }
}
template <RScalar T>
inline r_dbl as_double(const T& x){

  using unwrapped_t = unwrap_t<T>;

  if constexpr (MathType<unwrapped_t>){
    return is_na(x) ? na<r_dbl>() : r_dbl(static_cast<double>(unwrap(x)));
  } else if constexpr (RStringType<T>){
    return parse_double(x.c_str());
  } else {
    return na<r_dbl>();
  }
}
template <RScalar T>
inline r_cplx as_complex(const T& x){

  using unwrapped_t = unwrap_t<T>;

  if constexpr (MathType<unwrapped_t>){
    return r_cplx{as_double(x), r_dbl(0.0)};
  } else {
    return na<r_cplx>();
  }
}
template <RScalar T>
inline r_raw as_raw(const T& x){
  
  using unwrapped_t = unwrap_t<T>;

  if constexpr (MathType<unwrapped_t>){
    return is_na(x) || !numeric_can_be_cast_without_complete_loss<unsigned char>(unwrap(x)) ? na<r_raw>() : r_raw(static_cast<unsigned char>(unwrap(x)));
  } else {
    return na<r_raw>();
  }
}

// As CHARSXP
template <RScalar T>
inline r_str_view as_r_string(const T& x){
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
  } else if constexpr (RNumber<T>){
    
    // If NA or NaN
    // Diverging from R here to satisfy the identity: `is_na(r_dbl::nan()) == is_na(as<r_str>(r_dbl::nan()))` 
    // with the rationale being that we are favouring general NA propagation over NaN preservation.
    if (is_na(x)){
      return na<r_str_view>();
    }

    if constexpr (is<T, r_dbl>){
      if (x.is_infinite()){
        return unwrap(x) > 0 ? cached_str<"Inf">() : cached_str<"-Inf">();
      }
    }
    
    char buffer[48];
    auto result = std::to_chars(buffer, buffer + sizeof(buffer) - 1, unwrap(x) + unwrap_t<T>(0));
    
    if (result.ec != std::errc{}) [[unlikely]] {
      abort("Internal error, increase buffer size for string conversion");
    }

    *result.ptr = '\0';
    return c_str_to_r_str_view(static_cast<const char*>(buffer));
  } else if constexpr (RComplexType<T>){
    
    if (is_na(x)){
      return na<r_str_view>();
    }

    double re = static_cast<double>(unwrap(x).real()) + 0.0;
    double im = static_cast<double>(unwrap(x).imag()) + 0.0;

    constexpr int max_dbl_chars = 24;
    char buffer[2 * max_dbl_chars + 3]; // re + '+' + im + 'i' + '\0'

    char* pos = write_r_double(buffer, buffer + max_dbl_chars, re);

    if (im >= 0){
      *pos++ = '+';
    }

    pos = write_r_double(pos, pos + max_dbl_chars, im);
    *pos++ = 'i';
    *pos = '\0';
    return c_str_to_r_str_view(static_cast<const char*>(buffer));
  } else if constexpr (is<T, r_raw>){
    char buffer[8];
    snprintf(buffer, sizeof(buffer), "%02x", x.value);
    return c_str_to_r_str_view(static_cast<const char*>(buffer));
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
    if constexpr (RDateType<T> && RPsxctType<U>){
      return x.as_date();
    } else if constexpr (RPsxctType<T> && RDateType<U>){
      return x.as_datetime();
    } else if constexpr (RDateType<T>) {
      return r_date(as_double(x));
    } else {
      return r_psxct(as_double(x));
    }
  } else {
    static_assert(always_false<T>);
    return T();
  }
}

[[noreturn]] inline CPPALLY_NOINLINE void bad_coercion(const char* from, const char* to){
  abort(
    "Implicit NA coercion detected from %s to %s, please ensure data can be coerced without complete loss of information",
    from, to
  );
}

}

template <RScalar T, RScalar U>
inline T scalar_coerce(const U& x, bool allow_lossy = false) {
  if constexpr (is<U, T>){
    return x;
  } else {
    T out = internal::scalar_coerce_impl<T, U>(x);
    if (!allow_lossy && is_na(out) && !is_na(x)) [[unlikely]] {
      internal::bad_coercion(internal::type_str<U>(), internal::type_str<T>());
    }
    return out;
  }
}

}

#endif
