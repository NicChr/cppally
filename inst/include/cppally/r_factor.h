#ifndef CPPALLY_R_FACTOR_H
#define CPPALLY_R_FACTOR_H

#include <cppally/r_limits.h>
#include <cppally/r_vec.h>
#include <cppally/r_vec_ops.h>
#include <cppally/r_hash_names.h>
#include <cppally/r_attrs.h>

namespace cppally {

namespace internal {
inline void share_levels_cache(r_factors&, const r_factors&);
}

struct r_factors {

  public:

  r_vec<r_int> value;
  using value_type = r_vec<r_int>;
  using data_type = r_str; // data_type is tied to return type of `get()`

  private: 

  #ifdef CPPALLY_CHECK_FACTORS
  static constexpr bool chk_fct_lvls_opt = true;
  #else
  static constexpr bool chk_fct_lvls_opt = false;
  #endif

  // For methods that just return a non-factor (like length())
  #define FORWARD_METHOD(NAME)                               \
      template <typename... Args>                            \
      decltype(auto) NAME(Args&&... args) const {            \
          return value.NAME(std::forward<Args>(args)...);    \
      }

  #define FORWARD_MUTATING_METHOD(NAME)                  \
  template <typename... Args>                            \
  decltype(auto) NAME(Args&&... args) {                  \
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


  // Shared cache for levels — see r_vec::cached_names for the design
  mutable std::shared_ptr<internal::names_map> cached_levels;
  // Counts get_code calls on this wrapper. First lookup is a linear scan over
  // levels; the hash is only built on the second so one-shot lookups pay no
  // build cost.
  mutable bool first_access = false;

  friend void internal::share_levels_cache(r_factors&, const r_factors&);

  void ensure_levels_cached() const {
    if (!cached_levels) {
      cached_levels = internal::levels_cache().get_or_create(static_cast<SEXP>(value));
    }
    if (!cached_levels->names.has_value()) {
      r_vec<r_str_view> validated(Rf_getAttrib(value, symbol::levels_sym));
      cached_levels->names.emplace(static_cast<r_sexp>(validated));
    }
  }

  public:

  // Inherit standard methods from r_vec<>

  FORWARD_METHOD(length)
  FORWARD_METHOD(is_null)
  FORWARD_METHOD(is_exclusive)
  FORWARD_MUTATING_METHOD(maybe_ensure_exclusive)
  FORWARD_METHOD(data)
  FORWARD_METHOD(address)
  FORWARD_METHOD(names)
  FORWARD_MUTATING_METHOD(set_names)
  FORWARD_METHOD(name_index)

  // Methods that return factors
  FORWARD_FACTOR_METHOD(subset)
  FORWARD_FACTOR_METHOD(rep_len)
  FORWARD_FACTOR_METHOD(resize)

  // Undefine the macros so they don't leak out of the struct
  #undef FORWARD_METHOD
  #undef FORWARD_MUTATING_METHOD
  #undef FORWARD_FACTOR_METHOD

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
    const auto *p_codes = codes.data();

    OMP_SIMD_REDUCTION1(max:max_code)
    for (r_size_t i = 0; i < n; ++i){
        // No need to ignore NA for max() because NA is defined as lowest representable value
        max_code = std::max(max_code, p_codes[i]);
    }

    // If max is still the same value as when initialised, this either means the vector was full of NAs, or the max really is max int
    // Either way, we check in this rare case
    if (max_code == unwrap(r_limits<r_int>::min()) && (codes.na_count() == n)){
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
    maybe_ensure_exclusive();
    safe[Rf_setAttrib](value, symbol::levels_sym, levels);
    cached_levels = internal::levels_cache().get_or_create(static_cast<SEXP>(value));
    cached_levels->invalidate();
  }

  void set_codes(r_vec<r_int> new_codes) {
    r_vec<r_str_view> lvls = levels();
    value = std::move(new_codes);
    cached_levels.reset();
    init_factor(lvls, false);
  }
  
  // Direct constructor from integer codes + string levels
  template <RStringType T>
  explicit r_factors(r_vec<r_int> codes, const r_vec<T>& levels, bool check_valid_levels = chk_fct_lvls_opt) : value(std::move(codes)){
      init_factor(levels, check_valid_levels);
  }

  private:

  template <RStringType T>
  void init_factor(const r_vec<T>& levels, bool check_valid_levels = chk_fct_lvls_opt) {
      // Set class
      attr::set_attr(value, symbol::class_sym, r_vec<r_str_view>(1, r_str_view(cached_str<"factor">())));
      // Set levels
      set_levels(levels, check_valid_levels);
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
  explicit r_factors(r_sexp x, bool check_valid_levels = chk_fct_lvls_opt) : value(std::move(x)) {
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
  explicit operator r_sexp() const noexcept { return static_cast<r_sexp>(value); }

  // Find factor code associated with factor string
  // Since levels are assumed to be unique, we find the first match
  template <RStringType U>
  r_int get_code(const U& val, r_int no_match = na<r_int>()) const {
    
    // Hash path: cache already built by us or by a sibling wrapper.
    if (cached_levels && cached_levels->names.has_value()) {
      r_int out = cached_levels->find(val, /*offset = */ 1);
      return is_na(out) ? no_match : out;
    }

    // Second-or-later lookup without a built cache: build it.
    if (first_access) {
      ensure_levels_cached();
      r_int out = cached_levels->find(val, /*offset = */ 1);
      return is_na(out) ? no_match : out;
    }

    first_access = true;

    // First lookup: linear scan over the levels STRSXP.
    r_vec<r_str_view> levels_attr = levels();
    if (levels_attr.is_null()) [[unlikely]] {
      return no_match;
    }
    r_size_t n = levels_attr.length();
    auto key = unwrap(val);
    const auto* RESTRICT p = levels_attr.data();
    for (r_size_t i = 0; i < n; ++i) {
      if (p[i] == key){
        return r_int(static_cast<int>(i) + 1);
      }
    }
    return no_match;
  }

  r_int get_code(const char* val) const {
    return get_code(r_str(val));
  }

  r_int get_code(r_size_t index) const {
    return value.get(index);
  }

  // Find factor codes associated with character vector
  template <RStringType U>
  r_vec<r_int> get_codes(const r_vec<U>& vals, r_int no_match = na<r_int>()) const {
    int n = vals.length();
    r_vec<r_int> out(n);
    for (int i = 0; i < n; ++i){
      r_int code = get_code(vals.view(i), no_match);
      out.set(i, code);
    }
    return out;
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

  // Generate new factor codes along x against new factor levels
  template <RStringType U>
  r_vec<r_int> new_codes(const r_vec<U>& new_levels, r_int no_match = na<r_int>()) const {
    // Empty codes — we only need the temp's levels for lookup
    r_factors new_lvls_fct(r_vec<r_int>(), new_levels);
    
    // For each of this factor's levels, find its position in new_levels
    r_vec<r_int> remap = new_lvls_fct.get_codes(levels(), no_match);
    r_size_t n = length();
    r_vec<r_int> out(n);
    for (r_size_t i = 0; i < n; ++i){
      r_int c = value.get(i);
      // Legit NA codes always stay NA — only unmapped levels use the sentinel
      out.set(i, is_na(c) ? na<r_int>() : remap.get(unwrap(c) - 1));
    }
    return out;
  }

  // Re-generate factor using new factor levels in-place
  template <RStringType U>
  void recode(const r_vec<U>& new_levels) {
    // Empty codes — we only need the temp's levels for lookup
    r_factors new_lvls_fct(r_vec<r_int>(), new_levels);
    
    // For each of this factor's levels, find its position in new_levels
    r_vec<r_int> remap = new_lvls_fct.get_codes(levels(), na<r_int>());
    r_size_t n = length();
    for (r_size_t i = 0; i < n; ++i){
      r_int c = value.get(i);
      // Legit NA codes always stay NA — only unmapped levels use the sentinel
      value.set(i, is_na(c) ? na<r_int>() : remap.get(unwrap(c) - 1));
    }
    set_levels(new_levels);
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

};

namespace internal {

// Same shape as share_name_cache, for the levels cache on r_factors.
//
// Shares only the inner sexp_index_table (via shared_ptr), not the enclosing
// names_map. Each wrapper keeps its own names_map so that a future mutation
// of one (e.g. append_level → set_levels → invalidate) cannot poison the
// other's view. The inner table is still safe to share because both sides
// have the same levels STRSXP at the point of sharing; any later mutation
// goes through append_level's detach-when-shared path.
inline void share_levels_cache(r_factors& target, const r_factors& source) {
    if (!source.cached_levels) return;
    if (!source.cached_levels->names.has_value()) return;
    SEXP target_levels = Rf_protect(Rf_getAttrib(target, symbol::levels_sym));
    if (target_levels != static_cast<SEXP>(*source.cached_levels->names)){
        Rf_unprotect(1);
        return;
    }
    target.ensure_levels_cached();
    target.cached_levels->map = source.cached_levels->map;
    Rf_unprotect(1);
}

}

}

#endif
