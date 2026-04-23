#ifndef CPPALLY_R_COERCE_IMPL_H
#define CPPALLY_R_COERCE_IMPL_H

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
requires is<T, U>
inline std::remove_cvref_t<T> as(const U& x);

template <typename T, typename U>
inline std::remove_cvref_t<T> as(const U& x);


namespace internal {

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
    return internal::between_impl<common_t>(unwrap(x), min_int, max_int);
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
    return internal::between_impl<common_t>(unwrap(x), min_int64, max_int64);
  } else {
    return false;
  }
}

inline bool parse(std::string_view s, double& out) {
#if defined(__cpp_lib_to_chars_floating_point) || \
    (!defined(_LIBCPP_VERSION) && defined(__GLIBCXX__))
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), out);
    return ec == std::errc{} && ptr == s.data() + s.size();
#else
    // libc++ < 17 does not implement std::from_chars for floating-point types.
    // Use strtod with the "C" locale to match from_chars locale-independence.
    const char* saved = std::setlocale(LC_NUMERIC, nullptr);
    std::string saved_locale(saved ? saved : "C");
    std::setlocale(LC_NUMERIC, "C");
    errno = 0;
    const char* begin = s.data();
    char* end = nullptr;
    out = std::strtod(begin, &end);
    bool ok = end == begin + s.size() && errno != ERANGE;
    std::setlocale(LC_NUMERIC, saved_locale.c_str());
    return ok;
#endif
}

// Coerce functions that account for NA
template <RScalar T>
inline r_lgl as_bool(T const& x){
  if constexpr (is<unwrap_t<T>, int>){
    return r_lgl(unwrap(x));
  } else if constexpr (RMathType<T>){
    return is_na(x) ? na<r_lgl>() : r_lgl(static_cast<bool>(unwrap(x)));
  } else if constexpr (RStringType<T>){
    const char* str = x.c_str();
    if (std::strcmp(str, "TRUE") == 0){
      return r_true;
    } else if ( std::strcmp(str, "FALSE") == 0){
      return r_false;
    } else {
      double res;
      if (!parse(x.cpp_str(), res)){
        return na<r_lgl>();
      }
      return r_lgl(static_cast<bool>(res));
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
    return is_na(x) || !internal::can_be_int(x) ? na<r_int>() : r_int(static_cast<int>(unwrap(x)));
  } else if constexpr (RStringType<T>){
    double res;
    if (!parse(x.cpp_str(), res)){
      return na<r_int>();
    } else if (can_be_int(res)){
      return r_int(static_cast<int>(res));
    } else {
      return na<r_int>();
    }
  } else {
    return na<r_int>();
  }
}
template <RScalar T>
inline r_int64 as_int64(T const& x){
  if constexpr (is<unwrap_t<T>, int64_t>){
    return r_int64(unwrap(x));
  } else if constexpr (RMathType<T>){
    return is_na(x) || !internal::can_be_int64(x) ? na<r_int64>() : r_int64(static_cast<int64_t>(unwrap(x)));
  } else if constexpr (RStringType<T>){
    double res;
    if (!parse(x.cpp_str(), res)){
      return na<r_int64>();
    } else if (can_be_int64(res)){
      return r_int64(static_cast<int64_t>(res));
    } else {
      return na<r_int64>();
    }
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
    double res;
    if (!parse(x.cpp_str(), res)){
      return na<r_dbl>();
    }
    return r_dbl(res);
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
  } else if constexpr (RIntegerType<T> && sizeof(T) <= sizeof(int8_t)){
    return is_na(x) || x < 0 ? na<r_raw>() : r_raw(static_cast<unsigned char>(unwrap(x)));
  } else if constexpr (RMathType<T>){
    using r_t = unwrap_t<T>;
    return is_na(x) || !internal::between_impl(unwrap(x), r_t(0), r_t(255)) ? na<r_raw>() : r_raw(static_cast<unsigned char>(unwrap(x)));
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

// R version of static_cast
template <RScalar T, RScalar U>
struct as_impl;

// Specializations for each target type

template<RScalar U>
struct as_impl<r_lgl, U> {
  static constexpr r_lgl cast(U const& x) {
    return as_bool(x);
  }
};

template<RScalar U>
struct as_impl<r_int, U> {
  static constexpr r_int cast(U const& x) {
    return as_int(x);
  }
};

template<RScalar U>
struct as_impl<r_int64, U> {
  static constexpr r_int64 cast(U const& x) {
    return as_int64(x);
  }
};

template<RScalar U>
struct as_impl<r_dbl, U> {
  static constexpr r_dbl cast(U const& x) {
    return as_double(x);
  }
};

template<RTimeType T, RScalar U>
struct as_impl<T, U> {
  static constexpr T cast(U const& x) {
    using inherited_t = inherited_type_t<T>;

    if constexpr (RDateType<T> && RPsxctType<U>){
      double days = std::floor(static_cast<double>(unwrap(x)) / 86400.0);
      return T(as_impl<inherited_t, r_dbl>::cast(r_dbl(days)));
    } else if constexpr (RPsxctType<T> && RDateType<U>){
      auto seconds = unwrap(x) * 86400;
      return T(as_impl<inherited_t, as_r_scalar_t<decltype(seconds)>>::cast(as_r_scalar(seconds)));
    } else if constexpr (RTimeType<U>){
      return T(as_impl<inherited_t, as_r_scalar_t<unwrap_t<U>>>::cast(as_r_scalar(unwrap(x))));
    } else {
      return T(as_impl<inherited_t, U>::cast(x));
    }
  }
};

template<RScalar U>
struct as_impl<r_cplx, U> {
  static constexpr r_cplx cast(U const& x) {
    return as_complex(x);
  }
};

template<RScalar U>
struct as_impl<r_raw, U> {
  static constexpr r_raw cast(U const& x) {
    return as_raw(x);
  }
};

template<RScalar U>
struct as_impl<r_str_view, U> {
  static r_str_view cast(U const& x) {
    return as_r_string(x);
  }
};

template<RScalar U>
struct as_impl<r_str, U> {
  static r_str cast(U const& x) {
    r_str_view res = as_r_string(x);
    return r_str(unwrap(res));
  }
};

template <RScalar T, RScalar U>
inline T as_scalar_impl(U const& x) {
  if constexpr (is<U, T>){
    return x;
  } else {
    using r_t = std::remove_cvref_t<T>;
    T out = internal::as_impl<r_t, U>::cast(x);
    if (is_na(out) && !is_na(x)) [[unlikely]] {
      abort(
        "Implicit NA coercion detected from %s to %s, please ensure data can be coerced without complete loss of information", 
        internal::type_str<U>(), internal::type_str<T>()
      );
    }
    return out;
  }
}

}

}

#endif
