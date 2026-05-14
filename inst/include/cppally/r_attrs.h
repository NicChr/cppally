#ifndef CPPALLY_R_ATTRS_H
#define CPPALLY_R_ATTRS_H

#include <cppally/r_setup.h>
#include <cppally/r_vec.h>
#include <cppally/r_utils.h>

namespace cppally {

namespace attr {

template <RObject T>
inline bool can_have_attributes(const T& x) noexcept {
  if constexpr (RComposite<T>){
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

inline void check_can_have_attributes(SEXP x){
  if (!can_have_attributes(x)) [[unlikely]] {
    abort("%s: `x` cannot have attributes added to it", __func__);
  }
}

template <RObject T>
inline bool can_have_names(const T& x) noexcept {
  if constexpr (RComposite<T>){
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

inline bool inherits1(SEXP x, const char *r_cls){
  return Rf_inherits(x, r_cls);
}

inline bool has_attrs(SEXP x){
  return ANY_ATTRIB(x);
}

// Attributes of x as a list
template <RObject T>
inline r_vec<r_sexp> get_attrs(const T& x) {
  if (can_have_attributes(x) && has_attrs(x)){
    SEXP x_ = x;
    SEXP expr = Rf_protect(Rf_lang2(cppally::cached_sym<"attributes">(), x_));
    SEXP res = Rf_protect(Rf_eval(expr, R_BaseEnv));
    r_vec<r_sexp> out(res);
    Rf_unprotect(2);
    return out;
  } else {
    return r_vec<r_sexp>(r_null);
  }
}
inline r_sexp get_attr(SEXP x, const r_sym& sym){
  return r_sexp(Rf_getAttrib(x, sym));
}
template <RObject T, RObject U>
inline void set_attr(T& x, const r_sym& sym, const U& value){
  check_can_have_attributes(x);
  // Cached attributes: prefer the typed setter when available (it knows the
  // wrapper's exact invalidation needs); otherwise invalidate via the registry
  // and fall through to a raw Rf_setAttrib. Either path keeps the cache coherent.
  if (internal::ptrs_identical(sym, symbol::names_sym)) [[unlikely]] {
    if constexpr (requires { x.set_names(value); }) {
      x.set_names(value);
      return;
    }
    if (auto sp = internal::name_cache().try_lookup(static_cast<SEXP>(x))) sp->invalidate();
  } else if (internal::ptrs_identical(sym, symbol::levels_sym)) [[unlikely]] {
    if constexpr (requires { x.set_levels(value); }) {
      x.set_levels(value);
      return;
    }
    if (auto sp = internal::levels_cache().try_lookup(static_cast<SEXP>(x))) sp->invalidate();
  }
  safe[Rf_setAttrib](x, sym, value);
}
// Thin alias over set_attr — kept for readability and to match the get_old_names
// shape. set_attr does the right thing: typed setter when available, registry
// invalidation + raw Rf_setAttrib otherwise.
template <RObject T, RStringType U>
inline void set_old_names(T& x, const r_vec<U>& names){
  set_attr(x, symbol::names_sym, names);
}
inline r_vec<r_str_view> get_old_names(SEXP x){
  return r_vec<r_str_view>(get_attr(x, symbol::names_sym));
}
inline r_vec<r_str_view> get_old_class(SEXP x){
  return r_vec<r_str_view>(get_attr(x, symbol::class_sym));
}
template <RObject T, RStringType U>
inline void set_old_class(const T& x, const r_vec<U>& cls){
  check_can_have_attributes(x);
  Rf_classgets(x, cls);
}
template <RStringType U>
inline bool inherits_any(SEXP x, const r_vec<U>& classes){
  r_size_t n = classes.length();
  for (r_size_t i = 0; i < n; ++i) {
    if (inherits1(x, classes.view(i).c_str())){
      return true;
    }
  }
  return false;
}
template <RStringType U>
inline bool inherits_all(SEXP x, const r_vec<U>& classes){
  r_size_t n = classes.length();
  for (r_size_t i = 0; i < n; ++i) {
    if (!inherits1(x, classes.view(i).c_str())){
      return false;
    }
  }
  return true;
}
inline void clear_attrs(SEXP x){
  CLEAR_ATTRIB(x);
  // Cached attributes (names, levels) have just been removed from x —
  // invalidate any wrapper caches that point at them.
  if (auto sp = internal::name_cache().try_lookup(x)) sp->invalidate();
  if (auto sp = internal::levels_cache().try_lookup(x)) sp->invalidate();
}

}

namespace internal {

template <RObject T>
inline void modify_attrs_impl(const T& x, const r_vec<r_sexp>& attrs) {

  r_sexp x_ = r_sexp(x, internal::view_tag{});

  if (x_.is_null()) [[unlikely]] {
    abort("Cannot add attributes to `NULL`");
  }

  if (attrs.is_null()){
    return;
  }

  r_vec<r_str_view> names = attrs.names();

  if (names.is_null()) [[unlikely]] { 
    abort("attributes must be a named list");
  }

  r_sym attr_nm;

  int n = names.length();

  for (int i = 0; i < n; ++i){
    if ( (names.view(i) != cached_str<"">()).is_true() ) {
      attr_nm = r_sym(names.view(i));
      // We can't add an object as its own attribute in-place (as this will crash R)
      if (internal::ptrs_identical(x_, attrs.view(i))) [[unlikely]] {
        r_sexp dup_attr = r_sexp(safe[Rf_duplicate](attrs.view(i)));
        attr::set_attr(x_, attr_nm, dup_attr);
      } else [[likely]] {
        attr::set_attr(x_, attr_nm, attrs.view(i));
      }
    }
  }
}

}

namespace attr {

inline void set_attrs(SEXP x, const r_vec<r_sexp>& attrs){
  clear_attrs(x);
  internal::modify_attrs_impl(x, attrs);
}

}

}

#endif

