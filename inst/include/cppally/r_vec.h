#ifndef CPPALLY_R_VECTOR_H
#define CPPALLY_R_VECTOR_H

#include <cppally/r_setup.h>
#include <cppally/r_concepts.h>
#include <cppally/r_utils.h>
#include <cppally/r_limits.h>
#include <cppally/r_scalar_ops.h>
#include <cppally/r_vec_utils.h>
#include <cppally/r_coerce_scalars.h>
#include <cppally/r_hash_names.h>
#include <algorithm>
#include <utility>

namespace cppally {

// Forward declarations
template <typename T, typename U>
inline constexpr bool identical(const T& a, const U& b) noexcept(RScalar<T>);

template <RVector T>
inline void r_copy_n(T& target, const T& source, r_size_t target_offset, r_size_t n);

// Defined in r_length.h
inline r_size_t length(const r_sexp& x);
  
namespace internal {

template <typename V>
inline void share_name_cache(V&, const V&);

// Concept helpers for location-based subset helpers
template <typename T>
concept RSubscript = any<T, r_lgl, r_int, r_int64, r_str_view, r_str>;

template <typename T>
concept RNumericSubscript = any<T, r_int, r_int64>;

// break signal for short-circuiting folds (see `r_vec::reduce`) - done(x) stops the fold
template <typename Acc>
struct control_flow {
  using value_type = Acc;
  Acc value;
  bool stop = false;
  control_flow(Acc v) : value(std::move(v)) {}              // bare value = continue
  control_flow(Acc v, bool s) : value(std::move(v)), stop(s) {}
};
template <typename R> struct fold_result                 { using acc = R; static constexpr bool stops = false; };
template <typename A> struct fold_result<control_flow<A>> { using acc = A; static constexpr bool stops = true;  };

}

template <typename Acc>
internal::control_flow<Acc> done(Acc v){ return { std::move(v), true }; }
template <typename Acc>
internal::control_flow<Acc> keep(Acc v){ return { std::move(v), false }; }

template <RVal T>
struct r_vec {

  r_sexp value;
  using data_type = std::remove_cvref_t<T>;

  bool is_null() const noexcept {
    return value.is_null();
  }

  bool is_altrep() const noexcept {
    return value.is_altrep();
  }

  // Is this the only r_vec holding this r_sexp?
  bool is_exclusive() const noexcept {
    return value.is_exclusive();
  }

  // Has data been materialised?
  // Will only have been materialised if data ptr has been assigned
  bool materialised() const noexcept {
    return static_cast<bool>(m_ptr); 
  }

  void ensure_exclusive() {
    if (!is_exclusive()) [[unlikely]] {
      r_size_t n = length();
      r_vec<T> new_vec(n);
      r_copy_n(new_vec, *this, 0, n);
      safe[SHALLOW_DUPLICATE_ATTRIB](new_vec, *this);
      // internal::share_name_cache(new_vec, *this); // Maybe include this
      *this = std::move(new_vec);
    }
  }

  void maybe_ensure_exclusive() {
    #ifdef CPPALLY_COPY_ON_MODIFY
    ensure_exclusive();
    #endif
  }

  private:

  template <RVector U>
  friend void r_copy_n(U& target, const U& source, r_size_t target_offset, r_size_t n);

  // Initialise data (pointer) to:
  // const SEXP* (read-only) - If T is a type convertible to SEXP
  // unwrap_t<T>* - Otherwise
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
    if (!is_null()){
      if constexpr (is_write_barrier_protected){
        m_ptr = is_altrep() ? safe[internal::vector_ptr_ro<T>](value) : internal::vector_ptr_ro<T>(value);
      } else {
        m_ptr = is_altrep() ? safe[internal::vector_ptr<T>](value) : internal::vector_ptr<T>(value);
      }
    }
  }

  // Shared cache: any two r_vec wrappers around the same SEXP point to the same
  // names_map via the registry, so set_names() propagates to all of them
  mutable std::shared_ptr<internal::names_map> cached_names;
  // Counts name_index calls on this wrapper. First lookup uses a linear scan;
  // the hash table is only built on the second, so one-shot callers pay no
  // build cost and the benefit accrues to repeated-lookup C++ code.
  mutable bool first_access = false;

