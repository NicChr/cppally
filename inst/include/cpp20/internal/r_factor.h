#ifndef CPP20_R_FACTOR_H
#define CPP20_R_FACTOR_H

#include <cpp20/internal/r_vec.h>
#include <cpp20/internal/r_stats.h>
#include <cpp20/internal/r_attrs.h>

namespace cpp20 {

struct r_factors { 

  public: 

  r_vec<r_int> value;

  r_vec<r_str_view> levels() const {
    return r_vec<r_str_view>(attr::get_attr(value, symbol::levels_sym));
  }

  private: 
  
  template <RStringType T>
  void validate_factor(const r_vec<r_int>& codes, const r_vec<T>& levels){
    r_int max_code = max(codes, true);

    if ((levels.length() < max_code).is_true()){
      abort("Invalid factor levels");
    }
  }

  void validate_factor(SEXP x){
    if (TYPEOF(x) != INTSXP){
      abort("SEXP must have integer storage to be constructed as a factor");
    }
    if (!Rf_inherits(x, "factor")){
      abort("SEXP must be of class 'factor' to be constructed as a factor");
    }
    SEXP levels = Rf_getAttrib(x, symbol::levels_sym);
    if (TYPEOF(levels) != STRSXP){
      abort("SEXP must have valid levels attribute to be constructed as a factor");
    }
    r_vec<r_str_view> levels2 = r_vec<r_str_view>(levels, internal::view_tag{});
    r_vec<r_int> codes = r_vec<r_int>(x, internal::view_tag{});
    validate_factor(codes, levels2);
  }

  // Internal direct constructor
  template <RStringType T>
  explicit r_factors(r_vec<r_int>&& codes, const r_vec<T>& levels, 
    bool check_valid_levels = true) : value(std::move(codes)){
      validate_factor(value, levels);
      init_factor(levels, false); 
    }

  public: 

  template <RStringType T>
  void set_levels(const r_vec<T>& levels, bool check_valid_levels = true) {
    if (check_valid_levels){
      validate_factor(value, levels);
    }
    attr::set_attr(value, symbol::levels_sym, levels);
  }

  private:

  template <RStringType T>
  void init_factor(const r_vec<T>& levels, bool check_valid_levels = true) {
      // Set class
      attr::set_attr(value, symbol::class_sym, r_vec<r_str_view>(1, "factor"));
      // Set levels
      set_levels(levels, check_valid_levels);
  }

  public: 

  // Constructors
  r_factors() : value() {
    init_factor(r_vec<r_str_view>(), false);
  }

  explicit r_factors(SEXP x) : value(x) {
    if (!value.is_null()){
      validate_factor(value); 
    }
  }

  explicit r_factors(SEXP x, internal::view_tag) : value(x, internal::view_tag{}) {
    if (!value.is_null()){
      validate_factor(value); 
    }
  }

  explicit r_factors(r_size_t n): value(n, na<r_int>()){
    init_factor(r_vec<r_str_view>(), false);
  }

  template <RVal T>
  explicit r_factors(const r_vec<T>& x, const r_vec<T>& levels);
  
  template <RVal T>
  explicit r_factors(const r_vec<T>& x);

  // Implicit coercion to r_vec<r_int> 
  constexpr operator r_vec<r_int>&() noexcept { return value; }
  constexpr operator const r_vec<r_int>&() const noexcept { return value; }

  constexpr operator SEXP() const noexcept { return static_cast<SEXP>(value); }

  r_vec<r_str_view> as_character() const {
    return levels().subset(value);
  }

};

}

#endif
