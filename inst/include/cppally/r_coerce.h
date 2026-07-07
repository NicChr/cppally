#ifndef CPPALLY_R_COERCE_H
#define CPPALLY_R_COERCE_H

#include <cppally/r_coerce_scalars.h>
#include <cppally/r_vec.h>
#include <cppally/r_visit.h>
#include <cppally/r_sexp_types.h>
#include <cppally/r_copy.h>
#include <vector>

namespace cppally {

namespace internal {


template <typename T>
struct is_std_vector : std::false_type {};
template <typename T, typename A>
struct is_std_vector<std::vector<T, A>> : std::true_type {};
template <typename T>
inline constexpr bool is_std_vector_v = is_std_vector<std::remove_cvref_t<T>>::value;

template <typename T>
concept CppVector = is_std_vector_v<T>;

// any cppally type that isn't SEXP or r_sexp
template <typename T>
concept NotSexp = CppallyType<T> && !is_sexp<T>;
template <typename T>
concept AnySexp = is_sexp<T>;

// Powerful and flexible coercion function that can handle many types and convert to R-specific C++ types and R vectors
// Note: forwards declarations exist in r_coerce_scalars.h

// -> SEXP
template <AnySexp T, typename U>
inline T as_impl(const U& x) {
  if constexpr (is<T, SEXP>){
    if constexpr (RObject<U>){
      return static_cast<SEXP>(x);
    } else {
      return static_cast<SEXP>(new_scalar_vec(as_r_scalar_t<U>(x)));
    }
  } else {
    if constexpr (RVector<U>){
      return x.value;
    } else if constexpr (RComposite<U>){
      return x.value.value;
    } else if constexpr (RObject<U>){
      return T(static_cast<SEXP>(x));
    } else {
      return new_scalar_vec(as_r_scalar_t<U>(x));
    }
  }
}

// SEXP -> !SEXP
// Always visit the SEXP and then convert using the disambiguated type
template <NotSexp T, AnySexp U>
inline T as_impl(const U& x) {
  return internal::visit_sexp(r_sexp(x), []<typename x_t> (const x_t& xvec) -> T {
    if constexpr (is<x_t, r_sexp>){
      abort("Don't know how to visit this `r_sexp`");
    } else {
      return as<T>(xvec);
    }
  });
}

// ----- Scalars -----

template <RScalar T, CastableToRScalar U>
requires (CppType<U>)
inline T as_impl(const U& x) {
  return as<T>(as_r_scalar_t<U>(x));
}

template <CastableToRScalar T, typename U>
requires (CppType<T>)
inline T as_impl(const U& x) {
  return static_cast<T>(unwrap(as<as_r_scalar_t<T>>(x)));
}

template <RScalar T, RScalar U>
inline T as_impl(const U& x) {
  return internal::scalar_coerce<T>(x);
}

// ----- Vectors -----

template <RVector T, RVector U>
inline T as_impl(const U& x) {
  using to_data_t = typename T::data_type;
  using from_data_t = typename U::data_type;

  if constexpr (RStringType<to_data_t> && RStringType<from_data_t>){
    return T(x.value);
  }

  if constexpr (is<to_data_t, r_str>){
    return T(as<r_vec<r_str_view>>(x));
  }

  r_size_t n = x.length();
  T out(n);
  for (r_size_t i = 0; i < n; ++i){
    out.set(i, x.view(i));
  }
  out.set_names(x.names());
  return out;
}

template <RScalar T, RVector U>
inline T as_impl(const U& x) {
  if (x.length() != 1) [[unlikely]] {
    abort("Vector must be length-1 to be coerced to requested scalar type");
  }
  return as<T>(x.get(0));
}

template <CppVector T, CppVector U>
inline T as_impl(const U& x) {
  r_size_t n = x.size();
  T out(n);
  using data_t = typename T::value_type;
  for (r_size_t i = 0; i < n; ++i){
    out[i] = as<data_t>(x[i]);
  }
  return out;
}

template <CppVector T, RVector U>
inline T as_impl(const U& x) {
  r_size_t n = x.length();
  T out(n);
  using data_t = typename T::value_type;
  for (r_size_t i = 0; i < n; ++i){
    out[i] = as<data_t>(x.get(i));
  }
  return out;
}

template <RVector T, CppVector U>
inline T as_impl(const U& x) {
  r_size_t n = x.size();
  T out(n);
  for (r_size_t i = 0; i < n; ++i) out.set(i, x[i]);
  return out;
}


// ----- Factors -----

template <RScalar T, RFactor U>
inline T as_impl(const U& x) {
  if (x.length() != 1) [[unlikely]] {
    abort("Factor must be length-1 to be coerced to requested scalar type");
  }
  return as<T>(x.get(0));
}

template <RVector T, RFactor U>
inline T as_impl(const U& x) {
  T coerced_levels = as<T>(x.levels());
  r_size_t n = x.length();
  T out(n);
  using data_t = typename T::data_type;
  for (r_size_t i = 0; i < n; ++i) {
    r_int code = x.get_code(i);
    if (is_na(code)) {
      out.set(i, na<data_t>());
    } else {
      out.set(i, coerced_levels.view(static_cast<r_size_t>(unwrap(code)) - 1));
    }
  }
  return out;
}

template <RFactor T, RVector U>
inline T as_impl(const U& x) {
  return r_factors(x);
}

// ----- Data Frames -----

template <typename T, RDataFrame U>
requires (RScalar<T> || RVector<T> || RFactor<T>)
inline T as_impl(const U& x) {
  if (x.ncol() != 1) [[unlikely]] {
    abort("r_df must be a 1-column data frame to convert to %s", internal::type_str<T>());
  }
  return as<T>(x.get_col(0));
}

template <RDataFrame T, RVector U>
inline T as_impl(const U& x) {
  return r_df(x);
}

template <RDataFrame T, RFactor U>
inline T as_impl(const U& x) {
  return r_df(x);
}

template <RComposite T, RScalar U>
inline T as_impl(const U& x) {
  if constexpr (RVector<T>){
    using data_t = typename T::data_type;
    return r_vec<data_t>(1, as<data_t>(x)); 
  } else {
    return as<T>(r_vec<U>(1, x));
  }
}

// ----- R Symbols -----

// Route through r_str where possible

template <typename T, RSymbolType U>
requires (RScalar<T> || RComposite<T>)
inline T as_impl(const U& x) {
  return as<T>(x.name());
}

template <RSymbolType T, CStringType U>
inline T as_impl(const U& x) {
  return r_sym(x);
}

template <RSymbolType T, typename U>
requires (RScalar<U> || RComposite<U>)
inline T as_impl(const U& x) {
  return r_sym(as<r_str_view>(x));
}

}

// Convert any obj to an r_vec<>
template <typename T>
inline auto as_vector(const T& x){
  if constexpr (RVector<T>){
    return x;
  } else if constexpr (RFactor<T>){
    return x.value;
  } else if constexpr (CastableToRScalar<T>){
    using scalar_t = as_r_scalar_t<T>;
    return r_vec<scalar_t>(1, scalar_t(x));
  } else {
    static_assert(always_false<T>, "Can't convert `x` to vector, please use `as<>`");
    return T();
  }
}

template <typename T>
requires (CastableToRScalar<T> || RAtomicVector<T> || RFactor<T>)
inline auto as_scalar(const T& x){
  if constexpr (CastableToRScalar<T>){
    return as_r_scalar_t<T>(x);
  } else if constexpr (RAtomicVector<T>){
    return as<typename T::data_type>(x);
  } else if constexpr (RFactor<T>) {
    return as<r_str>(x);
  }
}

template <typename T, typename U>
std::remove_cvref_t<T> as(const U& x) {
  if constexpr (is<T, U>){
    return x;
  } else {
    return internal::as_impl<std::remove_cvref_t<T>>(x);
  }
}

}

#endif
