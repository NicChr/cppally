#ifndef CPP20_R_FNS_H
#define CPP20_R_FNS_H

#include <cpp20/r_setup.h>
#include <cpp20/r_sexp.h>
#include <cpp20/r_types.h>
#include <cpp20/r_env.h>
#include <cpp20/r_symbols.h>
#include <cpp20/r_exprs.h>

namespace cpp20 {

namespace fn {
// Return R function from a specified package
inline r_sexp find_pkg_fun(const char *name, const char *pkg, bool all_fns){

  r_sexp expr = r_null;

  if (all_fns){
    expr = internal::make_call(symbol::triple_colon_sym, r_sym(pkg), r_sym(name));
  } else {
    expr = internal::make_call(symbol::double_colon_sym, r_sym(pkg), r_sym(name));
  }
  return eval(expr, env::base_env);
}

template<typename... Args>
inline r_sexp eval_fn(const r_sexp& r_fn, const r_sexp& envir, Args... args){
  // Expression
  r_sexp expr = internal::make_call(r_fn, args...);
  // Evaluate expression
  return eval(expr, envir);
}

template<typename... Args>
inline r_sexp eval_fn(const r_sym& r_fn, const r_sexp& envir, Args... args){
  // Expression
  r_sexp expr = internal::make_call(r_fn, args...);
  // Evaluate expression
  return eval(expr, envir);
}

template<typename... Args>
inline r_sexp eval_fn(const r_str& r_fn, const r_sexp& envir, Args... args){
  return eval_fn(r_sym(r_fn), envir, std::forward<Args>(args)...);
}


}

}

#endif
