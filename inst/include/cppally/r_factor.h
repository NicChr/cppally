#ifndef CPPALLY_R_FACTOR_H
#define CPPALLY_R_FACTOR_H

#include <cppally/r_limits.h>
#include <cppally/r_vec.h>
#include <cppally/r_hash_names.h>
#include <cppally/r_attrs.h>

namespace cppally {

namespace internal {
inline void share_levels_cache(r_factors&, const r_factors&);
}

struct r_factors {

  public:

  r_vec<r_int> value;

  private: 

  #ifdef CPPALLY_CHECK_FACTORS
  static constexpr bool chk_fct_lvls_opt = true;
  #else
  static constexpr bool chk_fct_lvls_opt = false;
  #endif

  // Shared cache for levels — see r_vec::cached_names for the design
  mutable std::shared_ptr<internal::names_map> cached_levels;

  friend void internal::share_levels_cache(r_factors&, const r_factors&);

  void ensure_levels_cached() const {
    if (!cached_levels) {
      cached_levels = internal::get_or_create_levels_cache(static_cast<SEXP>(value));
    }
    if (!cached_levels->names.has_value()) {
      r_vec<r_str_view> validated(Rf_getAttrib(value, symbol::levels_sym));
      cached_levels->names.emplace(static_cast<r_sexp>(validated));
    }
  }

  public:

  r_vec<r_str_view> levels() const {
    ensure_levels_cached();
    return r_vec<r_str_view>(*cached_levels->names);
  }

