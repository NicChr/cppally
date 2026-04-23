#ifndef CPPALLY_R_VECTOR_H
#define CPPALLY_R_VECTOR_H

#include <cppally/r_setup.h>
#include <cppally/r_concepts.h>
#include <cppally/r_utils.h>
#include <cppally/r_limits.h>
#include <cppally/r_scalar_methods.h>
#include <cppally/r_vec_utils.h>
#include <cppally/r_coerce_impl.h>
#include <algorithm>

namespace cppally {

// Forward declarations
template <typename T, typename U>
inline constexpr bool identical(const T& a, const U& b);

template <RVal T>
inline void r_copy_n(r_vec<T>& target, const r_vec<T>& source, r_size_t target_offset, r_size_t n);
  
namespace internal {

// Concept helpers for location-based subset helpers
template <typename T>
concept RSubscript = any<T, r_lgl, r_int, r_int64, r_str_view, r_str>;

template <typename T>
concept RNumericSubscript = any<T, r_int, r_int64>;
}

template<RVal T>
struct r_vec {

  r_sexp sexp = r_null;
  using data_type = std::remove_cvref_t<T>;

  bool is_null() const noexcept {
    return sexp.is_null();
  }

  bool is_altrep() const noexcept {
    return ALTREP(sexp.value);
  }

  private:

  // Initialise read-only ptr to:
  // SEXP - If T is a type convertible to SEXP
  // unwrap_t<T> - Otherwise
  static constexpr bool is_write_barrier_protected = RObject<T>;
  using ptr_t = std::conditional_t<is_write_barrier_protected, const SEXP*, unwrap_t<T>*>;
#ifdef CPPALLY_PRESERVE_ALTREP
  // `mutable` so that lazy materialisation for ALTREP inputs can happen
  // For non-ALTREP this is a one-shot init at construction
  mutable ptr_t m_ptr = nullptr;
#else
  ptr_t m_ptr = nullptr;
#endif

  void initialise_ptr(){
#ifdef CPPALLY_PRESERVE_ALTREP
    // For ALTREP leave m_ptr null so get/view go through elt<T>
    // data() will materialise on first call
    if (is_altrep()) return;
#endif
    if constexpr (is_write_barrier_protected){
      m_ptr = internal::vector_ptr_ro<T>(sexp);
    } else {
      m_ptr = internal::vector_ptr<T>(sexp);
    }
  }

  // By default do nothing (e.g. for vectors with no attrs)
  template <typename U>
  void validate_attrs(SEXP x){
    return;
  }

  template <RDateType U>
  void validate_attrs(SEXP x){
    if (!Rf_inherits(x, "Date")) [[unlikely]] {
      abort("SEXP must be of class 'Date'");
    }
  }

  template <RPsxctType U>
  void validate_attrs(SEXP x){
    if (!Rf_inherits(x, "POSIXct")) [[unlikely]] {
      abort("SEXP must inherit class 'POSIXct'");
    }
  }

  public:

  // Constructor that wraps new_vec_impl<T>
  explicit r_vec(r_size_t n)
    : sexp(internal::new_vec_impl<data_type>(n))
  {
    initialise_ptr();
  }

  explicit r_vec(r_size_t n, T const& default_value)
    : r_vec(n)
  {
    fill(r_size_t(0), n, default_value);
  }
  
  r_vec(): r_vec(r_size_t(0)){}

  // Constructors from existing r_sexp/SEXP
  explicit r_vec(r_sexp s) : sexp(std::move(s)) {
    if (!is_null()) {
      internal::check_valid_construction<r_vec<T>>(sexp);
      validate_attrs<T>(sexp.value);
      initialise_ptr();
    }
  }

  explicit r_vec(const r_sexp& s, internal::view_tag) : sexp(s.value, internal::view_tag{}){
    if (!is_null()){
      internal::check_valid_construction<r_vec<T>>(sexp);
      validate_attrs<T>(sexp.value);
      initialise_ptr();
    }
  }

  explicit r_vec(SEXP s) : r_vec(r_sexp(s)) {}
  explicit r_vec(SEXP s, internal::view_tag) : r_vec(r_sexp(s, internal::view_tag{}), internal::view_tag{}) {}

  // Implicit conversion to SEXP
  operator SEXP() const noexcept {
    return sexp.value;
  }

  // Explicit conversion to r_sexp
  explicit operator r_sexp() const noexcept {
    return sexp;
  }

