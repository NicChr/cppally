#ifndef CPPALLY_R_FUNCTION_H
#define CPPALLY_R_FUNCTION_H

#include <cppally/r_setup.h>
#include <cppally/r_sexp.h>
#include <cppally/r_sym.h>
#include <cppally/r_env.h>
#include <cppally/r_named_arg.h>
#include <cppally/r_coerce_scalars.h>

namespace cppally {

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
        SETCAR(current, internal::as_list_element(args.value));
        SET_TAG(current, r_sym(args.name));
      } else {
        SETCAR(current, internal::as_list_element(args));
      }
      current = CDR(current);
    }()), ...);

    return out;
  }
}
inline r_sexp empty_fn(){
  static r_sexp empty_clo(safe[R_mkClosure](R_NilValue, R_NilValue, env::empty_env));
  return empty_clo;
}

}

template <internal::fixed_string pkg>
inline r_sexp pkg_env() {
  static r_sexp ns = r_sexp(
    internal::unwind_protect([] { return R_FindNamespace(Rf_ScalarString(Rf_mkCharCE(pkg.data, CE_UTF8))); })
  );
  return ns;
}

inline r_sexp eval(const r_sexp& expr, const r_sexp& env){
  return r_sexp(safe[Rf_eval](expr, env));
}

// Wraps a callable R object
struct r_function {

    r_sexp value;
    using value_type = r_sexp;
  
    // By default, construct a NULL returning empty fn
    r_function() : r_function(internal::empty_fn(), internal::no_checks_tag{}) {}

    explicit r_function(SEXP x) : value(x) {
      check_is_function(value);
    }
    explicit r_function(SEXP x, internal::view_tag) : value(x, internal::view_tag{}) {
      check_is_function(value);
    }
    explicit r_function(r_sexp x) : value(std::move(x)) {
      check_is_function(value);
    }
    explicit r_function(const r_sexp& x, internal::view_tag) : value(unwrap(x), internal::view_tag{}) {
      check_is_function(value);
    }

    // Unchecked constructors: skip function-type validation
    // For use where the SEXP type is already established (e.g. r_visit.h dispatchers)
    explicit r_function(r_sexp x, internal::no_checks_tag) : value(std::move(x)) {}
    explicit r_function(const r_sexp& x, internal::view_tag, internal::no_checks_tag) : value(unwrap(x), internal::view_tag{}) {}

    // Look a function up by symbol
    explicit r_function(const r_sym& name, const r_sexp& env = env::global_env) : value(safe[Rf_findFun](name, env)) {}
    // Look a function up by name (string)
    explicit r_function(const char* name, const r_sexp& env = env::global_env) : r_function(r_sym(name), env) {}
    // Look a function up by name (string)
    template <RStringType T>
    explicit r_function(const T& name, const r_sexp& env = env::global_env) : r_function(r_sym(name), env) {}
  
    operator SEXP() const noexcept { return value; }
    explicit operator r_sexp() const noexcept { return value; }
  
    // operator() to make r_function callable
    template <typename... Args>
    r_sexp operator()(Args&&... args) const {
      r_sexp fn_args = internal::make_pairlist(std::forward<Args>(args)...);

      return r_sexp(
        internal::unwind_protect([&] { return Rf_eval(Rf_lcons(*this, fn_args), env::global_env); })
      );
    }
  
    private:
    static void check_is_function(SEXP x) {
      if (!Rf_isFunction(x)) [[unlikely]] {
        abort("Bad construction from R type %s to C++ type r_function", Rf_type2char(TYPEOF(x)));
      }
    }
};

}
#endif
