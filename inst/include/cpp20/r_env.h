#ifndef CPP20_R_ENV_H
#define CPP20_R_ENV_H

#include <cpp20/r_setup.h>
#include <cpp20/r_concepts.h>
#include <cpp20/r_types.h>

namespace cpp20 {

namespace env {
inline const r_sexp empty_env = r_sexp(R_EmptyEnv, internal::view_tag{});
inline const r_sexp base_env = r_sexp(R_BaseEnv, internal::view_tag{});
}

}

#endif
