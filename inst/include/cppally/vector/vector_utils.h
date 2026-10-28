#ifndef CPPALLY_VECTOR_UTILS_H
#define CPPALLY_VECTOR_UTILS_H

#include <cppally/scalar/scalars.h>
#include <cppally/r_sexp/r_sexp_types.h>
#include <cppally/r_sym.h>
#include <cstring>
#include <array>

namespace cppally {

namespace internal {

inline r_sexp new_vec(SEXPTYPE type, r_size_t n, bool resizable = false){
#ifdef CPPALLY_HAS_RESIZABLE_VECTORS
  if (resizable){
    return r_sexp(safe[R_allocResizableVector](type, n));
  }
#endif
  return r_sexp(safe[Rf_allocVector](type, n));
}

template <RVectorisable T>
inline unwrap_t<T>* vector_ptr(SEXP x) {
  if constexpr (RTimeType<T>){
    return reinterpret_cast<unwrap_t<T>*>(vector_ptr<typename T::value_type>(x));
  } else {
    static_assert(
      always_false<T>,
      "Unsupported type for vector_ptr"
    );
    return nullptr;
  }
}

template <RVal T>
inline const unwrap_t<T>* vector_ptr_ro(SEXP x) {
  if constexpr (RTimeType<T>){
    return reinterpret_cast<const unwrap_t<T>*>(vector_ptr_ro<typename T::value_type>(x));
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
inline const int* vector_ptr_ro<r_lgl>(SEXP x) {
  return LOGICAL_RO(x);
}
template<>
inline int* vector_ptr<r_int>(SEXP x) {
  return INTEGER(x);
}
template<>
inline const int* vector_ptr_ro<r_int>(SEXP x) {
  return INTEGER_RO(x);
}

template<>
inline double* vector_ptr<r_dbl>(SEXP x) {
  return REAL(x);
}
template<>
inline const double* vector_ptr_ro<r_dbl>(SEXP x) {
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
inline const int64_t* vector_ptr_ro<r_int64>(SEXP x) {
  return reinterpret_cast<const int64_t*>(REAL_RO(x));
}
template<>
inline std::complex<double>* vector_ptr<r_cplx>(SEXP x) {
  return reinterpret_cast<std::complex<double>*>(COMPLEX(x));
}
template<>
inline const std::complex<double>* vector_ptr_ro<r_cplx>(SEXP x) {
  return reinterpret_cast<const std::complex<double>*>(COMPLEX_RO(x));
}

template<>
inline unsigned char* vector_ptr<r_raw>(SEXP x) {
  return RAW(x);
}
template<>
inline const unsigned char* vector_ptr_ro<r_raw>(SEXP x) {
  return RAW_RO(x);
}

template<>
inline const SEXP* vector_ptr_ro<r_str>(SEXP x) {
  #ifdef STRING_PTR_RO
  return STRING_PTR_RO(x);
  #else
  return ((const SEXP *) DATAPTR_RO(x));
  #endif
}
template<>
inline const SEXP* vector_ptr_ro<r_str_view>(SEXP x) {
  return vector_ptr_ro<r_str>(x);
}
template<>
inline const SEXP* vector_ptr_ro<r_sexp>(SEXP x) {
  #ifdef VECTOR_PTR_RO
  return VECTOR_PTR_RO(x);
  #else
  return ((const SEXP *) DATAPTR_RO(x));
  #endif
}


// ALTREP-safe accessors
template <RVal T>
inline unwrap_t<T> elt(SEXP x, r_size_t i) {
  static_assert(
    always_false<T>,
    "Unsupported type for elt"
  );
  return {};
}

template<>
inline unwrap_t<r_lgl> elt<r_lgl>(SEXP x, r_size_t i) {
  return LOGICAL_ELT(x, i);
}
template<>
inline unwrap_t<r_int> elt<r_int>(SEXP x, r_size_t i) {
  return INTEGER_ELT(x, i);
}
template<>
inline unwrap_t<r_dbl> elt<r_dbl>(SEXP x, r_size_t i) {
  return REAL_ELT(x, i);
}
template<>
inline unwrap_t<r_int64> elt<r_int64>(SEXP x, r_size_t i) {
  double d = REAL_ELT(x, i);
  int64_t v;
  std::memcpy(&v, &d, sizeof(v));
  return v;
}
template<>
inline unwrap_t<r_cplx> elt<r_cplx>(SEXP x, r_size_t i) {
  Rcomplex c = COMPLEX_ELT(x, i);
  return std::complex<double>{c.r, c.i};
}
template<>
inline unwrap_t<r_raw> elt<r_raw>(SEXP x, r_size_t i) {
  return RAW_ELT(x, i);
}
template <>
inline unwrap_t<r_date> elt<r_date>(SEXP x, r_size_t i) {
  return static_cast<unwrap_t<r_date>>(elt<typename r_date::value_type>(x, i));
}

template <>
inline unwrap_t<r_psxct> elt<r_psxct>(SEXP x, r_size_t i) {
  return static_cast<unwrap_t<r_psxct>>(elt<typename r_psxct::value_type>(x, i));
}
template<>
inline unwrap_t<r_str> elt<r_str>(SEXP x, r_size_t i) {
  return STRING_ELT(x, i);
}
template<>
inline unwrap_t<r_str_view> elt<r_str_view>(SEXP x, r_size_t i) {
  return STRING_ELT(x, i);
}
template<>
inline unwrap_t<r_sexp> elt<r_sexp>(SEXP x, r_size_t i) {
  return VECTOR_ELT(x, i);
}


// Immutable class (std::array) of r_vec<T>
template <typename T>
constexpr auto immutable_class() noexcept {
  if constexpr (RDataFrame<T>){
    return std::array<const char*, 1>{"data.frame"};
  } else if constexpr (RFactor<T>){
    return std::array<const char*, 1>{"factor"};
  } else if constexpr (RVector<T>){
    using data_t = typename std::remove_cvref_t<T>::data_type;
    if constexpr (RDateType<data_t>){
      return std::array<const char*, 1>{"Date"};
    } else if constexpr (RPsxctType<data_t>){
      return std::array<const char*, 2>{"POSIXct", "POSIXt"};
    } else if constexpr (is<data_t, r_int64>){
      return std::array<const char*, 1>{"integer64"};
    } else {
      return std::array<const char*, 0>{};
    }
  } else {
    return std::array<const char*, 0>{};
  }
}

// Immutable class (r_sexp) of r_vec<T>
template <typename T>
inline const r_sexp& immutable_class_sexp() {
  static r_sexp& cls = *new r_sexp([]{
    constexpr auto nms = immutable_class<T>();
    r_sexp out = new_vec(STRSXP, static_cast<r_size_t>(nms.size()));
    for (std::size_t i = 0; i < nms.size(); ++i){
      SET_STRING_ELT(out, static_cast<r_size_t>(i), unwrap(r_str(nms[i])));
    }
    MARK_NOT_MUTABLE(out);
    return out;
  }());
  return cls;
}

// If an attribute replacement dropped any of T's required classes, reset them to their defaults
template <typename T>
inline void restore_immutable_class(SEXP x) {
  
  if (r_sexp(x, internal::view_tag{}).is_null()){
    return;
  }

  constexpr auto cls = immutable_class<T>();
  if constexpr (!cls.empty()){
    for (const char* c : cls){
      if (!Rf_inherits(x, c)) [[unlikely]] {
        safe[Rf_setAttrib](x, cached_sym<"class">(), immutable_class_sexp<T>());
        return;
      }
    }
  }
}

// Internal vec constructor
template <RVal T>
inline r_sexp new_vec_impl(r_size_t n, bool resizable = false) {

  r_sexp out = new_vec(r_typeof_impl<r_vec<T>>, n, resizable);
  
  if constexpr (RPsxctType<T>){
    static r_sexp& utc_tz = *new r_sexp(safe[Rf_ScalarString](unwrap(r_str("UTC"))));
    Rf_setAttrib(out, cached_sym<"tzone">(), utc_tz);
  }

  if constexpr (!immutable_class<r_vec<T>>().empty()){
    Rf_setAttrib(out, cached_sym<"class">(), immutable_class_sexp<r_vec<T>>());
  }

  return out;
}

}

}

#endif
