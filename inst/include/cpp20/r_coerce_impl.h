#ifndef CPP20_R_COERCE_IMPL_H
#define CPP20_R_COERCE_IMPL_H

#include <cpp20/r_setup.h>
#include <cpp20/sugar/r_named_arg.h>
#include <cpp20/r_utils.h>
#include <cpp20/r_types.h>
#include <cpp20/r_limits.h>
#include <cpp20/r_nas.h>
#include <cpp20/r_vec_utils.h>
#include <limits>
#include <charconv> // For to_chars

namespace cpp20 {

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

// Coerce functions that account for NA
template<typename T>
inline r_lgl as_bool(T const& x){
  if constexpr (is<unwrap_t<T>, int>){
    return r_lgl(unwrap(x));
  } else if constexpr (MathType<T>){
    return is_na(x) ? na<r_lgl>() : r_lgl(static_cast<bool>(unwrap(x)));
  } else {
    return na<r_lgl>();
  }
}
template<typename T>
inline r_int as_int(T const& x){
  if constexpr (is<unwrap_t<T>, int>){
    return r_int(unwrap(x));
  } else if constexpr (MathType<T>){
    return is_na(x) || !internal::can_be_int(x) ? na<r_int>() : r_int(static_cast<int>(unwrap(x)));
  } else {
    return na<r_int>();
  }
}
template<typename T>
inline r_int64 as_int64(T const& x){
  if constexpr (is<unwrap_t<T>, int64_t>){
    return r_int64(unwrap(x));
  } else if constexpr (MathType<T>){
    return is_na(x) || !internal::can_be_int64(x) ? na<r_int64>() : r_int64(static_cast<int64_t>(unwrap(x)));
  } else {
    return na<r_int64>();
  }
}
template<typename T>
inline r_dbl as_double(T const& x){
  if constexpr (is<unwrap_t<T>, double>){
    return r_dbl(unwrap(x));
  } else if constexpr (MathType<T>){
    return is_na(x) ? na<r_dbl>() : r_dbl(static_cast<double>(unwrap(x)));
  } else {
    return na<r_dbl>();
  }
}
template<typename T>
inline r_cplx as_complex(T const& x){
  if constexpr (is<unwrap_t<T>, std::complex<double>>){
    return r_cplx(unwrap(x));
  } else if constexpr (MathType<T>){
    return r_cplx{as_double(x), r_dbl(0.0)};
  } else {
    return na<r_cplx>();
  }
}
template<typename T>
inline r_raw as_raw(T const& x){
  if constexpr (is<unwrap_t<T>, unsigned char>){
    return r_raw(unwrap(x));
  } else if constexpr (IntegerType<T> && sizeof(T) <= sizeof(int8_t)){
    return is_na(x) || x < 0 ? na<r_raw>() : r_raw(static_cast<unsigned char>(unwrap(x)));
  } else if constexpr (MathType<T>){
    using r_t = unwrap_t<T>;
    return is_na(x) || !internal::between_impl(unwrap(x), r_t(0), r_t(255)) ? na<r_raw>() : r_raw(static_cast<unsigned char>(unwrap(x)));
  } else {
    return na<r_raw>();
  }
}
// As CHARSXP
template<typename T>
inline r_str_view as_r_string(T const& x){
  if constexpr (is<T, r_str_view>){
    return x;
  } else if constexpr (is<T, r_str>){
    return r_str_view(unwrap(x));
  } else if constexpr (std::is_convertible_v<T, const char*>){
    return r_str_view(Rf_mkCharCE(x, CE_UTF8));
  } else if constexpr (is<T, std::string>){
    return r_str_view(x.c_str());
  } else if constexpr (is<T, r_sym>){
    return r_str_view(PRINTNAME(static_cast<SEXP>(x)));
  } else if constexpr (is<T, r_lgl>){
    if (x.is_na()){
      return na<r_str_view>();
    } else if (x.is_true()){
      return as_r_string("TRUE");
    } else {
      return as_r_string("FALSE");
    }
  } else if constexpr (IntegerType<T>){
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
    return as_r_string(static_cast<const char *>(buffer));
  } else if constexpr (FloatType<T>){
    if (is_na(x)){
      return na<r_str_view>();
    }
    char buffer[48];
    auto result = std::to_chars(buffer, buffer + sizeof(buffer), unwrap(x) + unwrap_t<T>(0));
    if (result.ec != std::errc{}) {
      abort("Internal error, increase buffer size for string conversion");
    }
    *result.ptr = '\0';  // Null-terminate
    return as_r_string(static_cast<const char *>(buffer));
  } else if constexpr (ComplexType<T>){
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
    return as_r_string(static_cast<const char *>(buffer));
  } else if constexpr (is<T, r_raw>){
    char buffer[8];
    snprintf(buffer, sizeof(buffer), "%02x", x.value);
    return as_r_string(static_cast<const char *>(buffer));
  } else if constexpr (RDateType<T>){
    return x.date_str();
  } else if constexpr (RPsxctType<T>){
    return x.datetime_str();
  } else if constexpr (RObject<T>){
    if (Rf_length(x) != 1){
      abort("`x` is a non-scalar vector and cannot be converted to an `r_str_view` in %s", __func__);
    }
    r_sexp str = r_sexp(safe[Rf_coerceVector](x, STRSXP));
    return r_str_view(STRING_ELT(str, 0));
  } else {
    static_assert(always_false<T>, "Unsupported type for `as_r_string`");
  }
}

// As SYMSXP
template<typename T>
inline r_sym as_r_sym(T const& x){
  if constexpr (is<T, r_sym>){
    return x;
  } else if constexpr (is<T, const char *>){
    return r_sym(x);
  } else if constexpr (RStringType<T>){
    return r_sym(x.c_str());
  } else {
    r_str_view str = as_r_string(x);
    return r_sym(str.c_str());
  }
}

// CHARSXP is always converted to STRSXP here, see `r_types.h` for info
template<typename T>
inline r_sexp as_sexp(T const& x){
  if constexpr (is<T, r_sexp>){
    return x;
  } else if constexpr (RVector<T>){
    return x.sexp;
  } else if constexpr (std::is_convertible_v<T, SEXP>){
    return r_sexp(static_cast<SEXP>(x));
  } else if constexpr (RVal<T>){
    return r_sexp(new_scalar_vec(x));
  } else {
    return new_scalar_vec(as_r_val(x)); 
  }
}

template<>
inline r_sexp as_sexp<bool>(bool const& x){
  return r_sexp(new_scalar_vec(r_lgl(static_cast<int>(x))));
}
template<>
inline r_sexp as_sexp<const char *>(const char * const& x){
  return new_scalar_vec(as_r_string(x));
}
template<>
inline r_sexp as_sexp<r_sym>(r_sym const& x){
  return r_sexp(x.value, internal::view_tag{});
}
template<>
inline r_sexp as_sexp<r_str_view>(r_str_view const& x){
  return r_sexp(static_cast<SEXP>(x));
}
template<>
inline r_sexp as_sexp<r_str>(r_str const& x){
  return x.value;
}

template<>
inline r_sexp as_sexp<SEXP>(SEXP const& x){ 
  return r_sexp(x);
}

// R version of static_cast
template<typename T, typename U>
struct as_impl {
  static T cast(U const& x) {
    static_assert(
      always_false<T>,
      "Can't `as` this type, use `static_cast`"
    );
    return T{};
  }
};

// Specializations for each target type

template<typename U>
struct as_impl<r_lgl, U> {
  static constexpr r_lgl cast(U const& x) {
    return as_bool(x);
  }
};

template<typename U>
struct as_impl<r_int, U> {
  static constexpr r_int cast(U const& x) {
    return as_int(x);
  }
};

template<typename U>
struct as_impl<r_int64, U> {
  static constexpr r_int64 cast(U const& x) {
    return as_int64(x);
  }
};

template<typename U>
struct as_impl<r_dbl, U> {
  static constexpr r_dbl cast(U const& x) {
    return as_double(x);
  }
};

template<RTimeType T, typename U>
struct as_impl<T, U> {
  static constexpr T cast(U const& x) {
    using inherited_t = inherited_type_t<T>;

    if constexpr (RDateType<T> && RPsxctType<U>){
      double days = std::floor(static_cast<double>(unwrap(x)) / 86400.0);
      return T(as_impl<inherited_t, double>::cast(days));
    } else if constexpr (RPsxctType<T> && RDateType<U>){
      auto seconds = unwrap(x) * 86400;
      return T(as_impl<inherited_t, decltype(seconds)>::cast(seconds));
    } else if constexpr (RTimeType<U>){
      return T(as_impl<inherited_t, unwrap_t<U>>::cast(unwrap(x)));
    } else {
      return T(as_impl<inherited_t, U>::cast(x));
    }
  }
};

template<typename U>
struct as_impl<r_cplx, U> {
  static constexpr r_cplx cast(U const& x) {
    return as_complex(x);
  }
};

template<typename U>
struct as_impl<r_raw, U> {
  static constexpr r_raw cast(U const& x) {
    return as_raw(x);
  }
};

template<typename U>
struct as_impl<r_str_view, U> {
  static r_str_view cast(U const& x) {
    return as_r_string(x);
  }
};

template<typename U>
struct as_impl<r_str, U> {
  static r_str cast(U const& x) {
    r_str_view res = as_r_string(x);
    return r_str(unwrap(res));
  }
};

template<typename U>
struct as_impl<r_sym, U> {
  static r_sym cast(U const& x) {
    return as_r_sym(x);
  }
};

template<typename U>
struct as_impl<r_sexp, U> {
  static r_sexp cast(U const& x) {
    return as_sexp(x);
  }
};

template <RVal T, typename U>
inline T as_r(U const& x) {
  if constexpr (is<U, T>){
    return x;
  } else {
    using r_t = std::remove_cvref_t<T>;
    return internal::as_impl<r_t, U>::cast(x);
  } 
}

// ── as_r<T> for named_arg ──────────────────────────────────────────
template <RVal T, NamedArg U>
inline T as_r(U const& a) {
  return internal::as_r<T>(a.value);
}

}

}

#endif
