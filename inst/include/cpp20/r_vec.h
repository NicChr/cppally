#ifndef CPP20_R_VECTOR_H
#define CPP20_R_VECTOR_H

#include <cpp20/r_setup.h>
#include <cpp20/r_concepts.h>
#include <cpp20/r_utils.h>
#include <cpp20/r_limits.h>
#include <cpp20/r_symbols.h>
#include <cpp20/r_methods.h>
#include <cpp20/r_vec_utils.h>
#include <cpp20/r_coerce_impl.h>

namespace cpp20 {

// Forward declarations
template <typename T, typename U>
inline constexpr bool identical(const T& a, const U& b);

template <RVal T>
inline void r_copy_n(r_vec<T>& target, const r_vec<T>& source, r_size_t target_offset, r_size_t n);
  
namespace internal {

template <typename T, typename U>
inline bool is_implicit_na_coercion(const T& before, const U& after){
  return !is_na(before) && is_na(after);
}

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

  private:

  // Initialise read-only ptr to: 
  // SEXP - If T is a type convertible to SEXP
  // unwrap_t<T> - Otherwise
  using ptr_t = std::conditional_t<RObject<T>, const SEXP*, unwrap_t<T>*>;
  ptr_t m_ptr = nullptr;

  void initialise_ptr(){
    if constexpr (internal::RPtrWritableType<T>) {
      m_ptr = internal::vector_ptr<T>(sexp);
    } else if constexpr (any<T, r_sexp, r_sym>) {
      m_ptr = (const SEXP*) DATAPTR_RO(sexp);
    } else if constexpr (RStringType<T>){
      m_ptr = (const SEXP*) STRING_PTR_RO(sexp);
    }
  }

  // By default do nothing (e.g. for vectors with no attrs)
  template <typename U>
  void validate_attrs(SEXP x){
    return;
  }

  template <RDateType U>
  void validate_attrs(SEXP x){
    if (!Rf_inherits(x, "Date")){
      abort("SEXP must be of class 'Date'");
    }
  }

