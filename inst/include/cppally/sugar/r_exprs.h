#ifndef CPPALLY_R_EXPRS_H
#define CPPALLY_R_EXPRS_H


#include <cppally/r_setup.h>
#include <cppally/r_sexp.h>
#include <cppally/r_types.h>
#include <cppally/sugar/r_named_arg.h>
#include <cppally/r_coerce.h>

namespace cppally {

inline r_sexp eval(const r_sexp& expr, const r_sexp& env){
  return r_sexp(safe[Rf_eval](expr, env));
}

namespace internal {

template<typename... Args>
inline r_sexp make_pairlist(Args... args) {
  constexpr int n = sizeof...(args);

  if constexpr (n == 0){
    return r_sexp(Rf_allocList(0));
  } else {
    r_sexp out = r_sexp(safe[Rf_allocList](n));

    SEXP current = out;

    (([&]() {
      if constexpr (NamedArg<Args>) {
        SETCAR(current, as<r_sexp>(args.value));
        SET_TAG(current, r_sym(args.name));
      } else {
        SETCAR(current, as<r_sexp>(args));
      }
      current = CDR(current);
    }()), ...);

    return out;
  }
}


template<typename... Args>
inline r_sexp make_call(const r_sexp& fn, Args&&... args) { 
  if (!(Rf_isFunction(fn))){
    abort("`fn` must be a function");
  }
  r_sexp pairlist = make_pairlist(std::forward<Args>(args)...);
  return r_sexp(Rf_lcons(fn, pairlist));
}

template<typename... Args>
inline r_sexp make_call(const r_sym& fn, Args&&... args) {
  r_sexp pairlist = make_pairlist(std::forward<Args>(args)...);
  return r_sexp(Rf_lcons(fn, pairlist));
}

template<typename... Args>
inline r_sexp make_call(const r_str& fn, Args&&... args) {
  return make_call(r_sym(fn), std::forward<Args>(args)...);
}


}

}

#endif
