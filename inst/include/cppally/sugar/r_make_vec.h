#ifndef CPPALLY_R_MAKE_VEC_H
#define CPPALLY_R_MAKE_VEC_H

#include <cppally/r_setup.h>
#include <cppally/sugar/r_named_arg.h>
#include <cppally/r_vec.h>
#include <cppally/r_attrs.h>
#include <cppally/r_coerce.h>

namespace cppally {

// ── make_vec ─────────────────────────────────────────────────────
template <RVal T, typename... Args>
inline r_vec<T> make_vec(Args... args) {

  constexpr int n = sizeof...(args);

  if constexpr (n == 0){
    return r_vec<T>(0);
  } else {

    r_vec<T> out(n);

    constexpr bool any_named = (NamedArg<Args> || ...);

    auto nms = any_named ? r_vec<r_str_view>(n) : r_vec<r_str_view>(r_null);

    int i = 0;
    (([&]() {
      if constexpr (NamedArg<Args>) {
        out.set(i, as<T>(args.value));
        nms.set(i, as<r_str_view>(args.name));
      } else {
        out.set(i, as<T>(args));
      }
      ++i;
    }()), ...);

    attr::set_old_names(out.sexp, nms);
    return out;
  }
}


namespace attr {

template <typename... Args>
inline void modify_attrs(r_sexp x, Args&&... args) {
  auto attrs = make_vec<r_sexp>(std::forward<Args>(args)...);
  internal::modify_attrs_impl(x, attrs);
}

}

}

#endif
