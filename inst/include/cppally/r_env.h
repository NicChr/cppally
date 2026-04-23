#ifndef CPPALLY_R_ENV_H
#define CPPALLY_R_ENV_H

#include <cppally/r_setup.h>
#include <cppally/r_concepts.h>
#include <cppally/r_types.h>

namespace cppally {

namespace env {
inline const r_sexp empty_env = r_sexp(R_EmptyEnv, internal::view_tag{});
inline const r_sexp base_env = r_sexp(R_BaseEnv, internal::view_tag{});
}

}

#endif