  void ensure_names_cached() const {
    if (!cached_names) {
      cached_names = internal::name_cache().get_or_create(value.value);
    }
    if (!cached_names->names.has_value()) {
      // Construct via r_vec<r_str_view> so the SEXP type is validated before
      // we ever call STRING_PTR_RO on it inside lazy_build()
      r_vec<r_str_view> validated(Rf_getAttrib(value, symbol::names_sym));
      cached_names->names.emplace(static_cast<r_sexp>(validated));
    }
  }

  template <typename V>
  friend void internal::share_name_cache(V&, const V&);

  // By default do nothing (e.g. for vectors with no attrs)
  template <typename U>
  void validate_class(SEXP x) const {
    return;
  }

  template <RDateType U>
  void validate_class(SEXP x) const {
    if (!Rf_inherits(x, "Date")) [[unlikely]] {
      abort("SEXP must be of class 'Date'");
    }
  }

  template <RPsxctType U>
  void validate_class(SEXP x) const {
    if (!Rf_inherits(x, "POSIXct")) [[unlikely]] {
      abort("SEXP must inherit class 'POSIXct'");
    }
  }

  public:

  // Construct new r_vec of length n
  template <typename N>
  requires std::convertible_to<N, r_size_t>
  explicit r_vec(N n) : value(internal::new_vec_impl<data_type>(static_cast<r_size_t>(n))){
    initialise_ptr();
  }

  // Construct new r_vec of length n filled with default value
  template <typename N>
  requires std::convertible_to<N, r_size_t>
  explicit r_vec(N n, const T& default_value) : r_vec(n){
    fill(r_size_t{0}, static_cast<r_size_t>(n), default_value);
  }
  
  r_vec(): r_vec(r_size_t(0)){
    initialise_ptr();
  }

  // Constructors from existing r_sexp/SEXP
  explicit r_vec(r_sexp s) : value(std::move(s)) {
    if (!is_null()) {
      internal::check_valid_construction<r_vec<T>>(value);
      validate_class<T>(value.value);
      initialise_ptr();
    }
  }

  explicit r_vec(const r_sexp& s, internal::view_tag) : value(s.value, internal::view_tag{}){
    if (!is_null()){
      internal::check_valid_construction<r_vec<T>>(value);
      validate_class<T>(value.value);
      initialise_ptr();
    }
  }

  explicit r_vec(SEXP s) : r_vec(r_sexp(s)) {}
  explicit r_vec(SEXP s, internal::view_tag) : r_vec(r_sexp(s, internal::view_tag{}), internal::view_tag{}) {}

  // Unchecked constructors: skip type and class validation
  // For use where the SEXP type is already established (e.g. r_visit.h dispatchers)
  explicit r_vec(r_sexp s, internal::no_checks_tag) : value(std::move(s)) {
    initialise_ptr();
  }
  explicit r_vec(const r_sexp& s, internal::view_tag, internal::no_checks_tag) : value(s.value, internal::view_tag{}){
    initialise_ptr();
  }

  // Implicit conversion to SEXP
  operator SEXP() const noexcept {
    return value.value;
  }

  // Explicit conversion to r_sexp
  explicit operator r_sexp() const noexcept {
    return value;
  }

  // Direct pointer access - materialises ALTREP when CPPALLY_PRESERVE_ALTREP is on
  #ifdef CPPALLY_PRESERVE_ALTREP
  ptr_t data() const {
    if (!m_ptr) [[unlikely]] {
      if (is_null()){
        return m_ptr;
      }
      if constexpr (is_write_barrier_protected) {
        m_ptr = safe[internal::vector_ptr_ro<T>](value);
      } else {
        m_ptr = safe[internal::vector_ptr<T>](value);
      }
    }
    return m_ptr;
  }
  #else
  ptr_t data() const noexcept {
    return m_ptr;
  }
  #endif

  r_size_t length() const noexcept {
    return Rf_xlength(value);
  }

  bool is_long() const noexcept {
    return length() > unwrap(r_limits<r_int>::max());
  }

  r_str address() const {
    return value.address();
  }

  // Cheap presence test — never engages the name cache
  bool has_names() const noexcept {
    return Rf_getAttrib(value, symbol::names_sym) != R_NilValue;
  }

  r_vec<r_str_view> names() const {
    // Unnamed with no cache yet: skip the registry entirely
    if (!cached_names && !has_names()){
      return r_vec<r_str_view>(r_null, internal::view_tag{});
    }
    ensure_names_cached();
    return r_vec<r_str_view>(*cached_names->names);
  }

