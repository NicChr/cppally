#ifndef CPP20_R_MAKE_VEC_H
#define CPP20_R_MAKE_VEC_H

#include <cpp20/r_vec.h>
#include <cpp20/r_attrs.h>

namespace cpp20 {

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


// ── as_r<T> for named_arg ──────────────────────────────────────────
template <RVal T, typename V>
inline T internal::as_r(const internal::named_arg<V>& a) {
  return internal::as_r<T>(a.value);
}


// ── make_vec ─────────────────────────────────────────────────────
template <RVal T, typename... Args>
inline r_vec<T> make_vec(Args... args) {

  constexpr int n = sizeof...(args);

  if constexpr (n == 0){
    return r_vec<T>(0);
  } else {

    auto out = r_vec<T>(n);

    constexpr bool any_named = (NamedArg<Args> || ...);

    auto nms = any_named ? r_vec<r_str_view>(n) : r_vec<r_str_view>(r_null);

    int i = 0;
    (([&]() {
      if constexpr (NamedArg<Args>) {
        out.set(i, internal::as_r<T>(args));
        nms.set(i, internal::as_r<r_str_view>(args.name));
      } else {
        out.set(i, internal::as_r<T>(args));
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
