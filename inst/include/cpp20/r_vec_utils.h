#ifndef CPP20_VECTOR_UTILS_H
#define CPP20_VECTOR_UTILS_H

#include <cpp20/r_types.h>
#include <cpp20/r_sym.h>

namespace cpp20 {

namespace internal {

template <typename T>
using vec_ptr_t = std::conditional_t<std::is_const_v<T>, const unwrap_t<T>*, unwrap_t<T>*>;

inline r_sexp new_vec(SEXPTYPE type, r_size_t n){
  return r_sexp(safe[Rf_allocVector](type, n));
}

template <RPtrWritableType T>
inline vec_ptr_t<T> vector_ptr(SEXP x) {
  if constexpr (RTimeType<T>){
    return reinterpret_cast<vec_ptr_t<T>>(vector_ptr<inherited_type_t<T>>(x));
  } else {
    static_assert(
      always_false<T>,
      "Unsupported type for vector_ptr"
    );
    return nullptr;
  }
}

template<>
inline int* vector_ptr<r_lgl>(SEXP x) {
  return LOGICAL(x);
}

template<>
inline const int* vector_ptr<const r_lgl>(SEXP x) {
  return LOGICAL_RO(x);
}
template<>
inline int* vector_ptr<r_int>(SEXP x) {
  return INTEGER(x);
}
template<>
inline const int* vector_ptr<const r_int>(SEXP x) {
  return INTEGER_RO(x);
}

template<>
inline double* vector_ptr<r_dbl>(SEXP x) {
  return REAL(x);
}
template<>
inline const double* vector_ptr<const r_dbl>(SEXP x) {
  return REAL_RO(x);
}
// template <RTimeType T>
// inline T* vector_ptr(SEXP x){
//   using v_t = typename T::value_type;
//   return reinterpret_cast<T*>(vector_ptr<v_t>(x));
// }
// template <RTimeType T>
// inline const T* vector_ptr(SEXP x){
//   return  reinterpret_cast<const T*>(vector_ptr<T>(x));
// }

template<>
inline int64_t* vector_ptr<r_int64>(SEXP x) {
  return reinterpret_cast<int64_t*>(REAL(x));
}
template<>
inline const int64_t* vector_ptr<const r_int64>(SEXP x) {
  return reinterpret_cast<const int64_t*>(REAL_RO(x));
}
template<>
inline std::complex<double>* vector_ptr<r_cplx>(SEXP x) {
  return reinterpret_cast<std::complex<double>*>(COMPLEX(x));
}
template<>
inline const std::complex<double>* vector_ptr<const r_cplx>(SEXP x) {
  return reinterpret_cast<const std::complex<double>*>(COMPLEX_RO(x));
}

template<>
inline unsigned char* vector_ptr<r_raw>(SEXP x) {
  return RAW(x);
}
template<>
inline const unsigned char* vector_ptr<const r_raw>(SEXP x) {
  return RAW_RO(x);
}

// Internal vec constructor
template <RVal T>
inline r_sexp new_vec_impl(r_size_t n) {
  if constexpr (RDateType<T>){
    r_sexp out = new_vec_impl<inherited_type_t<T>>(n);
    Rf_setAttrib(out, symbol::class_sym, Rf_ScalarString(Rf_mkCharCE("Date", CE_UTF8)));
    return out;
  } else if constexpr (RPsxctType<T>){
    r_sexp out = new_vec_impl<inherited_type_t<T>>(n);
    r_sexp cls = new_vec(STRSXP, 2);
    SET_STRING_ELT(cls, 0, Rf_mkCharCE("POSIXct", CE_UTF8));
    SET_STRING_ELT(cls, 1, Rf_mkCharCE("POSIXt", CE_UTF8));
    Rf_setAttrib(out, symbol::class_sym, cls);
    Rf_setAttrib(out, lazy_sym<"tzone">()), Rf_ScalarString(Rf_mkCharCE("UTC", CE_UTF8)));
    return out;
  } else {
    static_assert(
      always_false<T>,
      "Unimplemented `new_vec_impl` specialisation"
    );
    return r_null;
  }
}

template <>
inline r_sexp new_vec_impl<r_lgl>(r_size_t n) {
  return internal::new_vec(LGLSXP, n);
}
template <>
inline r_sexp new_vec_impl<r_int>(r_size_t n){
  return internal::new_vec(INTSXP, n);
}
template <>
inline r_sexp new_vec_impl<r_dbl>(r_size_t n){
  return internal::new_vec(REALSXP, n);
}
template <>
inline r_sexp new_vec_impl<r_int64>(r_size_t n){
  r_sexp out = r_sexp(internal::new_vec(REALSXP, n));
  Rf_setAttrib(out, symbol::class_sym, Rf_ScalarString(r_str("integer64")));
  return out;
}
template <>
inline r_sexp new_vec_impl<r_str_view>(r_size_t n){
  return internal::new_vec(STRSXP, n);
}
template <>
inline r_sexp new_vec_impl<r_str>(r_size_t n){
  return internal::new_vec(STRSXP, n);
}
template <>
inline r_sexp new_vec_impl<r_cplx>(r_size_t n){
  return internal::new_vec(CPLXSXP, n);
}
template <>
inline r_sexp new_vec_impl<r_raw>(r_size_t n){
  return internal::new_vec(RAWSXP, n);
}
template <>
inline r_sexp new_vec_impl<r_sexp>(r_size_t n){
  return internal::new_vec(VECSXP, n);
}

template <RVal T>
inline r_sexp new_scalar_vec(T const& default_value) {

  if constexpr (RDateType<T>){
    r_sexp out = new_scalar_vec<inherited_type_t<T>>(static_cast<inherited_type_t<T>>(default_value));
    Rf_setAttrib(out, symbol::class_sym, Rf_ScalarString(r_str("Date")));
    return out;
  } else if constexpr (RPsxctType<T>){
    r_sexp out = new_scalar_vec<inherited_type_t<T>>(static_cast<inherited_type_t<T>>(default_value));
    r_sexp cls = new_vec(STRSXP, 2);
    SET_STRING_ELT(cls, 0, Rf_mkCharCE("POSIXct", CE_UTF8));
    SET_STRING_ELT(cls, 1, Rf_mkCharCE("POSIXt", CE_UTF8));
    Rf_setAttrib(out, symbol::class_sym, cls);
    Rf_setAttrib(out, lazy_sym<"tzone">(), Rf_ScalarString(r_str("UTC")));
    return out;
  } else {
    static_assert(
      always_false<T>,
      "Unimplemented `new_scalar_vec` specialisation"
    );
    return r_null;
  }
}

template <>
inline r_sexp new_scalar_vec<r_lgl>(r_lgl const& default_value) {
  return r_sexp(Rf_ScalarLogical(unwrap(default_value)));
}
template <>
inline r_sexp new_scalar_vec<r_int>(r_int const& default_value){
  return r_sexp(Rf_ScalarInteger(unwrap(default_value)));
}
template <>
inline r_sexp new_scalar_vec<r_dbl>(r_dbl const& default_value){
  return r_sexp(Rf_ScalarReal(unwrap(default_value)));
}
template <>
inline r_sexp new_scalar_vec<r_int64>(r_int64 const& default_value){
  r_sexp out = new_vec_impl<r_int64>(1);
  vector_ptr<r_int64>(out)[0] = default_value;
  return out;
}
template <>
inline r_sexp new_scalar_vec<r_str_view>(r_str_view const& default_value){
  return r_sexp(Rf_ScalarString(unwrap(default_value)));
}
template <>
inline r_sexp new_scalar_vec<r_str>(r_str const& default_value){
  return r_sexp(Rf_ScalarString(unwrap(default_value)));
}
template <>
inline r_sexp new_scalar_vec<r_cplx>(r_cplx const& default_value){
  return r_sexp(Rf_ScalarComplex(Rcomplex{{default_value.re(), default_value.im()}}));
}
template <>
inline r_sexp new_scalar_vec<r_raw>(r_raw const& default_value){
  return r_sexp(Rf_ScalarRaw(unwrap(default_value)));
}
template <>
inline r_sexp new_scalar_vec<r_sexp>(r_sexp const& default_value){
  r_sexp out = new_vec_impl<r_sexp>(1);
  SET_VECTOR_ELT(out.value, 0, unwrap(default_value));
  return out;
}

}

}

#endif