  template <RStringType U>
  void set_names(const r_vec<U>& names){
      bool removing = names.is_null();
      if (!removing && names.length() != length()) [[unlikely]] {
        abort("`length(names)` must equal `length(x)`");
      }
      // Removing names from an unnamed vector - return early, no copy needed
      if (removing && !has_names()){
        return;
      }
      maybe_ensure_exclusive();
      if (removing){
        Rf_setAttrib(value, symbol::names_sym, r_null);
      } else {
        Rf_namesgets(value, names);
      }
      cached_names = internal::name_cache().get_or_create(value);
      cached_names->invalidate();
  }

  // For named vectors: find first index of name
  // `abort_on_missing` - When supplied name doesn't exist, abort, otherwise return `NA`
  template <RStringType U>
  r_int name_index(const U& name, bool abort_on_missing = true) const {
    auto report_no_match = [&]() {
      abort("%s: There is no element named '%s'", __func__, name.c_str());
    };

    // Second-or-later lookup - cache hash map
    if (first_access) {
      ensure_names_cached();
      r_int index = cached_names->find(name);
      if (is_na(index)){ 
        if (abort_on_missing) {
          report_no_match();
        } else {
          return na<r_int>();
        }
      }
      return index;
    }

    first_access = true;

    // First lookup: linear scan, no hash-table allocation
    r_vec<r_str_view> names_attr = names();
    if (names_attr.is_null()) [[unlikely]] {
      abort("%s: vector has no names", __func__);
    }
    SEXP key = unwrap(name);
    int n = names_attr.length();
    const auto* RESTRICT p = names_attr.data();
    for (int i = 0; i < n; ++i) {
      if (p[i] == key) return r_int(i);
    }
    if (abort_on_missing){
      report_no_match();
    }
    return na<r_int>();
  }

  r_int name_index(const char* name, bool abort_on_missing = true) const {
    return name_index(r_str(name), abort_on_missing);
  }

  // Split get into two constrained members - r_str/r_str_view are special cases where 
  // their SEXP type is verified on construction
  // Since r_vec<r_str> has been verified on construction as a valid STRSXP already, 
  // this means all its elements will be valid CHARSXP and hence we can avoid re-checking on element access

  // Get element (no bounds-check)
  T get(r_size_t index) const requires (!RStringType<T>) {
    #ifdef CPPALLY_PRESERVE_ALTREP
    if (m_ptr) [[likely]] {
      return T(m_ptr[index]);
    } else {
      return T(internal::elt<T>(value, index));
    }
    #else
    return T(m_ptr[index]);
    #endif
  }

  T get(r_size_t index) const requires (RStringType<T>) {
    #ifdef CPPALLY_PRESERVE_ALTREP
    if (m_ptr) [[likely]] {
      return T(m_ptr[index], internal::no_checks_tag{});
    } else {
      return T(internal::elt<T>(value, index), internal::no_checks_tag{});
    }
    #else
    return T(m_ptr[index], internal::no_checks_tag{});
    #endif
  }
  
  template <RStringType U>
  T get(const U& name) const {
    return get(static_cast<r_size_t>(unwrap(name_index(name))));
  }

  T get(const char* name) const {
    return get(r_str(name));
  }

  // View element (like `get()` but elements must be short-lived)
  // Element must not outlive the parent vector
  T view(r_size_t index) const requires (!RStringType<T>) {
    if constexpr (std::is_constructible_v<data_type, unwrap_t<data_type>, internal::view_tag>) {
      #ifdef CPPALLY_PRESERVE_ALTREP
      if (m_ptr) [[likely]] {
        return T(m_ptr[index], internal::view_tag{});
      } else {
        return T(internal::elt<T>(value, index), internal::view_tag{});
      }
      #else
      return T(m_ptr[index], internal::view_tag{});
      #endif
    } else {
      #ifdef CPPALLY_PRESERVE_ALTREP
      if (m_ptr) [[likely]] {
        return T(m_ptr[index]);
      } else {
        return T(internal::elt<T>(value, index));
      }
      #else
      return T(m_ptr[index]);
      #endif
    }
  }
  T view(r_size_t index) const requires (RStringType<T>) {
    #ifdef CPPALLY_PRESERVE_ALTREP
    if (m_ptr) [[likely]] {
      return T(m_ptr[index], internal::view_tag{}, internal::no_checks_tag{});
    } else {
      return T(internal::elt<T>(value, index), internal::view_tag{}, internal::no_checks_tag{});
    }
    #else
    return T(m_ptr[index], internal::view_tag{}, internal::no_checks_tag{});
    #endif
  }

