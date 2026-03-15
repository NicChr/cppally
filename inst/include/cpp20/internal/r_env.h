#ifndef CPP20_R_ENV_H
#define CPP20_R_ENV_H

#include <cpp20/internal/r_setup.h>
#include <cpp20/internal/r_exprs.h>

namespace cpp20 {

namespace env {

inline const r_sexp empty_env = r_sexp(R_EmptyEnv, internal::view_tag{});
inline const r_sexp base_env = r_sexp(R_BaseEnv, internal::view_tag{});

inline r_sexp get(r_sym sym, const r_sexp& env, bool inherits = true){

  if (TYPEOF(env) != ENVSXP){
    abort("second argument to '%s' must be an environment", __func__);
  }

  r_sexp val = r_sexp(inherits ? Rf_findVar(sym, env) : Rf_findVarInFrame(env, sym));

  if (val == static_cast<SEXP>(symbol::missing_arg)){
    abort("arg `sym` cannot be missing");
  } else if (val == static_cast<SEXP>(symbol::unbound_value)){
    return r_null;
  } else if (TYPEOF(val) == PROMSXP){
    val = eval(val, env);
  }
  return val;
}
}

}

#endif