  // Direct pointer access - materialises ALTREP when CPPALLY_PRESERVE_ALTREP is on
  ptr_t data() const {
#ifdef CPPALLY_PRESERVE_ALTREP
    if (!m_ptr) [[unlikely]] {
      if constexpr (is_write_barrier_protected) {
        m_ptr = internal::vector_ptr_ro<T>(sexp);
      } else {
        m_ptr = internal::vector_ptr<T>(sexp);
      }
    }
#endif
    return m_ptr;
  }

  // Iterator support - begin + end
  ptr_t begin() {
      return data();
  }

  ptr_t end() {
      return data() + size();
  }

  ptr_t begin() const {
      return data();
  }

  ptr_t end() const {
      return data() + size();
  }

  r_size_t length() const noexcept {
    return sexp.length();
  }

  // Get size
  r_size_t size() const noexcept {
    return length();
  }

  bool is_long() const noexcept {
    return length() > unwrap(r_limits<r_int>::max());
  }

  r_str address() const {
    return sexp.address();
  }

  // Get element (no bounds-check)
  T get(r_size_t index) const {
#ifdef CPPALLY_PRESERVE_ALTREP
    if (m_ptr) [[likely]] {
      return T(m_ptr[index]);
    } else {
      return T(internal::elt<T>(sexp, index));
    }
#else
    return T(m_ptr[index]);
#endif
  }

  // View element (like `get()` but elements must be short-lived)
  // Element must not outlive the parent vector
  T view(r_size_t index) const {
    if constexpr (std::is_constructible_v<data_type, unwrap_t<data_type>, internal::view_tag>) {
#ifdef CPPALLY_PRESERVE_ALTREP
      if (m_ptr) [[likely]] {
        return T(m_ptr[index], internal::view_tag{});
      } else {
        return T(internal::elt<T>(sexp, index), internal::view_tag{});
      }
#else
      return T(m_ptr[index], internal::view_tag{});
#endif
    } else {
#ifdef CPPALLY_PRESERVE_ALTREP
      if (m_ptr) [[likely]] {
        return T(m_ptr[index]);
      } else {
        return T(internal::elt<T>(sexp, index));
      }
#else
      return T(m_ptr[index]);
#endif
    }
  }

  // Set element (no bounds-check)
  void set(r_size_t index, const T& val) {
      if constexpr (RStringType<T>){
        SET_STRING_ELT(sexp, index, val);
      } else if constexpr (RObject<T>){
        SET_VECTOR_ELT(sexp, index, val);
      } else {
        static_assert(!is_write_barrier_protected, "Can't write data directly here, data is R write-barrier protected");
        data()[index] = unwrap(val);
      }
  }

  template <typename U>
  void set(r_size_t index, const U& val) {
    set(index, as<T>(val));
  }

  template <internal::RSubscript U> 
  r_vec<T> subset(const r_vec<U>& indices, bool check = true, bool invert = false) const;

  r_vec<T> subset(r_size_t index, bool check = true, bool invert = false) const {
    return subset(r_vec<r_int64>(1, r_int64(static_cast<int64_t>(index))), check, invert);
  }

  r_vec<r_str_view> names() const {
    r_vec<r_str_view> out = r_vec<r_str_view>(Rf_getAttrib(sexp, symbol::names_sym));
    if (!out.is_null() && out.length() != length()) [[unlikely]] {
      abort("bad names detected, length of names must match vector length");
    }
    return out;
  }

  template <RStringType U>
  void set_names(const r_vec<U>& names){
    if (names.is_null()){
      Rf_setAttrib(sexp, symbol::names_sym, r_null);
    } else if (names.length() != sexp.length()) [[unlikely]] {
      abort("`length(names)` must equal `length(x)`");
    } else {
      Rf_namesgets(sexp, names);
    }
  }

  r_vec<r_lgl> is_na() const {
    r_size_t n = length();
    r_vec<r_lgl> out(n);

    if constexpr (!is_write_barrier_protected){
      int n_threads = internal::calc_threads(n);
      if (n_threads > 1){
        OMP_PARALLEL_FOR_SIMD(n_threads)
        for (r_size_t i = 0; i < n; ++i){
          out.set(i, r_lgl(cppally::is_na(view(i))));
        }
      } else {
        OMP_SIMD
        for (r_size_t i = 0; i < n; ++i){
          out.set(i, r_lgl(cppally::is_na(view(i))));
        }
      }
    } else {
      for (r_size_t i = 0; i < n; ++i){
        out.set(i, r_lgl(cppally::is_na(view(i))));
      }
    }

    return out;
  }