  template <RStringType U>
  T view(const U& name) const {
    return view(static_cast<r_size_t>(unwrap(name_index(name))));
  }

  T view(const char* name) const {
    return view(r_str(name));
  }

  // Set element (no bounds-check)
  void set(r_size_t index, const T& val) {
    #ifdef CPPALLY_COPY_ON_MODIFY
    ensure_exclusive();
    #endif
    if constexpr (RStringType<T>){
      SET_STRING_ELT(value, index, val);
    } else if constexpr (is<T, r_sexp>){
      SET_VECTOR_ELT(value, index, val);
    } else {
      static_assert(!is_write_barrier_protected, "Can't write data directly here, data is R write-barrier protected");
      data()[index] = unwrap(val);
    }
  }

  template <typename U>
  void set(r_size_t index, const U& val) {
    // Lists must not hold RScalar, only RComposite (e.g. vectors) and other SEXP types
    if constexpr (is<T, r_sexp>) {
      set(index, internal::as_list_element(val));
    } else {
      set(index, as<T>(val));
    }
  }

  template <RStringType U>
  void set(const U& name, const T& val) {
      set(static_cast<r_size_t>(unwrap(name_index(name))), val);
  }
  
  void set(const char* name, const T& val) {
      set(r_str(name), val);
  }

  // These overloads exist purely to avoid ambiguity between nullptr (int=0) and const char*

  template <typename U>
  void set(int index, const U& val) requires long_vectors_supported {
    set(static_cast<r_size_t>(index), val); 
  }
  T get(int index) const requires long_vectors_supported {
    return get(static_cast<r_size_t>(index)); 
  }
  T view(int index) const requires long_vectors_supported { 
    return view(static_cast<r_size_t>(index)); 
  }

  template <internal::RSubscript U>
  r_vec<T> subset(const r_vec<U>& indices, bool invert = false, bool check = true) const;

  r_vec<T> subset(r_size_t index, bool invert = false, bool check = true) const {
    return subset(r_vec<r_int64>(1, r_int64(static_cast<int64_t>(index))), invert, check);
  }

  private: 

  // Core engine: fn(index, value) -> set onto target[i]. All map/apply variants use this
  template <RVal U>
  void map_impl(r_vec<U>& target, std::invocable<r_size_t, T> auto fn, bool simd, bool parallel) const {
    r_size_t n = length();
    if (target.length() != n) [[unlikely]] {
      abort("map: target length must match source length");
    }

    if constexpr (RVectorisable<T> && RVectorisable<U>){

    int n_threads = parallel ? internal::calc_threads(n) : 1;
     
    if (simd){

      auto* p_x = data();
      auto* p_target = target.data();

      if (n_threads > 1){
        OMP_PARALLEL_FOR_SIMD(n_threads)
        for (r_size_t i = 0; i < n; ++i){
          p_target[i] = unwrap(fn(i, T(p_x[i])));
        }
      } else {
        OMP_SIMD
        for (r_size_t i = 0; i < n; ++i){
          p_target[i] = unwrap(fn(i, T(p_x[i])));
        }
      }
    } else {
      if (n_threads > 1){
        OMP_PARALLEL(n_threads)
        for (r_size_t i = 0; i < n; ++i){
          target.set(i, fn(i, view(i)));
        }
      } else {
        for (r_size_t i = 0; i < n; ++i){
          target.set(i, fn(i, view(i)));
        }
      }
    }
  } else {
    for (r_size_t i = 0; i < n; ++i){
      target.set(i, fn(i, view(i)));
    }
  }
}

  public:

  // Apply a function to each element with access to the index, modifying *this in-place: fn(index, value) -> T
  // simd - Should function be applied in an omp simd loop (via OMP_SIMD)? Only applicable for RVectorisable types
  // parallel - Should loop be exected using multiple threads? Only applicable for RVectorisable types. Threads are set via `set_threads()`
  void apply_with_index(std::invocable<r_size_t, T> auto fn, bool simd = false, bool parallel = false) {
    maybe_ensure_exclusive();
    map_impl(*this, fn, simd, parallel);
  }