  template <RPsxctType U>
  void validate_attrs(SEXP x){
    if (!Rf_inherits(x, "POSIXct")){
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

  template<typename U>
  explicit r_vec(r_size_t n, U default_value)
    : r_vec(n)
  {
    fill(r_size_t(0), n, default_value);
  }
  
  r_vec(): r_vec(r_size_t(0)){}

  // Constructors from existing r_sexp/SEXP
  explicit r_vec(r_sexp s) : sexp(std::move(s)) {
    if (!is_null()) {
      internal::check_valid_construction<r_vec<T>>(sexp);
      validate_attrs<T>(s.value);
      initialise_ptr();
    }
  }

  explicit r_vec(const r_sexp& s, internal::view_tag) : sexp(s.value, internal::view_tag{}){
    if (!is_null()){
      internal::check_valid_construction<r_vec<T>>(sexp);
      validate_attrs<T>(s.value);
      initialise_ptr();
    }
  }

  explicit r_vec(SEXP s) : r_vec(r_sexp(s)) {}
  explicit r_vec(SEXP s, internal::view_tag) : r_vec(r_sexp(s, internal::view_tag{}), internal::view_tag{}) {}

  // Implicit conversion to SEXP
  constexpr operator SEXP() const noexcept {
    return sexp.value;
  }

  // Explicit conversion to r_sexp
  constexpr explicit operator r_sexp() const noexcept {
    return sexp;
  }

  // Direct pointer access
  ptr_t data() const {
    return m_ptr;
  }

  // Iterator support - begin + end
  ptr_t begin() {
      return data();
  }

  ptr_t end() {
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

  bool is_bare() const {
    return !Rf_isObject(sexp);
  }

  // Get element (no bounds-check)
  template <CppIntegerType U>
  T get(U index) const {
    return T(m_ptr[index]);
  }

  // View element (like `get()` but copied elements must be short-lived)
  // Element must not live after parent vector has been destroyed
  // It is designed to be a simple view into the vector 
  template <CppIntegerType U>
  T view(U index) const {
    if constexpr (std::is_constructible_v<data_type, unwrap_t<data_type>, internal::view_tag>) {
      return T(m_ptr[index], internal::view_tag{});
    } else {
      return T(m_ptr[index]);
    }
  }

  // Set element (no bounds-check) - We use flexible template to be able to coerce it to an RVal
  template <CppIntegerType U, typename V>
  void set(U index, const V& val) {
    // Avoid copies and extra protections (especially of r_sexp/r_str)
    if constexpr (is<T, V>){
      if constexpr (RStringType<T>){
        SET_STRING_ELT(sexp, index, val);
      } else if constexpr (RObject<T>){
        SET_VECTOR_ELT(sexp, index, val);
      } else {
        m_ptr[index] = unwrap(val);
      }
    } else {
      if constexpr (RStringType<T>){
        SET_STRING_ELT(sexp, index, cpp20::internal::as_r<T>(val));
      } else if constexpr (RObject<T>){
        SET_VECTOR_ELT(sexp, index, cpp20::internal::as_r<T>(val));
      } else {
        m_ptr[index] = unwrap(cpp20::internal::as_r<T>(val));
      }
    }
  }

  // IMPORTANT - indices are 1-indexed
  // This has the benefit of allowing empty locations (0) and negative indexing
  template <internal::RSubscript U> 
  r_vec<T> subset(const r_vec<U>& indices, bool check = true) const;

  template <IntegerType U>
  requires (!any<U, r_lgl, bool>)
  r_vec<T> subset(U index, bool check = true) const {
    if constexpr (internal::can_definitely_be_int<unwrap_t<U>>()){
      return subset(r_vec<r_int>(1, internal::as_r<r_int>(index)), check);
    } else {
      return subset(r_vec<r_int64>(1, internal::as_r<r_int64>(index)), check);
    }
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
    } else if (names.length() != sexp.length()){
      abort("`length(names)` must equal `length(x)`");
    } else {
      Rf_namesgets(sexp, names);
    }
  }

  r_vec<r_lgl> is_na() const {
    r_size_t n = length();
    r_vec<r_lgl> out(n);

    if constexpr (internal::RPtrWritableType<T>){
      int n_threads = internal::calc_threads(n);
      if (n_threads > 1){
        OMP_PARALLEL_FOR_SIMD(n_threads)
        for (r_size_t i = 0; i < n; ++i){
          out.set(i, cpp20::is_na(view(i)));
        }
      } else {
        OMP_SIMD
        for (r_size_t i = 0; i < n; ++i){
          out.set(i, cpp20::is_na(view(i)));
        }
      }
    } else {
      for (r_size_t i = 0; i < n; ++i){
        out.set(i, cpp20::is_na(view(i)));
      }
    }

    return out;
  }

  r_size_t na_count() const {
    
    r_size_t n = length();
    r_size_t out(0);

    if constexpr (internal::RPtrWritableType<T>){
      int n_threads = internal::calc_threads(n);

      if (n_threads > 1){
        #pragma omp parallel for simd num_threads(n_threads) reduction(+:out)
        for (r_size_t i = 0; i < n; ++i){
          out += cpp20::is_na(view(i));
        }
      } else {
        #pragma omp simd reduction(+:out)
        for (r_size_t i = 0; i < n; ++i){
          out += cpp20::is_na(view(i));
        }
      }
    } else {
      for (r_size_t i = 0; i < n; ++i){
        out += cpp20::is_na(view(i));
      }
    }

    return out;
  }

  bool any_na() const {
    r_size_t n = length();
    for (r_size_t i = 0; i < n; ++i){
      if (cpp20::is_na(view(i))) return true;
    }
    return false;
  }

  bool all_na() const {
    r_size_t n = length();
    for (r_size_t i = 0; i < n; ++i){
      if (!cpp20::is_na(view(i))) return false;
    }
    return true;
  }

  template <typename U>
  bool any_v(const U& val) const {
    T val_ = internal::as_r<T>(val);
    r_size_t n = length();
    for (r_size_t i = 0; i < n; ++i){
      if (identical(view(i), val_)) return true;
    }
    return false;
  }

  template <typename U>
  bool all_v(const U& val) const {
    T val_ = internal::as_r<T>(val);
    r_size_t n = length();
    for (r_size_t i = 0; i < n; ++i){
      if (!identical(view(i), val_)) return false;
    }
    return true;
  } 

  template <typename U>
  r_size_t count(U const& value) const {
    r_size_t out = 0;
    r_size_t n = length();
    
    T v = internal::as_r<T>(value);

    // If there was implicit coercion, then avoid counting matches
    if (cpp20::is_na(v) && !cpp20::is_na(value)){
      return out;
    }
    if constexpr (any<T, r_lgl, r_int, r_int64, r_str, r_str_view, r_sym, r_cplx, r_raw>){
      auto* RESTRICT p_x = data();
      auto val_ = unwrap(v);

      // SIMD vectorisation isn't working with identical function (sad)
      // Even though the code is simply: unwrap(x) == unwrap(y)
      // At least we know this is equivalent to identical for the specified types above
      #pragma omp simd reduction(+:out)
      for (r_size_t i = 0; i < n; ++i){
        out += (p_x[i] == val_);
      }
    } else {
      // Fall-back
      for (r_size_t i = 0; i < n; ++i){
        out += identical(view(i), v);
      }
    }

    return out;
  }
  template <typename U>
  r_vec<T> remove(U const& value) const {
    r_size_t n_remove = count(value);
    r_size_t n = length();

    if (n_remove == 0){
      return *this;
    } else if (n_remove == n){
      return r_vec<T>();
    } else {
      r_vec<T> out(n - n_remove);
      r_size_t k = 0;
      T v = internal::as_r<T>(value);

      for (r_size_t i = 0; i < n; ++i){
        if (!identical(view(i), v)){
          out.set(k++, view(i));
        }
      }
      return out;
    }
  }

  // 1-indexed locations of value in vector
  template <internal::RNumericSubscript V = r_int, typename U>
  r_vec<V> find(U const& value, bool invert = false) const {

    r_size_t n = length();

    using int_t = unwrap_t<V>;
  
    if constexpr (is<V, r_int>){
      if ( (n > r_limits<r_int>::max()).is_true()){
        abort("`x` is a long vector, please use `find<r_int64>` instead");
      }
    }

    T v = internal::as_r<T>(value);
  
    r_size_t n_vals = count(value);
    int_t whichi = 0; 
    int_t i = 0; 
  
    if (invert){
      int_t out_size = n - n_vals;
      r_vec<V> out(out_size);
      while (whichi < out_size){
          out.set(whichi, i + 1);
          whichi += static_cast<int_t>(!identical(view(i++), v));
      }
      return out;
    } else {
      int_t out_size = n_vals;
      r_vec<V> out(out_size);
      while (whichi < out_size){
        out.set(whichi, i + 1);
        whichi += static_cast<int_t>(identical(view(i++), v));
    }
    return out;
    }
  }

  // Sequential fill
  template <typename U>
  void fill(r_size_t start, r_size_t n, U const& val){
    auto val2 = internal::as_r<T>(val);
    if constexpr (internal::RPtrWritableType<T>){
      int n_threads = internal::calc_threads(n);
      auto* RESTRICT p_target = data();
      if (n_threads > 1) {
        OMP_PARALLEL_FOR_SIMD(n_threads)
        for (r_size_t i = 0; i < n; ++i) {
          p_target[start + i] = unwrap(val2);
        }
      } else {
        std::fill_n(p_target + start, n, unwrap(val2));
      }
    } else {
      for (r_size_t i = 0; i < n; ++i) {
        set(start + i, val2);
      }
    }
  }

  // Fill entire vector with value
  template <typename U>
  void fill(U const& val){
    fill(0, length(), val);
  }

  template <typename U1, typename U2>
  void replace(r_size_t start, r_size_t n, U1 const& old_val, U2 const& new_val){
    T old_val2 = internal::as_r<T>(old_val);
    T new_val2 = internal::as_r<T>(new_val);
    if (!internal::is_implicit_na_coercion(old_val, old_val2)){
        for (r_size_t i = 0; i < n; ++i) {
          r_size_t idx = start + i;
          if (identical(view(idx), old_val2)){
            set(idx, new_val2);
          }
        }
    }
  }

  // Replace all occurrences of old_val with new_val
  template <typename U1, typename U2>
  void replace(U1 const& old_val, U2 const& new_val){
    replace(0, length(), old_val, new_val);
  }

  template <typename U>
  r_size_t count(const r_vec<U>& values) const;
  template <internal::RNumericSubscript V = r_int, typename U>
  r_vec<V> find(const r_vec<U>& values, bool invert = false) const;
  template <typename U>
  r_vec<T> remove(const r_vec<U>& values) const;
  template <internal::RSubscript U, typename V>
  void fill(const r_vec<U>& where, const r_vec<V>& with);
  template <typename U1, typename U2>
  void replace(const r_vec<U1>& old_values, const r_vec<U2>& new_values);


  r_vec<T> resize(r_size_t n){
    r_size_t vec_size = length();
    if (n == vec_size){
      return *this;
    } else {
      auto resized_vec = r_vec<T>(n);
      r_size_t n_to_copy = std::min(n, vec_size);

      if constexpr (internal::RPtrWritableType<T>){
        std::copy_n(this->begin(), n_to_copy, resized_vec.begin());
      } else {
        for (r_size_t i = 0; i < n_to_copy; ++i){
          resized_vec.set(i, view(i)); 
        }
      }
      return resized_vec;
    }
  }

  r_vec<T> rep_len(r_size_t n){

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
      out.fill(na<T>());
    }
    return out;
  }

  // Forward decl
  // r_vec<T> remove(r_size_t index) const;


  // Conditional member functions (only available for certain types)

  // POSIXct-only members
  r_str tzone() const requires RPsxctType<T> {
    auto tz = r_vec<r_str_view>(Rf_getAttrib(sexp, r_sym("tzone")));
    
    if (tz.length() == 0){
      abort("`r_vec<r_psxct_t>` vector must have a valid tzone attribute");
    } else {
      r_str_view tz_str = tz.view(0);
      if (cpp20::is_na(tz_str)){
        abort("tzone cannot be NA");
      }
      return r_str(tz_str);
    }
  }

  void set_tzone(const char* tz) requires RPsxctType<T> {
    Rf_setAttrib(sexp, r_sym("tzone"), Rf_ScalarString(Rf_mkCharCE(tz, CE_UTF8)));
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
  } else if constexpr (RObject<T>){
    // Cast const SEXP* to SEXP* and write directly
    auto* p_target = const_cast<unwrap_t<T>*>(target.data());
    std::copy_n(source.data(), n, p_target + target_offset);
  } else {
    for (r_size_t i = 0; i < n; ++i) {
      target.set(target_offset + i, source.view(i));
    }
  }
}

// template <RVal T>
// r_vec<T> r_vec<T>::remove(r_size_t index) const {
//   std::vector<int> out(3);
//   out.erase(
//   if (index 
// }

// Compact seq generator as ALTREP, same as `seq_len()`
// ALTREP is currently unsupported due to the overhead in checking altrep
// inline r_vec<r_int> compact_seq_len(r_size_t n){
//   if (n < 0){
//     abort("`n` must be >= 0");
//   }
//   if (n == 0){
//     return r_vec<r_int>();
//   }
//   r_sexp colon_fn = fn::find_pkg_fun(":", "base", false);
//   r_sexp out = fn::eval_fn(colon_fn, env::base_env, 1, n);
//   return r_vec<r_int>(out);
// }

}

#endif
