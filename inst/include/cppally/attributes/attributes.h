#ifndef CPPALLY_R_ATTRS_H
#define CPPALLY_R_ATTRS_H

#include <cppally/r_setup.h>
#include <Rversion.h>
#include <cppally/vector/r_vector.h>
#include <cppally/utils.h>
#include <cppally/r_function.h>
#include <initializer_list>

namespace cppally {

namespace attr {

// Forward declared
inline r_vec<r_sexp> get_attrs(SEXP x);

namespace impl {

inline void set_attr_impl(SEXP x, r_sym sym, SEXP value){

  safe[Rf_setAttrib](x, sym, value);

  if (internal::ptrs_identical(sym, symbol::names_sym)) [[unlikely]] {
    if (auto sp = internal::name_cache().try_lookup(static_cast<SEXP>(x))){
      sp->invalidate();
    }
  } else if (internal::ptrs_identical(sym, symbol::levels_sym)) [[unlikely]] {
    if (auto sp = internal::levels_cache().try_lookup(static_cast<SEXP>(x))){
      sp->invalidate();
    }
  }

}

inline void clear_attrs_impl(SEXP x){
  #if R_VERSION >= R_Version(4, 5, 0)
  CLEAR_ATTRIB(x);
  #else
  r_vec<r_str_view> nms = get_attrs(x).names();
  r_size_t n_attrs = nms.length();
  for (r_size_t i = 0; i < n_attrs; ++i) {
    set_attr_impl(x, r_sym(nms.view(i)), r_null);
  }
  #endif
  // Cached attributes (names, levels) have just been removed from x —
  // invalidate any wrapper caches that point at them.
  if (auto sp = internal::name_cache().try_lookup(x)){
    sp->invalidate();
  }
  if (auto sp = internal::levels_cache().try_lookup(x)){
    sp->invalidate();
  }
}

}

inline bool inherits1(SEXP x, const char *r_cls){
  return Rf_inherits(x, r_cls);
}

// Attributes of x as a list
// x is assumed to be protected
inline r_vec<r_sexp> get_attrs(SEXP x) {
  #if R_VERSION >= R_Version(4, 6, 0)
  return r_vec<r_sexp>(safe[R_getAttributes](x));
  #else
  static r_function& r_attrs_fn = *new r_function("attributes", env::base_env);
  return r_vec<r_sexp>(r_attrs_fn( {r_sexp(x, internal::view_tag{})} ));
  #endif
}

inline bool has_attrs(SEXP x){
  #if R_VERSION >= R_Version(4, 5, 0)
  return ANY_ATTRIB(x);
  #else
  return !get_attrs(x).is_null();
  #endif
}

inline r_sexp get_attr(SEXP x, r_sym sym){
  return r_sexp(Rf_getAttrib(x, sym));
}

template <RObject T>
requires requires(T& x) { x.maybe_ensure_exclusive(); }
inline void set_attr(T& x, r_sym sym, SEXP value){
  x.maybe_ensure_exclusive();
  impl::set_attr_impl(x, sym, value);
}
// Do not use, use equivalent `set_names()` member.
template <RObject T, RStringType U>
inline void set_old_names(T& x, const r_vec<U>& names){
  set_attr(x, symbol::names_sym, names);
}
// Do not use, use equivalent `names()` member.
inline r_vec<r_str_view> get_old_names(SEXP x){
  return r_vec<r_str_view>(get_attr(x, symbol::names_sym));
}
inline r_vec<r_str_view> get_old_class(SEXP x){
  return r_vec<r_str_view>(get_attr(x, symbol::class_sym));
}
template <RObject T, RStringType U>
inline void set_old_class(T& x, const r_vec<U>& cls){
  set_attr(x, symbol::class_sym, cls);
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
template <RObject T>
requires requires(T& x) { x.maybe_ensure_exclusive(); }
inline void clear_attrs(T& x){
  x.maybe_ensure_exclusive();
  impl::clear_attrs_impl(x);
}

}

namespace internal {

template <RObject T>
requires requires(T& x) { x.maybe_ensure_exclusive(); }
inline void modify_attrs_impl(T& x, const r_vec<r_sexp>& attrs) {

  if (x.is_null()) [[unlikely]] {
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
      attr::set_attr(x, attr_nm, attrs.view(i));
    }
  }
}

}

namespace attr {

template <RObject T>
requires requires(T& x) { x.maybe_ensure_exclusive(); }
inline void set_attrs(T& x, const r_vec<r_sexp>& attrs){
  clear_attrs(x);
  internal::modify_attrs_impl(x, attrs);
}

}

}

#endif