  // Apply a function to each element, modifying *this in-place: fn(value) -> T
  // simd - Should function be applied in an omp simd loop (via OMP_SIMD)? Only applicable for RVectorisable types
  // parallel - Should loop be exected using multiple threads? Only applicable for RVectorisable types. Threads are set via `set_threads()`
  void apply(std::invocable<T> auto fn, bool simd = false, bool parallel = false) {
    apply_with_index([fn = std::move(fn)](r_size_t, auto v){ return fn(v); }, simd, parallel);
  }

  // From left-to-right: recursively apply a binary function to pairs of elements across *this
  // the result of each fn is used the first argument of the next call
  // use `done(x)` and `keep(x)` to break early and/or continue,  where x is the result to escape or carry forward
  template <typename Acc, typename F>
  requires std::invocable<F&, Acc, T>
  auto reduce(F fn, Acc init, bool na_skip = false, r_size_t from = 0) const {

    using ret_t = std::remove_cvref_t<std::invoke_result_t<F&, Acc, T>>;
    using acc_t = typename internal::fold_result<ret_t>::acc;
    r_size_t n = length();
    acc_t acc = as<acc_t>(init);

    for (r_size_t i = from; i < n; ++i){
      if (na_skip && is_na(view(i))){
        continue;
      }
      if constexpr (internal::fold_result<ret_t>::stops){
        auto step = fn(acc, view(i));
        acc = std::move(step.value);
        if (step.stop){
          break;
        }
      } else {
        acc = fn(acc, view(i));
      }
    }
    return acc;
  }

  // From left-to-right: recursively apply a binary function to pairs of elements across *this
  // the result of each fn is used the first argument of the next call
  // use `done(x)` and `keep(x)` to break early and/or continue,  where x is the result to escape or carry forward
  template <typename F>
  requires std::invocable<F&, T, T>
  auto reduce(F fn) const {
    if (length() == 0) [[unlikely]] {
      abort("`reduce`: cannot reduce an empty vector without an `init` starting value");
    }
    return reduce(fn, /*init = */ view(0), /*na_skip = */ false, /*from = */ 1);
  }

  template <typename Acc, typename F>
  requires std::invocable<F&, Acc, T>
  auto cumulative_reduce(F fn, Acc init, bool na_skip = false, r_size_t from = 0) const {
    using acc_t = std::remove_cvref_t<std::invoke_result_t<F&, Acc, T>>;
    static_assert(!internal::fold_result<acc_t>::stops,
                  "cumulative_reduce combiner must not short-circuit (no `done()`)");
    r_vec<acc_t> out(length() - from);
    r_size_t k = 0;
    reduce([&](const acc_t& acc, T x){
      acc_t next = (na_skip && is_na(x)) ? acc : fn(acc, x);
      out.set(k++, next);
      return next;
    }, as<acc_t>(init), /*na_skip=*/false, from);
    return out;
  }

  template <typename F>
  requires std::invocable<F&, T, T>
  auto cumulative_reduce(F fn, bool na_skip = false) const {
    using acc_t = std::remove_cvref_t<std::invoke_result_t<F&, T, T>>;
    r_size_t n = length();
    if (n < 2){
      return as<r_vec<acc_t>>(*this);
    }
    r_vec<acc_t> out(n);
    acc_t acc = as<acc_t>(view(0));
    out.set(r_size_t{0}, acc);
    for (r_size_t i = 1; i < n; ++i){
      if (na_skip && is_na(view(i))){
        out.set(i, acc);
        continue;
      }
      acc = fn(acc, view(i));
      out.set(i, acc);
    }
    return out;
  }

  // Count the number of NAs in a vector
  // It is marked noexcept because all `is_na()` functions are noexcept and there are no R vector allocations
  r_size_t na_count() const noexcept {
    r_size_t out = 0;
    r_size_t n = length();
    
    if constexpr (RVectorisable<T>){

      const auto* RESTRICT p = data();
      int n_threads = internal::calc_threads(n);

      if (n_threads > 1){
        OMP_PARALLEL_FOR_SIMD_REDUCTION1(n_threads, +:out)
        for (r_size_t i = 0; i < n; ++i){
          out += static_cast<r_size_t>(is_na(T(p[i])));
        }
      } else {
        OMP_SIMD_REDUCTION1(+:out)
        for (r_size_t i = 0; i < n; ++i){
          out += static_cast<r_size_t>(is_na(T(p[i])));
        }
      }

    } else {
      for (r_size_t i = 0; i < n; ++i){
        out += static_cast<r_size_t>(is_na(view(i)));
      }
    }
    return out;
  }

