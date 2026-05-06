#ifndef CPPALLY_R_FACTOR_H
#define CPPALLY_R_FACTOR_H

#include <cppally/r_limits.h>
#include <cppally/r_vec.h>
#include <cppally/r_attrs.h>
#include <optional>
#include <ankerl/unordered_dense.h> // Hash map for levels

namespace cppally {

struct r_factors {

  public:

  r_vec<r_int> value;

  private: 

  r_vec<r_str_view> cached_levels; // Cache levels to avoid overhead of retrieving attribute

  // Lazily-loaded hash table of levels (initialised once when `get_code()` is called)
  using levels_map_t = ankerl::unordered_dense::map<SEXP, int>; // nullptr until first get_code()
  mutable std::optional<levels_map_t> levels_hash_table;

  public: 

  r_vec<r_str_view> levels() const noexcept {
    return cached_levels;
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

  void validate_factor(bool check_valid_levels = true){
    if (!attr::inherits1(value, "factor")) [[unlikely]] {
      abort("SEXP must be of class 'factor' to be constructed as a factor");
    }
    if (check_valid_levels){
      validate_levels(value, levels());
    }
  }

  public:

  template <RStringType T>
  void set_levels(const r_vec<T>& levels, bool check_valid_levels = true) {
    if (check_valid_levels){
      validate_levels(value, levels);
    }
    attr::set_attr(value, symbol::levels_sym, levels);
    cached_levels = r_vec<r_str_view>(static_cast<SEXP>(levels));
    levels_hash_table.reset();
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


  void lazy_hash_levels() const {

    // If hash table already built, exit
    if (levels_hash_table.has_value()) return;

    r_vec<r_str_view> lvls = levels();
    int n = lvls.length();

    levels_hash_table.emplace();
    levels_hash_table->reserve(static_cast<std::size_t>(n));

    for (int i = 0; i < n; ++i) {
        levels_hash_table->emplace(unwrap(lvls.view(i)), i + 1);
    }
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
      cached_levels = r_vec<r_str_view>(attr::get_attr(value, symbol::levels_sym));
      validate_factor(check_valid_levels);
    }
  }

  explicit r_factors(SEXP x, internal::view_tag, bool check_valid_levels = true) : value(x, internal::view_tag{}) {
    if (!value.is_null()){
      cached_levels = r_vec<r_str_view>(attr::get_attr(value, symbol::levels_sym));
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
    lazy_hash_levels();
    auto it = levels_hash_table->find(unwrap(val));
    if (it == levels_hash_table->end()) {
        return na<r_int>();
    }
    return r_int(it->second);
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
      return r_factors(std::move(fct_codes), this->levels(), false);
    }
    r_int code = get_code(val);
    if (is_na(code)){
      return *this;
    }
    r_vec<r_int> fct_codes = value.remove(code);
    return r_factors(std::move(fct_codes), this->levels(), false);
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

    // Store previous hash table as set_levels() resets
    auto previous_hash_table = levels_hash_table;

    set_levels(new_levels, false);

    previous_hash_table->emplace(unwrap(level), previous_hash_table->size() + 1);
    levels_hash_table = std::move(previous_hash_table);
  }

};

}

#endif
