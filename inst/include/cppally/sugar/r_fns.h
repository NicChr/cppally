#ifndef CPPALLY_R_FNS_H
#define CPPALLY_R_FNS_H

#include <cppally/r_setup.h>
#include <cppally/r_sexp.h>
#include <cppally/r_env.h>
#include <cppally/r_sym.h>
#include <cppally/sugar/r_exprs.h>

namespace cppally {

namespace fn {

// Return R function from a specified package
inline r_sexp find_pkg_fun(r_sym name, r_sym pkg){
  r_sexp expr = internal::make_call(cached_sym<"::">(), pkg, name);
  return eval(expr, env::base_env);
}
inline r_sexp find_pkg_fun(const char *name, const char *pkg){
  return find_pkg_fun(r_sym(name), r_sym(pkg));
}

template<typename... Args>
inline r_sexp eval_fn(const r_sexp& r_fn, const r_sexp& envir, Args&&... args){
  // Expression
  r_sexp expr = internal::make_call(r_fn, std::forward<Args>(args)...);
  // Evaluate expression
  return eval(expr, envir);
}

template<typename... Args>
inline r_sexp eval_fn(const r_sym& r_fn, const r_sexp& envir, Args&&... args){
  // Expression
  r_sexp expr = internal::make_call(r_fn, std::forward<Args>(args)...);
  // Evaluate expression
  return eval(expr, envir);
}

template<typename... Args>
inline r_sexp eval_fn(const r_str& r_fn, const r_sexp& envir, Args&&... args){
  return eval_fn(r_sym(r_fn), envir, std::forward<Args>(args)...);
}


}

}

#endif