  r_size_t count(const T& val) const {
    r_size_t out = 0;
    r_size_t n = length();

    if constexpr (RVectorisable<T>){
      const auto* RESTRICT p = data();
      OMP_SIMD_REDUCTION1(+:out)
      for (r_size_t i = 0; i < n; ++i){
        out += identical(T(p[i]), val);
      }
    } else {
      for (r_size_t i = 0; i < n; ++i){
        out += identical(view(i), val);
      }
    }
    return out;
  }
  r_vec<T> remove(const T& val) const {
    r_size_t n_remove = count(val);
    r_size_t n = length();

    if (n_remove == 0){
      return *this;
    } else if (n_remove == n){
      return r_vec<T>();
    } else {
      r_vec<T> out(n - n_remove);
      r_size_t k = 0;

      for (r_size_t i = 0; i < n; ++i){
        if (!identical(view(i), val)){
          out.set(k++, view(i));
        }
      }
      return out;
    }
  }

  // locations of value in vector
  template <internal::RNumericSubscript V = r_int>
  r_vec<V> find(const T& val, bool invert = false) const {

    r_size_t n = length();

    using int_t = unwrap_t<V>;
  
    if constexpr (is<V, r_int>){
      if ( (n > r_limits<r_int>::max()).is_true()){
        abort("`x` is a long vector, please use `find<r_int64>` instead");
      }
    }
  
    r_size_t n_vals = count(val);
    int_t whichi = 0; 
    int_t i = 0; 
  
    if (invert){
      int_t out_size = n - n_vals;
      r_vec<V> out(out_size);
      while (whichi < out_size){
          out.set(whichi, V(i));
          whichi += static_cast<int_t>(!identical(view(static_cast<r_size_t>(i++)), val));
      }
      return out;
    } else {
      int_t out_size = n_vals;
      r_vec<V> out(out_size);
      while (whichi < out_size){
        out.set(whichi, V(i));
        whichi += static_cast<int_t>(identical(view(static_cast<r_size_t>(i++)), val));
    }
    return out;
    }
  }

  // Sequential fill
  void fill(r_size_t start, r_size_t n, const T& val){
    
    if constexpr (RVectorisable<T>){
      
      maybe_ensure_exclusive();

      int n_threads = internal::calc_threads(n);
      auto* RESTRICT p_target = data();
      unwrap_t<T> v = unwrap(val);
      if (n_threads > 1) {
        OMP_PARALLEL_FOR_SIMD(n_threads)
        for (r_size_t i = 0; i < n; ++i) {
          p_target[start + i] = v;
        }
      } else {
        std::fill_n(p_target + start, n, v);
      }
    } else {
      for (r_size_t i = 0; i < n; ++i) {
        set(start + i, val);
      }
    }
  }

  // Fill entire vector with value
  void fill(const T& val){
    fill(0, length(), val);
  }

  void replace(r_size_t start, r_size_t n, const T& old_val, const T& new_val){
    for (r_size_t i = 0; i < n; ++i) {
      r_size_t idx = start + i;
      if (identical(view(idx), old_val)){
        set(idx, new_val);
      }
    }
  }

  // Replace all occurrences of old_val with new_val
  void replace(const T& old_val, const T& new_val){
    replace(0, length(), old_val, new_val);
  }

  r_vec<T> resize(r_size_t n) const {
    r_size_t vec_size = length();
    if (n == vec_size || is_null()){
      return *this;
    } else {
      r_vec<T> resized_vec(n);
      r_size_t n_to_copy = std::min(n, vec_size);

      if constexpr (RVectorisable<T>){
        std::copy_n(this->data(), n_to_copy, resized_vec.data());
      } else {
        for (r_size_t i = 0; i < n_to_copy; ++i){
          resized_vec.set(i, view(i)); 
        }
      }
      resized_vec.set_names(names().resize(n));
      return resized_vec;
    }
  }