  r_size_t na_count() const {
    
    r_size_t n = length();
    r_size_t out(0);

    if constexpr (!is_write_barrier_protected){
      int n_threads = internal::calc_threads(n);

      if (n_threads > 1){
        OMP_PARALLEL_FOR_SIMD_REDUCTION1(n_threads, +:out)
        for (r_size_t i = 0; i < n; ++i){
          out += cppally::is_na(view(i));
        }
      } else {
        OMP_SIMD_REDUCTION1(+:out)
        for (r_size_t i = 0; i < n; ++i){
          out += cppally::is_na(view(i));
        }
      }
    } else {
      for (r_size_t i = 0; i < n; ++i){
        out += cppally::is_na(view(i));
      }
    }

    return out;
  }

  bool any_na() const {
    r_size_t n = length();
    for (r_size_t i = 0; i < n; ++i){
      if (cppally::is_na(view(i))) return true;
    }
    return false;
  }

  bool all_na() const {
    r_size_t n = length();
    for (r_size_t i = 0; i < n; ++i){
      if (!cppally::is_na(view(i))) return false;
    }
    return true;
  }

  bool any_v(const T& val) const {
    r_size_t n = length();
    for (r_size_t i = 0; i < n; ++i){
      if (identical(view(i), val)) return true;
    }
    return false;
  }

  bool all_v(const T& val) const {
    r_size_t n = length();
    for (r_size_t i = 0; i < n; ++i){
      if (!identical(view(i), val)) return false;
    }
    return true;
  } 

  r_size_t count(T const& value) const {
    r_size_t out = 0;
    r_size_t n = length();

    if constexpr (RIntegerType<T>){
      // SIMD vectorisation isn't working with identical function (sad)
      OMP_SIMD_REDUCTION1(+:out)
      for (r_size_t i = 0; i < n; ++i){
        out += (unwrap(get(i)) == unwrap(value));
      }
    } else {
      // Fall-back
      for (r_size_t i = 0; i < n; ++i){
        out += identical(view(i), value);
      }
    }
    return out;
  }
  r_vec<T> remove(T const& value) const {
    r_size_t n_remove = count(value);
    r_size_t n = length();

    if (n_remove == 0){
      return *this;
    } else if (n_remove == n){
      return r_vec<T>();
    } else {
      r_vec<T> out(n - n_remove);
      r_size_t k = 0;

      for (r_size_t i = 0; i < n; ++i){
        if (!identical(view(i), value)){
          out.set(k++, view(i));
        }
      }
      return out;
    }
  }

  // locations of value in vector
  template <internal::RNumericSubscript V = r_int>
  r_vec<V> find(T const& value, bool invert = false) const {

    r_size_t n = length();

    using int_t = unwrap_t<V>;
  
    if constexpr (is<V, r_int>){
      if ( (n > r_limits<r_int>::max()).is_true()){
        abort("`x` is a long vector, please use `find<r_int64>` instead");
      }
    }
  
    r_size_t n_vals = count(value);
    int_t whichi = 0; 
    int_t i = 0; 
  
    if (invert){
      int_t out_size = n - n_vals;
      r_vec<V> out(out_size);
      while (whichi < out_size){
          out.set(whichi, V(i));
          whichi += static_cast<int_t>(!identical(view(i++), value));
      }
      return out;
    } else {
      int_t out_size = n_vals;
      r_vec<V> out(out_size);
      while (whichi < out_size){
        out.set(whichi, V(i));
        whichi += static_cast<int_t>(identical(view(i++), value));
    }
    return out;
    }
  }

  // Sequential fill
  void fill(r_size_t start, r_size_t n, T const& val){
    if constexpr (!is_write_barrier_protected){
      int n_threads = internal::calc_threads(n);
      // data() materialises ALTREP on first call
      auto* RESTRICT p_target = data();
      if (n_threads > 1) {
        OMP_PARALLEL_FOR_SIMD(n_threads)
        for (r_size_t i = 0; i < n; ++i) {
          p_target[start + i] = unwrap(val);
        }
      } else {
        std::fill_n(p_target + start, n, unwrap(val));
      }
    } else {
      for (r_size_t i = 0; i < n; ++i) {
        set(start + i, val);
      }
    }
  }

  // Fill entire vector with value
  void fill(T const& val){
    fill(0, length(), val);
  }

