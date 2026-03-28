#ifndef CPP20_R_ATTRS_H
#define CPP20_R_ATTRS_H

#include <cpp20/r_setup.h>
#include <cpp20/r_symbols.h>
#include <cpp20/r_env.h>
#include <cpp20/r_vec.h>

namespace cpp20 {

namespace attr {

template <RObject T>
inline bool can_have_attributes(const T& x) noexcept {
  if constexpr (RVector<T> || RMetaVector<T>){
    return true; 
  } else if constexpr (is_sexp<T>){
    switch (TYPEOF(x)){
      case LGLSXP:
      case INTSXP: 
      case REALSXP: 
      case STRSXP: 
      case CPLXSXP: 
      case RAWSXP: 
      case VECSXP: 
      case LISTSXP:
      case ENVSXP:
      case CLOSXP:
      case BUILTINSXP: 
      case SPECIALSXP: 
      case LANGSXP:
      case EXPRSXP: 
      case EXTPTRSXP: 
      case OBJSXP: {
        return true;
      }
      default: {
        return false;
      }
    }
  } else {
    return false;
  }
}

template <RObject T>
inline bool can_have_names(const T& x) noexcept {
  if constexpr (RVector<T> || RMetaVector<T>){
    return true; 
  } else if constexpr (is_sexp<T>){
    switch (TYPEOF(x)){
      case LGLSXP:
      case INTSXP: 
      case REALSXP: 
      case STRSXP: 
      case CPLXSXP: 
      case RAWSXP: 
      case VECSXP: 
      case LISTSXP:
      case ENVSXP:
      case LANGSXP:
      case EXPRSXP: {
        return true;
      }
      default: {
        return false;
      }
    }
  } else {
    return false;
  }
}

template <RObject T>
inline bool inherits1(const T& x, const char *r_cls){
  return Rf_inherits(x, r_cls);
}

// Attributes of x as a list
template <RObject T>
inline r_vec<r_sexp> get_attrs(const T& x) {
  if (can_have_attributes(x)){
    r_sym attributes_fn("attributes");
    SEXP expr = Rf_protect(Rf_lang2(attributes_fn, x));
    SEXP res = Rf_protect(Rf_eval(expr, env::base_env));
    r_vec<r_sexp> out(res);
    Rf_unprotect(2);
    return out;
  } else {
    return r_vec<r_sexp>(r_null);
  }
}
template <RObject T>
inline bool has_attrs(const T& x){
  return Rf_length(ATTRIB(x)) != 0;
}
template <RObject T>
inline r_sexp get_attr(const T& x, const r_sym& sym){
  return r_sexp(Rf_getAttrib(x, sym));
}
template <RObject T, RObject U> 
inline void set_attr(const T& x, const r_sym& sym, const U& value){
  if (can_have_attributes(x)){
    Rf_setAttrib(x, sym, value); 
  }
}
template <RObject T, RStringType U>
inline void set_old_names(const T& x, const r_vec<U>& names){
  if (x.is_null()){
    return;
  } else if (names.is_null()){
    attr::set_attr(x, symbol::names_sym, r_null);
  } else if (names.length() != x.length()){
    abort("`length(names)` must equal `length(x)`");
  } else if (can_have_names(x)){
    Rf_namesgets(x, names);
  } else {
    abort("Cannot add names to unsupported type");
  }
}
template <RObject T>
inline r_vec<r_str_view> get_old_names(const T& x){
  return r_vec<r_str_view>(get_attr(x, symbol::names_sym));
}
template <RObject T>
inline bool has_r_names(const T& x){
  return !get_old_names(x).is_null();
}
template <RObject T>
inline r_vec<r_str_view> get_old_class(const T& x){
  return r_vec<r_str_view>(get_attr(x, symbol::class_sym));
}
template <RObject T, RStringType U>
inline void set_old_class(const T& x, const r_vec<U>& cls){
  if (can_have_attributes(x)){
    Rf_classgets(x, cls);
  }
}
template <RObject T, RStringType U>
inline bool inherits_any(const T& x, const r_vec<U>& classes){
  r_size_t n = classes.length();
  for (r_size_t i = 0; i < n; ++i) {
    if (inherits1(x, classes.view(i).c_str())){
      return true;
    }
  }
  return false;
}
template <RObject T, RStringType U>
inline bool inherits_all(const T& x, const r_vec<U>& classes){
  r_size_t n = classes.length();
  for (r_size_t i = 0; i < n; ++i) {
    if (!inherits1(x, classes.view(i).c_str())){
      return false;
    }
  }
  return true;
}
template <RObject T>
inline void clear_attrs(const T& x){

  auto attrs = get_attrs(x);

  if (attrs.is_null()){
    return;
  }
  auto names = attr::get_old_names(attrs);

  int n = attrs.length();

  for (r_size_t i = 0; i < n; ++i){
    r_sym target_sym = internal::as_r<r_sym>(names.view(i));
    set_attr(x, target_sym, r_null);
  }
}

}

namespace internal {

template <RObject T>
inline void modify_attrs_impl(const T& x, const r_vec<r_sexp>& attrs) {

  if (x.is_null()){
    abort("Cannot add attributes to `NULL`");
  }

  if (attrs.is_null()){
    return;
  }

  auto names = attr::get_old_names(attrs);

  if (names.is_null()){
    abort("attributes must be a named list");
  }

  r_sym attr_nm;

  int n = names.length();

  for (int i = 0; i < n; ++i){
    if (names.view(i) != blank_r_string){
      attr_nm = internal::as_r<r_sym>(names.view(i));
      // We can't add an object as its own attribute in-place (as this will crash R)
      if (x.address() == attrs.view(i).address()){
        r_sexp dup_attr = r_sexp(Rf_duplicate(attrs.view(i)));
        attr::set_attr(x, attr_nm, dup_attr);
      } else {
        attr::set_attr(x, attr_nm, attrs.view(i));
      }
    }
  }
}

}

namespace attr {

template <RObject T>
inline void set_attrs(const T& x, const r_vec<r_sexp>& attrs){
  clear_attrs(x);
  internal::modify_attrs_impl(x, attrs);
}

}

}

#endif