  r_vec<T> rep_len(r_size_t n) const {

    r_size_t size = length();

    if (size == n || is_null()){
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
    out.set_names(names().rep_len(n));
    return out;
  }

  // Reverse vector in-place
  void rev() {
    if (is_null()){
      return;
    }
    r_size_t n = length();
    r_size_t n1 = n / 2;
    r_size_t n2 = n - 1;
    for (r_size_t i = 0; i < n1; ++i){
      r_size_t k = n2 - i;
      T left = view(i);
      set(i, view(k));
      set(k, left);
    }
    r_vec<r_str_view> nms = names();
    nms.rev();
    set_names(nms);
  }

  // In-place shift: k > 0 lags (shifts right), k < 0 leads (shifts left)
  void shift(r_size_t k = 1, const T& fill_value = na<T>()) {

    r_size_t n = length();

    if (n == 0 || k == 0) {
      return;
    }

    r_size_t ak = std::abs(k);
    if (ak >= n) {
      fill(fill_value);
      return;
    }
    if (k > 0) {
      for (r_size_t i = n - 1; i >= k; --i) {
        set(i, view(i - k));
      }
      for (r_size_t i = 0; i < ak; ++i) {
        set(i, fill_value);
      }
    } else {
      for (r_size_t i = 0; i < n - ak; ++i) {
        set(i, view(i + ak));
      }
      for (r_size_t i = n - ak; i < n; ++i) {
        set(i, fill_value);
      }
    }
    r_vec<r_str_view> nms = names();
    nms.shift(k, r_str_view());
    set_names(nms);
  }

  void iota(T init = T(0)) requires (any<T, r_int, r_int64>) {
    apply_with_index([init](r_size_t i, auto){ return init + i; }, /*simd = */ true);
  }

  // Conditional member functions (only available for certain types)

  // POSIXct-only members
  r_str tzone() const requires RPsxctType<T> {
    r_vec<r_str_view> tz(Rf_getAttrib(value, cached_sym<"tzone">()));
    
    if (tz.length() == 0){
      abort("`r_vec<r_psxct_t>` vector must have a valid tzone attribute");
    } else {
      r_str_view tz_str = tz.view(0);
      if (is_na(tz_str)){
        abort("tzone cannot be NA");
      }
      return r_str(tz_str);
    }
  }

  void set_tzone(const char* tz) requires RPsxctType<T> {
    maybe_ensure_exclusive();
    safe[Rf_setAttrib](value, cached_sym<"tzone">(), r_vec<r_str>(1, r_str(tz)));
  }

  // list-only members

  r_vec<r_int> lengths() const requires is<T, r_sexp> {
    r_size_t n = length();
    r_vec<r_int> out(n);
    out.set_names(names());
  
    for (r_size_t i = 0; i < n; ++i){
      r_size_t len = cppally::length(view(i));
      if (len > unwrap(r_limits<r_int>::max())) [[unlikely]] {
        abort("`lengths()` does not currently support long-vectors");
      }
      out.set(i, r_int(static_cast<int>(len)));
    }
    return out;
  }

};

// Alias of r_vec
template <RVal T>
using r_vector = r_vec<T>;

template <RVector T>
inline void r_copy_n(T& target, const T& source, r_size_t target_offset, r_size_t n){

  using data_t = typename T::data_type;

  target.maybe_ensure_exclusive();

  if constexpr (!RObject<data_t>){

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
  } else if constexpr (RStringType<data_t>){

    // Cast const SEXP* to SEXP* and write directly
    auto* p_target = const_cast<unwrap_t<data_t>*>(target.data());
    std::copy_n(source.data(), n, p_target + target_offset);
  } else {
    for (r_size_t i = 0; i < n; ++i) {
      target.set(target_offset + i, source.view(i));
    }
  }
}

namespace internal {

// Transplant a populated names cache from source to target. Intended for
// shallow-copy paths where the new SEXP shares the names STRSXP with source —
// the existing hash is still valid and rebuilding would be wasteful at high
// column counts. No-op if source has no populated cache or if target's names
// attribute differs from source's cached names.
//
// Shares only the inner sexp_index_table, not the enclosing names_map. Each
// wrapper keeps its own names_map so that a future set_names() (or any
// invalidation path) on one cannot poison the other's view.
template <typename V>
inline void share_name_cache(V& target, const V& source) {
    if (!source.cached_names) return;
    if (!source.cached_names->names.has_value()) return;
    SEXP target_names = Rf_protect(Rf_getAttrib(target, symbol::names_sym));
    if (target_names != static_cast<SEXP>(*source.cached_names->names)){
        Rf_unprotect(1);
        return;
    }
    target.ensure_names_cached();
    target.cached_names->map = source.cached_names->map;
    Rf_unprotect(1);
}

}

}

#endif
