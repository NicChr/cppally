#ifndef CPPALLY_R_FUNCTION_H
#define CPPALLY_R_FUNCTION_H

#include <cppally/r_setup.h>
#include <cppally/r_sexp/r_sexp.h>
#include <cppally/scalar/r_lgl.h>
#include <cppally/r_sym.h>
#include <cppally/r_env.h>
#include <cppally/r_named_arg.h>
#include <initializer_list>

namespace cppally {

namespace internal {

inline r_sexp make_pairlist(std::initializer_list<r_sexp> args){
  int n = args.size();
  r_sexp out = r_sexp(safe[Rf_allocList](n));

  SEXP current = out;
  for (const r_sexp& elem : args) {
    SETCAR(current, elem);
    current = CDR(current);
  }

  return out;
}

inline r_sexp empty_fn(){
  static r_sexp& empty_clo = *new r_sexp(safe[R_mkClosure](R_NilValue, R_NilValue, env::empty_env));
  return empty_clo;
}

}

template <string_literal pkg>
inline r_sexp pkg_env() {
  static r_sexp& ns = *new r_sexp(
    internal::unwind_protect([] { return R_FindNamespace(Rf_ScalarString(Rf_mkCharCE(pkg.data, CE_UTF8))); })
  );
  return ns;
}

inline r_sexp eval(const r_sexp& expr, const r_sexp& env){
  return r_sexp(safe[Rf_eval](expr, env));
}

// Wrap a callable R function.
// r_function allows you to find an existing function inside a specified namespace. 
// e.g. `r_function("sum")` will look for a function named 'sum' in the global environment (by default). 
// `r_function("sum", pkg_env<"base">())` will look for a function named sum specifically in the base R package.
// An efficient and clean way to call functions is via `cached_sym`, e.g. `r_function(cached_sym<"foo">())` as the symbol
// `foo` becomes cached by C++.
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
    r_sexp operator()(std::initializer_list<r_sexp> args) const {
      return call_impl(internal::make_pairlist(args));
    }
    
    template <typename... Args>
    r_sexp operator()(Args&&... args) const; // Defined in r_vector.h
    

    private:

    static void check_is_function(SEXP x) {
      if (!Rf_isFunction(x)) [[unlikely]] {
        abort("Bad construction from R type %s to C++ type r_function", Rf_type2char(TYPEOF(x)));
      }
    }

    r_sexp call_impl(const r_sexp& args) const {
      return r_sexp(
        internal::unwind_protect([&] { return Rf_eval(Rf_lcons(*this, args), env::global_env); })
      );
    }

};

inline r_lgl operator==(const r_function& lhs, const r_function& rhs) noexcept {
  return r_lgl{unwrap(lhs) == unwrap(rhs)};
}
inline r_lgl operator!=(const r_function& lhs, const r_function& rhs) noexcept {
  return r_lgl{unwrap(lhs) != unwrap(rhs)};
}

}

#endif
