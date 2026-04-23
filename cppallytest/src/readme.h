#pragma once

#include <cppally.hpp>
using namespace cppally;

// What type is deduced by dispatch?
template <typename T>
[[cpp::register]]
T scalar_init(T ptype){
	return T();
}

[[cpp::register]]
r_int64 as_int64(r_int x){
	return as<r_int64>(x);
}

[[cpp::register]]
r_size_t as_r_size_t(r_int x){
	return as<r_size_t>(x);
}

template <RVector T>
[[cpp::register]]
int cpp_length(T vec){
	return vec.length();
}

[[cpp::register]]
r_dbl add_half(r_dbl x){
  return x + 0.5;
}

[[cpp::register]]
r_str symbol_to_string(r_sym x){
	return as<r_str>(x);
}

[[cpp::register]]
r_factors new_factor(r_vec<r_str> x){
	return r_factors(x);
}

static_assert(!RVector<r_factors>);

[[cpp::register]]
r_vec<r_int> factor_codes(r_factors x){
	return x.codes();
}

[[cpp::register]]
r_vec<r_int> cpp_lengths(const r_vec<r_sexp>& x){
  r_size_t n = x.length();
  r_vec<r_int> out(n); // Initialise lengths vector
  
    for (r_size_t i = 0; i < n; ++i){
       visit_vector(x.view(i), [&](const auto& vec) {
         out.set(i, as<r_int>(vec.length()));
    });
      
    }

  return out;
}

[[cpp::register]]
r_vec<r_int> cpp_lengths2(const r_vec<r_sexp>& x){
    r_size_t n = x.length();
    r_vec<r_int> out(n); // Initialise lengths vector
    
      for (r_size_t i = 0; i < n; ++i){
         visit_sexp(x.view(i), [&](const auto& vec) {
         using vec_t = decltype(vec);
         
         if constexpr (!RVector<vec_t>){
             abort("Input must be an RVector");
         } else {
             out.set(i, as<r_int>(vec.length()));
         }
      });
        
      }
    return out;
}

// Coerces NA correctly
[[cpp::register]]
r_int double_to_int(r_dbl x){
  return as<r_int>(x);
}

[[cpp::register]]
r_vec<r_int> to_int_vec(r_vec<r_dbl> x){
  return as<r_vec<r_int>>(x);
}

[[cpp::register]]
r_vec<r_sexp> coercions(){
  r_dbl a(4.2);
  r_vec<r_dbl> b = make_vec<r_dbl>(2.5); // Vector containing 2.5
  
  return make_vec<r_sexp>(
    as<r_vec<r_int>>(a),
    as<r_int>(a),
    as<r_int>(b),
    as<r_dbl>(b)
  );
}

[[cpp::register]]
int cpp_na_count(r_vec<r_str> x){
  r_size_t n = x.length();

  int na_count = 0;

  for (r_size_t i = 0; i < n; ++i){
    r_str str = x.get(i); // `r_str` protects the underlying CHARSXP
    na_count += is_na(str);
  }
  return na_count;
}

[[cpp::register]]
int C_na_count(SEXP x){
  r_size_t n = Rf_xlength(x);

  int na_count = 0;

  const SEXP *p_x = STRING_PTR_RO(x);
  for (r_size_t i = 0; i < n; ++i){
    SEXP str = p_x[i]; // No protection so no extra overhead
    na_count += str == NA_STRING;
  }
  return na_count;
}

[[cpp::register]]
int cpp_fast_na_count(r_vec<r_str_view> x){
  r_size_t n = x.length();

  int na_count = 0;

  for (r_size_t i = 0; i < n; ++i){
    r_str_view str = x.get(i); // `r_str_view` does not protect underlying CHARSXP - make sure it doesn't outlive the object it is pointing to
    na_count += is_na(str);
  }
  return na_count;
}

[[cpp::register]]
int cpp_fast_na_count_v2(r_vec<r_str> x){
  r_size_t n = x.length();

  int na_count = 0;

  for (r_size_t i = 0; i < n; ++i){
    na_count += is_na(x.view(i)); // view() should always be safe as long as you don't assign the result to a variable or return the result
  }
  return na_count;
}
