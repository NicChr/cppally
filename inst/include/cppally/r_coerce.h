#ifndef CPPALLY_R_COERCE_H
#define CPPALLY_R_COERCE_H

#include <cppally/r_coerce_impl.h>
#include <cppally/r_vec.h>
#include <cppally/r_visit.h>
#include <cppally/r_sexp_types.h>


// TO-DO: simplify the if statements below

namespace cppally {

namespace internal {

// CHARSXP is always converted to STRSXP here, see `r_types.h` for info
// to sexp is fairly simple but coercion from a sexp is more complicated
// implementation for that is found in r_coerce.h
template <typename T>
inline r_sexp as_sexp(T const& x){
  if constexpr (RVector<T>){
    return x.sexp;
  } else if constexpr (RObject<T>){
    return r_sexp(static_cast<SEXP>(x));
  } else if constexpr (RVal<T>){
    return r_sexp(new_scalar_vec(x));
  } else if constexpr (CastableToRVal<T>) {
    return r_sexp(new_scalar_vec(as_r_val(x)));
  } else {
    return new_scalar_vec(as_r_val(x)); 
  }
}
template<>
inline r_sexp as_sexp<r_sym>(r_sym const& x){
  return r_sexp(x.value, internal::view_tag{});
}

}

// Powerful and flexible coercion function that can handle many types and convert to R-specific C++ types and R vectors
// Note: forwards declarations exist in r_coerce_impl.h
template <typename T, typename U>
requires is<T, U>
inline std::remove_cvref_t<T> as(const U& x) {
  return x;
}

template <typename T, typename U>
inline std::remove_cvref_t<T> as(const U& x) {

  using to_t = std::remove_cvref_t<T>;
  using from_t = std::remove_cvref_t<U>;
  
  if constexpr (is<to_t, SEXP>){ // To C-level plain SEXP
    // Special case for SEXP
    // While it's not an RVal or a type that is generally explicitly supported in cppally, it's needed for
    // registering C++ functions in R
    // So we want to avoid going through r_sexp and its protection management if we can
    if constexpr (RObject<from_t>){ // Is implicitly convertible to SEXP
      return static_cast<SEXP>(x);
    } else {
      // If it isn't implicitly convertible to SEXP, then rely on as<r_sexp> conversion
      return static_cast<SEXP>(as<r_sexp>(x));
    }
  } else if constexpr (is<to_t, r_sexp>){ // To r_sexp (to SEXP is handled above)
    return internal::as_sexp(x);

    // TO-DO: Check whether this should go after the branch below
  } else if constexpr (RFactor<to_t>){ // To factor
    return r_factors(x);

  } else if constexpr (is_sexp<from_t> && !is_sexp<to_t>){ // From SEXP to non-SEXP, use visit_sexp to disambiguate the type
    return view_sexp(x, [](const auto& xvec) -> to_t {
      if constexpr (is<decltype(xvec), r_sexp>){ // Couldn't disambiguate if r_sexp is the return type
        abort("Don't know how to visit this r_sexp");
      } else {
        return as<to_t>(xvec);
      }
    });
  } else if constexpr (RDataFrame<to_t>){
    return r_df(x);
  } else if constexpr (is<r_sym, to_t>){ // To symbol
    if constexpr (is<from_t, const char*> || RStringType<from_t>){
      return r_sym(x);
    } else {
      return r_sym(as<r_str_view>(x));
    }
    
  } else if constexpr (is<r_sym, from_t>){ // From symbol - just convert to r_str_view first and then coerce
    return as<to_t>(r_str_view(PRINTNAME(static_cast<SEXP>(x))));

  } else if constexpr (RFactor<from_t>){ // From factor to vector

    using levels_t = std::conditional_t<RVector<to_t>, to_t, r_vec<r_str_view>>;
    levels_t coerced_levels = as<levels_t>(x.levels());
    r_size_t n_levels = coerced_levels.length();
    r_size_t n = x.length();
    levels_t out(n);
  
    unsigned int na_val = unwrap(na<r_int>());
    unsigned int j;

    using data_t = typename levels_t::data_type;
  
    for (r_size_t i = 0; i < n; ++i){
      j = unwrap(x.value.get(i));
      if (j == na_val){
        out.set(i, na<data_t>());  
      } else if (j > na_val) [[unlikely]] {
        abort("Negative factor code detected in `r_factors.as_character()`");
      } else if (j == 0U) [[unlikely]] {
        abort("Invalid factor code of value 0 detected in `r_factors.as_character()`");
      } else if (static_cast<r_size_t>(j) > n_levels) [[unlikely]] {
        abort("Invalid factor code of value %lld detected", static_cast<long long int>(j));
      } else {
        out.set(i, coerced_levels.view(static_cast<r_size_t>(j) - r_size_t(1)));
      }
    }

    if constexpr (RVector<to_t>){
      return out;
    } else {
      return as<to_t>(out);
    }

  } else if constexpr (RDataFrame<from_t>){ // from data frame
    r_vec<r_sexp> lst(safe[Rf_shallow_duplicate](x.value));
    attr::clear_attrs(lst);
    // Keep names
    attr::set_old_names(lst, attr::get_old_names(x));
    return as<to_t>(lst);
  } else if constexpr (RScalar<to_t> && RVector<from_t>){ // From vector to scalar
    if (x.length() != 1){
      abort("Vector must be length-1 to be coerced to requested scalar type");
    }
    return as<to_t>(x.get(0));

  } else if constexpr (RVector<to_t> && RVal<from_t>){ // From scalar to vector
    using data_t = typename to_t::data_type;
    return r_vec<data_t>(1, as<data_t>(x));

  } else if constexpr (RVector<to_t> && RVector<from_t>){ // From one vector to another
    using to_data_t = typename to_t::data_type;
    using from_data_t = typename from_t::data_type;

    // Special case: from r_vec<r_str> to r_vec<r_str_view> (or vice versa)
    // No need to actually loop here as they both are exactly the same character vector
    // Only thing that changes is the ownership semantics associated with the element type
    if constexpr (RStringType<to_data_t> && RStringType<from_data_t>){
      return to_t(static_cast<SEXP>(x));
    }

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
  } else if constexpr (RScalar<to_t> && RScalar<from_t>) {
    return internal::as_scalar_impl<to_t>(x);
    // From C++ scalar that is RVal constructible
  } else if constexpr (CastableToRScalar<from_t> && RScalar<to_t>){
    return as<to_t>(as_r_scalar(x));
    // To C++ scalar that is RVal constructible
  } else if constexpr (CastableToRScalar<to_t>){
    return static_cast<to_t>(as<as_r_scalar_t<to_t>>(x));
  } else {
    static_assert(always_false<to_t>, "Unsupported type for `as`");
  }
}

// template <RScalar T, RScalar U>
// requires (!is<T, U>)
// inline std::remove_cvref_t<T> as(const U& x) {
//   return internal::as_scalar_impl<T>(x);
// }


// // as_scalar_impl<> can handle -> r_sexp 
// template <typename T, typename U>
// requires (is<T, r_sexp> && !is<U, r_sexp>)
// inline r_sexp as(const U& x) {
//   return internal::as_scalar_impl<r_sexp>(x);
// }

// template <RVector T, RVector U>
// requires (!is<T, U>)
// inline std::remove_cvref_t<T> as(const U& x) {
//   using to_data_t = typename T::data_type;
//   using from_data_t = typename U::data_type;
  
//   // Special case: from r_vec<r_str> to r_vec<r_str_view> (or vice versa)
//   // No need to actually loop here as they both are exactly the same character vector
//   // Only thing that changes is the ownership semantics associated with the element type
//   if constexpr (RStringType<to_data_t> && RStringType<from_data_t>){
//     return T(static_cast<SEXP>(x));
//   }

//   // Special case: If converting to character vector, we can safely bypass overhead of using r_str by using r_str_view
//   // The vector already is protecting the elements
//   if constexpr (is<to_data_t, r_str>){
//     return T(as<r_vec<r_str_view>>(x));
//   }
  
//   r_size_t n = x.length();
//   T out(n);
//   // Lists sometimes can't be converted to atomic vectors so we can't run the coercion under an SIMD clause
//   if constexpr (internal::RPtrWritableType<to_data_t> && internal::RPtrWritableType<from_data_t>){
//     OMP_SIMD
//     for (r_size_t i = 0; i < n; ++i){
//       out.set(i, as<to_data_t>(x.view(i)));
//     }
//   } else {
//     for (r_size_t i = 0; i < n; ++i){
//       out.set(i, as<to_data_t>(x.view(i)));
//     }
//   }
//   return out;
// }

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
