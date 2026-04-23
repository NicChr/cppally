#ifndef CPPALLY_R_FACTOR_H
#define CPPALLY_R_FACTOR_H

#include <cppally/r_limits.h>
#include <cppally/r_vec.h>
#include <cppally/r_attrs.h>

namespace cppally {

struct r_factors {

  public:

  r_vec<r_int> value;

  r_vec<r_str_view> levels() const {
    return r_vec<r_str_view>(attr::get_attr(value, symbol::levels_sym));
  }

  r_vec<r_int> codes() const {
    r_size_t n = value.length();
    r_vec<r_int> out(n);
    OMP_SIMD
    for (r_size_t i = 0; i < n; ++i){
      out.set(i, value.get(i));
    }
    return out;
  }

  private:

  template <RStringType T>
  void validate_levels(const r_vec<r_int>& codes, const r_vec<T>& levels){

    r_size_t n = codes.length();
    // Max code
    int max_code = unwrap(r_limits<r_int>::min());

    const int *p_codes = codes.data();

    OMP_SIMD_REDUCTION1(max:max_code)
    for (r_size_t i = 0; i < n; ++i){
        // No need to ignore NA for max() because NA is defined as lowest representable value
        max_code = std::max(max_code, p_codes[i]);
    }

    // If max is still the same value as when initialised, this either means the vector was full of NAs, or the max really is max int
    // Either way, we check in this rare case
    if (max_code == unwrap(r_limits<r_int>::min()) && codes.all_na()){
        max_code = unwrap(na<r_int>());
    }

    if (levels.length() < max_code){
      abort("Invalid factor levels");
    }
  }

  void validate_factor(SEXP x, bool check_valid_levels = true){
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
    if (check_valid_levels) validate_levels(codes, levels2);
  }

  public:

  template <RStringType T>
  void set_levels(const r_vec<T>& levels, bool check_valid_levels = true) {
    if (check_valid_levels){
      validate_levels(value, levels);
    }
    attr::set_attr(value, symbol::levels_sym, levels);
  }

  private:

  template <RStringType T>
  void init_factor(const r_vec<T>& levels, bool check_valid_levels = true) {
      // Set class
      attr::set_attr(value, symbol::class_sym, r_vec<r_str_view>(1, r_str_view(cached_str<"factor">())));
      // Set levels
      set_levels(levels, check_valid_levels);
  }

  // Internal direct constructor
  template <RStringType T>
  explicit r_factors(r_vec<r_int>&& codes, const r_vec<T>& levels,
    bool check_valid_levels = true) : value(std::move(codes)){
      init_factor(levels, check_valid_levels);
    }


    // For methods that just return a non-factor (like length())
    #define FORWARD_METHOD(NAME)                               \
        template <typename... Args>                            \
        decltype(auto) NAME(Args&&... args) const {            \
            return value.NAME(std::forward<Args>(args)...);    \
        }

    // For methods that return a factor
    #define FORWARD_FACTOR_METHOD(NAME)                                     \
        template <typename... Args>                                         \
        r_factors NAME(Args&&... args) const {                              \
            /* Call the method on the underlying r_vec<r_int> */            \
            auto new_vec = value.NAME(std::forward<Args>(args)...);         \
            /* Wrap it in a new r_factors and pass our current levels */    \
            return r_factors(std::move(new_vec), this->levels(), false);    \
        }

  public:

  // Constructors
  r_factors() : value() {
    init_factor(r_vec<r_str_view>(), false);
  }

  explicit r_factors(SEXP x, bool check_valid_levels = true) : value(x) {
    if (!value.is_null()){
      validate_factor(value, check_valid_levels);
    }
  }

  explicit r_factors(SEXP x, internal::view_tag, bool check_valid_levels = true) : value(x, internal::view_tag{}) {
    if (!value.is_null()){
      validate_factor(value, check_valid_levels);
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

  operator SEXP() const noexcept { return static_cast<SEXP>(value); }

  // Inherit standard methods from r_vec<>

  FORWARD_METHOD(length)
  FORWARD_METHOD(is_null)
  FORWARD_METHOD(size)
  FORWARD_METHOD(data)
  FORWARD_METHOD(begin)
  FORWARD_METHOD(end)
  FORWARD_METHOD(address)
  FORWARD_METHOD(get)
  FORWARD_METHOD(view)
  FORWARD_METHOD(set)
  FORWARD_METHOD(names)
  FORWARD_METHOD(set_names)
  // FORWARD_METHOD(is_na)
  FORWARD_METHOD(na_count)
  FORWARD_METHOD(any_na)
  FORWARD_METHOD(all_na)
  FORWARD_METHOD(count)
  FORWARD_METHOD(fill)
  FORWARD_METHOD(replace)
  FORWARD_METHOD(find)

  // Methods that return factors
  FORWARD_FACTOR_METHOD(subset)
  FORWARD_FACTOR_METHOD(rep_len)
  FORWARD_FACTOR_METHOD(resize)

  // Undefine the macros so they don't leak out of the struct
  #undef FORWARD_METHOD
  #undef FORWARD_FACTOR_METHOD

};

}

#endif
