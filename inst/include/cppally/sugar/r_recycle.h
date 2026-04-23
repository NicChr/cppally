#ifndef CPPALLY_R_RECYCLE_H
#define CPPALLY_R_RECYCLE_H

#include <cppally/r_visit.h>
#include <cppally/r_list_helpers.h>
#include <cppally/sugar/r_make_vec.h>

namespace cppally {

// Variadic recycle function
template <typename... Args>
inline r_vec<r_sexp> recycle(Args&&... args) {
  r_vec<r_sexp> out = make_vec<r_sexp>(std::forward<Args>(args)...);
  internal::recycle_impl(out, internal::recycle_size(out));
  return out;
}

}

#endif
