#ifndef CPPALLY_R_NAMED_ARG_H
#define CPPALLY_R_NAMED_ARG_H

#include <cppally/r_concepts.h>

namespace cppally {

namespace internal {

// named_arg<V>
// Carries name + value with full type info. Created by arg::operator=
template <typename V>
struct named_arg {
  const char* name;
  V           value;
};

// Type trait — detects any named_arg<V> specialisation
template <typename T>          struct is_named_arg_t              : std::false_type {};
template <typename V>          struct is_named_arg_t<named_arg<V>>: std::true_type  {};
template <typename T>
inline constexpr bool is_named_arg = is_named_arg_t<std::remove_cvref_t<T>>::value;

}

template <typename T>
concept NamedArg = internal::is_named_arg<T>;

// ── arg ──────────────────────────────────────────────────────────
struct arg {
  const char* name;

  explicit constexpr arg(const char* n) : name(n) {}

  template <typename V>
  [[nodiscard]] constexpr internal::named_arg<std::remove_cvref_t<V>>
  operator=(V&& v) const {
    return {name, std::forward<V>(v)};
  }
};

}

#endif