  void replace(r_size_t start, r_size_t n, T const& old_val, T const& new_val){
    for (r_size_t i = 0; i < n; ++i) {
      r_size_t idx = start + i;
      if (identical(view(idx), old_val)){
        set(idx, new_val);
      }
    }
  }

  // Replace all occurrences of old_val with new_val
  void replace(T const& old_val, T const& new_val){
    replace(0, length(), old_val, new_val);
  }

  r_size_t count(const r_vec<T>& values) const;
  template <internal::RNumericSubscript V = r_int>
  r_vec<V> find(const r_vec<T>& values, bool invert = false) const;
  r_vec<T> remove(const r_vec<T>& values) const;
  template <internal::RSubscript U>
  void fill(const r_vec<U>& where, const r_vec<T>& with);
  void replace(const r_vec<T>& old_values, const r_vec<T>& new_values);


  r_vec<T> resize(r_size_t n) const {
    r_size_t vec_size = length();
    if (n == vec_size){
      return *this;
    } else {
      auto resized_vec = r_vec<T>(n);
      r_size_t n_to_copy = std::min(n, vec_size);

      if constexpr (!is_write_barrier_protected){
        std::copy_n(this->begin(), n_to_copy, resized_vec.begin());
      } else {
        for (r_size_t i = 0; i < n_to_copy; ++i){
          resized_vec.set(i, view(i)); 
        }
      }
      return resized_vec;
    }
  }

  r_vec<T> rep_len(r_size_t n) const {

    r_size_t size = length();

    if (size == n){
      return *this;
    }

    r_vec<T> out(n);

    if (size == 1){
      out.fill(view(0));
    } else if (n > 0 && size > 0){
      // Copy first block
      r_copy_n(out, *this, 0, std::min(size, n));

      if (n > size){
        // copy result to itself, doubling each iteration
        r_size_t copied = size;
        while (copied < n) {
          r_size_t to_copy = std::min(copied, n - copied);
          r_copy_n(out, out, copied, to_copy);
          copied += to_copy;
        }
    }
      // If length > 0 but length(x) == 0 then fill with NA
    } else if (size == 0 && n > 0){
      if constexpr (RScalar<T>){
        out.fill(na<T>());
      }
    }
    return out;
  }

  // Forward decl
  // r_vec<T> remove(r_size_t index) const;


  // Conditional member functions (only available for certain types)

  // POSIXct-only members
  r_str tzone() const requires RPsxctType<T> {
    auto tz = r_vec<r_str_view>(Rf_getAttrib(sexp, cached_sym<"tzone">()));
    
    if (tz.length() == 0){
      abort("`r_vec<r_psxct_t>` vector must have a valid tzone attribute");
    } else {
      r_str_view tz_str = tz.view(0);
      if (cppally::is_na(tz_str)){
        abort("tzone cannot be NA");
      }
      return r_str(tz_str);
    }
  }

  void set_tzone(const char* tz) requires RPsxctType<T> {
    Rf_setAttrib(sexp, cached_sym<"tzone">(), r_vec<r_str>(1, r_str(tz)));
  }

  // list-only members

  r_vec<r_int> lengths() const requires is<T, r_sexp> {
    r_size_t n = length();
    r_vec<r_int> out(n);

    for (r_size_t i = 0; i < n; ++i){
      r_size_t len = view(i).length();
      if (len > unwrap(r_limits<r_int>::max())) [[unlikely]] {
        abort("`lengths()` does not currently support long-vectors");
      }
      out.set(i, r_int(static_cast<int>(len)));
    }
    return out;
  }

};

template <RVal T>
inline void r_copy_n(r_vec<T>& target, const r_vec<T>& source, r_size_t target_offset, r_size_t n){

  if constexpr (internal::RPtrWritableType<T>){
    auto* RESTRICT p_target = target.data();
    auto* RESTRICT p_source = source.data();

    int n_threads = internal::calc_threads(n);
    if (n_threads > 1) {
      OMP_PARALLEL_FOR_SIMD(n_threads)
      for (r_size_t i = 0; i < n; ++i) {
        p_target[target_offset + i] = p_source[i];
      }
    } else {
      std::copy_n(p_source, n, p_target + target_offset);
    }
  } else if constexpr (RStringType<T>){
    // Cast const SEXP* to SEXP* and write directly
    auto* p_target = const_cast<unwrap_t<T>*>(target.data());
    std::copy_n(source.data(), n, p_target + target_offset);
  } else {
    for (r_size_t i = 0; i < n; ++i) {
      target.set(target_offset + i, source.view(i));
    }
  }
}

}

#endif
