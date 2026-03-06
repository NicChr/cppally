#ifndef CPP20_R_VECTOR_H
#define CPP20_R_VECTOR_H

#include <cpp20/internal/r_symbols.h>
#include <cpp20/internal/r_methods.h>
#include <cpp20/internal/r_vec_utils.h>
#include <cpp20/internal/r_rtype_coerce.h>

namespace cpp20 {

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
  using ptr_t = std::conditional_t<internal::RPtrWritableType<T>, unwrap_t<T>*, const SEXP*>;  
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
      if constexpr (any<T, r_sexp, r_sym>){
        SET_VECTOR_ELT(sexp, index, unwrap(val));
      } else if constexpr (RStringType<T>){
        SET_STRING_ELT(sexp, index, unwrap(val));
      } else {
        m_ptr[index] = unwrap(val);
      }
    } else {
      if constexpr (any<T, r_sexp, r_sym>){
        SET_VECTOR_ELT(sexp, index, unwrap(cpp20::internal::as_r<T>(val)));
      } else if constexpr (RStringType<T>){
        SET_STRING_ELT(sexp, index, unwrap(cpp20::internal::as_r<T>(val)));
      } else {
        m_ptr[index] = unwrap(cpp20::internal::as_r<T>(val));
      }
    }
  }

  // IMPORTANT - indices are 1-indexed
  // This has the benefit of allowing empty locations (0) and negative indexing
  template <typename U>
  requires (any<U, r_lgl, r_int, r_int64, r_str_view, r_str>)
  r_vec<T> subset(const r_vec<U>& indices) const;

  template <IntegerType U>
  r_vec<T> subset(U index) const {
    if constexpr (internal::can_definitely_be_int<unwrap_t<U>>()){
      return subset(r_vec<r_int>(1, internal::as_r<r_int>(index)));
    } else {
      return subset(r_vec<r_int64>(1, internal::as_r<r_int64>(index)));
    }
  }

  r_vec<r_str_view> names() const {
    return r_vec<r_str_view>(Rf_getAttrib(sexp, symbol::names_sym));
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
    auto out = r_vec<r_lgl>(n);

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
    return na_count() > 0;
  }

  bool all_na() const {
    return na_count() == length();
  }

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

  template <typename U1, typename U2>
  void replace(r_size_t start, r_size_t n, U1 const& old_val, U2 const& new_val){
    auto old_val2 = internal::as_r<T>(old_val);
    auto new_val2 = internal::as_r<T>(new_val);
    bool implicit_na_coercion = !cpp20::is_na(old_val) && cpp20::is_na(old_val2);
    if (!implicit_na_coercion){
      if constexpr (internal::RPtrWritableType<T>){
        int n_threads = internal::calc_threads(n);
        auto *p_target = data();
        if (n_threads > 1) {
          OMP_PARALLEL_FOR_SIMD(n_threads)
          for (r_size_t i = 0; i < n; ++i) {
            r_lgl eq = p_target[start + i] == old_val2;
            if (eq.is_true()){
              p_target[start + i] = unwrap(new_val2);
            }
          }
        } else {
          OMP_SIMD
          for (r_size_t i = 0; i < n; ++i) {
            r_lgl eq = p_target[start + i] == old_val2;
            if (eq.is_true()){
              p_target[start + i] = unwrap(new_val2);
            }
          }
        }
      } else {
        for (r_size_t i = 0; i < n; ++i) {
          r_size_t idx = start + i;
          if (view(idx) == old_val2){
            set(idx, new_val2);
          }
        }
      }
    }
  }

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

    auto out = r_vec<T>(n);

    if (size == 1){
      out.fill(0, n, view(0));
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
      out.fill(0, n, na<T>());
    }
    return out;
  }


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
    auto *p_source = source.data();
    auto *p_target = target.data();

    int n_threads = internal::calc_threads(n);
    if (n_threads > 1) {
      OMP_PARALLEL_FOR_SIMD(n_threads)
      for (r_size_t i = 0; i < n; ++i) {
        p_target[target_offset + i] = p_source[i];
      }
    } else {
      std::copy_n(p_source, n, p_target + target_offset);
    }
  } else {
    for (r_size_t i = 0; i < n; ++i) {
      target.set(target_offset + i, source.view(i));
    }
  }
}

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
