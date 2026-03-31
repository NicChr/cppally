#ifndef CPP20_R_COERCE_H
#define CPP20_R_COERCE_H

#include <cpp20/r_vec.h>
#include <cpp20/sugar/r_factor.h>
#include <cpp20/r_visit.h>
#include <cpp20/sugar/r_match.h>
#include <cpp20/sugar/r_unique.h>
#include <cpp20/r_sexp_types.h>


// TO-DO: simplify the if statements below

namespace cpp20 {

template <RVal T>
r_factors::r_factors(const r_vec<T>& x, const r_vec<T>& levels) : value(match(x, levels)){
  r_vec<r_str_view> str_levels;
  if constexpr (RStringType<T>) {
      str_levels = r_vec<r_str_view>(levels);
  } else {
      r_size_t n = levels.length();
      str_levels = r_vec<r_str_view>(n);
      for (r_size_t i = 0; i < n; ++i) {
          str_levels.set(i, internal::as_r<r_str_view>(levels.view(i)));
      }
  }
  init_factor(str_levels, false);
}

template <RVal T>
r_factors::r_factors(const r_vec<T>& x) : r_factors(x, unique(x)) {}

// Powerful and flexible coercion function that can handle many types and convert to R-specific C++ types and R vectors
template <typename to_t, typename from_t>
inline to_t as(const from_t& x) {
  if constexpr (is<from_t, to_t>){
    return x;
  } else if constexpr (is<to_t, SEXP>){
    // Special case for SEXP
    // While it's not an RVal or a type that is generally explicitly supported in cpp20, it's needed for
    // registering C++ functions in R
    // So we want to avoid going through r_sexp and its protection management if we can
    if constexpr (RObject<from_t>){ // Is implicitly convertible to SEXP
      return static_cast<SEXP>(x);
    } else {
      // If it isn't implicitly convertible to SEXP, then rely on as<r_sexp> conversion
      return static_cast<SEXP>(as<r_sexp>(x));
    }
  } else if constexpr (RVector<from_t> && is<to_t, r_sexp>){
    return x.sexp;
  } else if constexpr (RFactor<to_t>){
    return r_factors(x);
  } else if constexpr (RFactor<from_t> && RVector<to_t>){
    if constexpr (RStringType<typename to_t::data_type>){
      return x.as_character();
    } else {
      return as<to_t>(x.value);
    }
  } else if constexpr (RVector<to_t> && is_sexp<from_t>){
    return visit_vector(x, [](const auto& xvec) -> to_t {
      // This will trigger the branch that checks that both are RVector
      return as<to_t>(xvec);
    });
  } else if constexpr (is_sexp<to_t> && is_sexp<from_t>){
    if constexpr (is<to_t, r_sexp>){
      // SEXP -> r_sexp
      return r_sexp(x);
    } else {
      // r_sexp -> SEXP
      return static_cast<SEXP>(x);
    }
  } else if constexpr (RVal<to_t> && is_sexp<from_t>){

      // Composite scalars are the opposite of atomic scalars which means only lists can hold them
      // e.g. symbols, NULL, etc
      
      if constexpr (RCompositeScalar<to_t>){
        if (internal::CPP20_TYPEOF(x) == internal::r_typeof<to_t>){
        return to_t(x);
      }
    }
    
    return visit_vector(x, [](const auto& xvec) -> to_t {
      return as<to_t>(xvec);
    });
  } else if constexpr (RVal<to_t> && RVector<from_t>){
    if (x.length() != 1){
      abort("Vector must be length-1 to be coerced to requested scalar type");
    }
    return as<to_t>(x.get(0));
  } else if constexpr (RVector<to_t> && RVal<from_t>){
    using data_t = typename to_t::data_type;
    return r_vec<data_t>(1, internal::as_r<data_t>(x));
  } else if constexpr (RVector<to_t> && RVector<from_t>){

    using to_data_t = typename to_t::data_type;
    using from_data_t = typename from_t::data_type;

    // Special case: If converting to character vector, we can safely bypass overhead of using r_str by using r_str_view
    // The vector already is protecting the elements
    if constexpr (is<to_data_t, r_str>){
      return to_t(as<r_vec<r_str_view>>(x));
    }

    r_size_t n = x.length();
    auto out = to_t(n);
    // Lists sometimes can't be converted to atomic vectors so we can't run the coercion under an SIMD clause
    if constexpr (internal::RPtrWritableType<to_data_t> && internal::RPtrWritableType<from_data_t>){
      OMP_SIMD
      for (r_size_t i = 0; i < n; ++i){
        out.set(i, as<to_data_t>(x.view(i)));
      }
    } else {
      for (r_size_t i = 0; i < n; ++i){
        out.set(i, as<to_data_t>(x.view(i)));
      }
    }
    return out;
  } else if constexpr (RVal<to_t> && !RVector<from_t>) {
    return internal::as_r<to_t>(x);
    // If input is not an R type or an R vector type
  } else if constexpr (!RVal<from_t> && !RVector<from_t>){
    return as<to_t>(as_r_val(x));
  } else if constexpr (CastableToRVal<to_t>){
    return static_cast<to_t>(as<as_r_val_t<to_t>>(x));
  } else {
    static_assert(always_false<to_t>, "Unsupported type for `as`");
  }
}

// Convert any obj to an r_vec<>
template <typename T>
inline auto as_vector(const T& x){
  if constexpr (RVector<T>){
    return x;
  } else if constexpr (is_sexp<T>){
    static_assert(always_false<T>, "Can't convert `SEXP/r_sexp` to `r_vec<>`, please use `as<>` to convert");
    return T();
  } else {
    auto rt_val = as_r_val(x);
    return r_vec<decltype(rt_val)>(1, rt_val);
  }
}

}

#endif