  r_vec<r_int> codes() const {
    r_vec<r_int> out(value.length());
    r_copy_n(out, value, 0, value.length());
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

  void validate_factor(bool check_valid_levels = chk_fct_lvls_opt){
    if (!attr::inherits1(value, "factor")) [[unlikely]] {
      abort("SEXP must be of class 'factor' to be constructed as a factor");
    }
    if (check_valid_levels){
      validate_levels(value, levels());
    }
  }

  public:

  template <RStringType T>
  void set_levels(const r_vec<T>& levels, bool check_valid_levels = chk_fct_lvls_opt) {
    if (check_valid_levels){
      validate_levels(value, levels);
    }
    safe[Rf_setAttrib](value, symbol::levels_sym, levels);
    if (!cached_levels) {
      cached_levels = internal::get_or_create_levels_cache(static_cast<SEXP>(value));
    }
    cached_levels->invalidate();
  }

  private:

  template <RStringType T>
  void init_factor(const r_vec<T>& levels, bool check_valid_levels = chk_fct_lvls_opt) {
      // Set class
      attr::set_attr(value, symbol::class_sym, r_vec<r_str_view>(1, r_str_view(cached_str<"factor">())));
      // Set levels
      set_levels(levels, check_valid_levels);
  }

  // Internal direct constructor
  template <RStringType T>
  explicit r_factors(r_vec<r_int>&& codes, const r_vec<T>& levels, bool check_valid_levels = chk_fct_lvls_opt) : value(std::move(codes)){
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

  explicit r_factors(SEXP x, bool check_valid_levels = chk_fct_lvls_opt) : value(x) {
    if (!value.is_null()){
      validate_factor(check_valid_levels);
    }
  }

  explicit r_factors(SEXP x, internal::view_tag, bool check_valid_levels = chk_fct_lvls_opt) : value(x, internal::view_tag{}) {
    if (!value.is_null()){
      validate_factor(check_valid_levels);
    }
  }

  explicit r_factors(r_size_t n): value(n, na<r_int>()){
    init_factor(r_vec<r_str_view>(), false);
  }

  template <RVal T>
  explicit r_factors(const r_vec<T>& x, const r_vec<T>& levels);

  template <RVal T>
  explicit r_factors(const r_vec<T>& x);

  operator SEXP() const noexcept { return static_cast<SEXP>(value); }

  // Inherit standard methods from r_vec<>

  FORWARD_METHOD(length)
  FORWARD_METHOD(is_null)
  FORWARD_METHOD(data)
  FORWARD_METHOD(begin)
  FORWARD_METHOD(end)
  FORWARD_METHOD(address)
  // FORWARD_METHOD(is_na)
  FORWARD_METHOD(na_count)
  FORWARD_METHOD(any_na)
  FORWARD_METHOD(all_na)

  // Methods that return factors
  FORWARD_FACTOR_METHOD(subset)
  FORWARD_FACTOR_METHOD(rep_len)
  FORWARD_FACTOR_METHOD(resize)

  // Undefine the macros so they don't leak out of the struct
  #undef FORWARD_METHOD
  #undef FORWARD_FACTOR_METHOD

  // Find factor code associated with factor string
  // Since levels are assumed to be unique, we find the first match
  template <RStringType U>
  r_int get_code(const U& val) const {
    ensure_levels_cached();
    return cached_levels->find(val, /*offset = */ 1);
  }

  r_int get_code(std::string_view val) const {
    return get_code(r_str(val.data()));
  }

  template <RStringType U>
  r_vec<r_int> get_codes(const r_vec<U>& vals) const {
    int n = vals.length();
    r_vec<r_int> out(n);
    for (int i = 0; i < n; ++i){
      out.set(i, get_code(vals.view(i)));
    }
    return out;
  }

  r_int get_code(r_size_t index) const {
    return value.get(index);
  }

  void set_code(r_size_t index, r_int val) {
    value.set(index, val);
  }

  r_str get(r_size_t index) const {
    r_int code = value.get(index);
    if (is_na(code)){
      return na<r_str>();
    }
    return r_str(levels().get(unwrap(code) - 1));
  }

  r_str_view view(r_size_t index) const {
    r_int code = value.get(index);
    if (is_na(code)){
      return na<r_str_view>();
    }
    return levels().view(unwrap(code) - 1);
  }

  template <RStringType U>
  void set(r_size_t index, const U& val) {
    value.set(index, get_code(val));
  }

  template <RStringType U>
  r_size_t count(const U& val) const {
    if (is_na(val)){
      return value.na_count();
    } else {
      r_int code = get_code(val);
      if (is_na(code)){
        return 0;
      } else {
        return value.count(code);
      }
    }
  }

  template <RStringType U>
  void fill(r_size_t start, r_size_t n, const U& val){
    return is_na(val) ? value.fill(start, n, na<r_int>()) : value.fill(start, n, get_code(val));
  }
  template <RStringType U>
  void fill(const U& val){
    fill(0, value.length(), val);
  }

  template <RStringType U>
  r_vec<r_int> find(const U& val, bool invert = false) const {
    if (is_na(val)){
      return value.find(na<r_int>(), invert);
    }
    r_int code = get_code(val);
    if (is_na(code)){
      if (invert){
        r_vec<r_int> out(value.length());
        out.iota();
        return out;
      } else {
        return r_vec<r_int>();
      }
    }
    return value.find(code, invert);
  }

  template <RStringType U>
  void replace(r_size_t start, r_size_t n, const U& old_val, const U& new_val){
    r_int old_code = get_code(old_val);
    r_int new_code = get_code(new_val);

    bool valid_old_code = is_na(old_val) == is_na(old_code);
    bool valid_new_code = is_na(new_val) == is_na(new_code);

    if (!valid_old_code || !valid_new_code){
      return;
    }

    for (r_size_t i = 0; i < n; ++i) {
      r_size_t idx = start + i;
      if (identical(value.get(idx), old_code)){
        value.set(idx, new_code);
      }
    }
  }
  
  template <RStringType U>
  void replace(const U& old_val, const U& new_val){
    replace(0, value.length(), old_val, new_val);
  }

  template <RStringType U>
  r_factors remove(const U& val) const {
    if (is_na(val)){
      r_vec<r_int> fct_codes = value.remove(na<r_int>());
      r_factors result(std::move(fct_codes), this->levels(), false);
      ensure_levels_cached();
      result.ensure_levels_cached();
      result.cached_levels->map = this->cached_levels->map;
      return result;
    }
    r_int code = get_code(val);
    if (is_na(code)){
      return *this;
    }
    r_vec<r_int> fct_codes = value.remove(code);
    r_factors result(std::move(fct_codes), this->levels(), false);
    ensure_levels_cached();
    result.ensure_levels_cached();
    result.cached_levels->map = this->cached_levels->map;
    return result;
  }

  template <RStringType U>
  void append_level(const U& level){
    r_int existing_code = get_code(level);
    // Level already exists
    if (!is_na(existing_code)){
      return;
    }

    // If new level is NA, existing factor NA values are replaced with corresponding code matching the newly added NA level
    if (is_na(level)){
      value.replace(na<r_int>(), r_int(static_cast<int>(levels().length() + 1)));
    }

    // Build new levels
    r_vec<r_str_view> new_levels(levels().length() + 1);
    r_copy_n(new_levels, levels(), 0, levels().length());
    new_levels.set(new_levels.length() - 1, level);

    // Preserve the lazy hash across set_levels() (which resets it)
    ensure_levels_cached();
    auto previous_map = std::move(cached_levels->map);

    set_levels(new_levels, false);

    if (previous_map){
      previous_map->set_names_ptr(new_levels.data());
      // previous_map->set_names_ptr(STRING_PTR_RO(static_cast<SEXP>(new_levels)));
      if (previous_map->insert(static_cast<int>(previous_map->size()))){
        ensure_levels_cached();
        cached_levels->names.emplace(static_cast<r_sexp>(new_levels));
        cached_levels->map = std::move(previous_map);
      }
    }
  }

};


namespace internal {

// Same shape as share_name_cache, for the levels cache on r_factors.
inline void share_levels_cache(r_factors& target, const r_factors& source) {
    if (!source.cached_levels) return;
    if (!source.cached_levels->names.has_value()) return;
    SEXP target_levels = Rf_protect(Rf_getAttrib(target, symbol::levels_sym));
    if (target_levels != static_cast<SEXP>(*source.cached_levels->names)){
        Rf_unprotect(1);
        return;
    }
    target.cached_levels = source.cached_levels;
    levels_cache_storage()[target] = target.cached_levels;
    Rf_unprotect(1);
}

}

}

#endif
