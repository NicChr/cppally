#ifndef CPPALLY_VECTOR_UTILS_H
#define CPPALLY_VECTOR_UTILS_H

#include <cppally/scalar/scalars.h>
#include <cppally/r_sexp/r_sexp_types.h>
#include <cppally/r_sym.h>
#include <cstring>
#include <array>

namespace cppally {

namespace internal {

inline r_sexp new_vec(SEXPTYPE type, r_size_t n, [[maybe_unused]] bool resizable = false){
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


// Named-attribute as a pointer to a cached symbol and array of default values
struct named_attr {
  r_sym (*sym)() = nullptr;
  std::array<const char*, 2> defaults{};
};

// Attributes that T must have for T to be valid.
// For example, `r_factors` needs "factor" class & levels, `r_vector<r_date>` needs "Date" class.
// Intentionally kept generic.
template <typename T>
constexpr auto core_attrs() noexcept {
  if constexpr (RDataFrame<T>){
    return std::array<named_attr, 3>{{ {cached_sym<"class">, {"data.frame"}}, {cached_sym<"names">}, {cached_sym<"row.names">} }};
  } else if constexpr (RFactor<T>){
    return std::array<named_attr, 2>{{ {cached_sym<"class">, {"factor"}}, {cached_sym<"levels">} }};
  } else if constexpr (RVector<T>){
    using data_t = typename std::remove_cvref_t<T>::data_type;
    if constexpr (RDateType<data_t>){
      return std::array<named_attr, 1>{{ {cached_sym<"class">, {"Date"}} }};
    } else if constexpr (RPsxctType<data_t>){
      return std::array<named_attr, 2>{{ {cached_sym<"class">, {"POSIXct", "POSIXt"}}, {cached_sym<"tzone">, {"UTC"}} }};
    } else if constexpr (is<data_t, r_int64>){
      return std::array<named_attr, 1>{{ {cached_sym<"class">, {"integer64"}} }};
    } else {
      return std::array<named_attr, 0>{};
    }
  } else {
    return std::array<named_attr, 0>{};
  }
}

// Named attribute and its associated SEXP
struct attr_value : named_attr {
  r_sexp value;
};

template <typename T>
using core_attr_values = std::array<attr_value, core_attrs<T>().size()>;

template <typename T>
inline const std::array<r_sexp, core_attrs<T>().size()>& core_attr_defaults() {
  using defaults_t = std::array<r_sexp, core_attrs<T>().size()>;
  static constexpr auto attrs = core_attrs<T>();
  static const defaults_t& defaults = *new defaults_t([]{
    defaults_t out;
    for (std::size_t i = 0; i < attrs.size(); ++i){
      const auto& vals = attrs[i].defaults;
      std::size_t n = 0;
      while (n < vals.size() && vals[n] != nullptr){
        ++n;
      }
      if (n > 0){
        out[i] = new_vec(r_typeof_impl<r_vec<r_str>>, static_cast<r_size_t>(n));
        for (std::size_t j = 0; j < n; ++j){
          SET_STRING_ELT(out[i], static_cast<r_size_t>(j), unwrap(r_str(vals[j])));
        }
        MARK_NOT_MUTABLE(out[i]);
      }
    }
    return out;
  }());
  return defaults;
}

// Current SEXP's core attributes
template <typename T>
inline core_attr_values<T> get_core_attrs(SEXP x) {
  core_attr_values<T> out;
  if constexpr (!core_attrs<T>().empty()){
    static constexpr auto attrs = core_attrs<T>();
    for (std::size_t i = 0; i < attrs.size(); ++i){
      out[i] = {attrs[i], r_sexp(Rf_getAttrib(x, attrs[i].sym()))};
    }
  }
  return out;
}

// Restore core attributes (after overriding them)
inline void restore_core_attr(SEXP x, const attr_value& old) {
  if (is_null(old.value)){
    return;
  }
  r_sym sym = old.sym();
  bool valid = true;
  if (r_addresses_equal(sym, cached_sym<"class">())){
    for (const char* cls : old.defaults){
      if (cls != nullptr && !Rf_inherits(x, cls)){
        valid = false;
        break;
      }
    }
  } else {
    valid = !is_null(Rf_getAttrib(x, sym));
  }
  if (!valid) [[unlikely]] {
    safe[Rf_setAttrib](x, sym, old.value);
  }
}

// Restore possibly removed needed attributes.
// e.g. Dates need "Date" class and factors need "factor" class and levels.
template <typename T>
inline void restore_core_attrs(SEXP x, const core_attr_values<T>& old) {
  for (const attr_value& v : old){
    restore_core_attr(x, v);
  }
}

// Internal vec constructor
template <RVal T>
inline r_sexp new_vec_impl(r_size_t n, bool resizable = false) {

  r_sexp out = new_vec(r_typeof_impl<r_vec<T>>, n, resizable);

  if constexpr (!core_attrs<r_vec<T>>().empty()){
    static constexpr auto attrs = core_attrs<r_vec<T>>();
    const auto& defaults = core_attr_defaults<r_vec<T>>();
    for (std::size_t i = 0; i < attrs.size(); ++i){
      Rf_setAttrib(out, attrs[i].sym(), defaults[i]);
    }
  }

  return out;
}

}

}

#endif
