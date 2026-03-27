#ifndef CPP20_R_TYPES_H
#define CPP20_R_TYPES_H

#include <cpp20/r_setup.h>
#include <cpp20/protect.hpp>
#include <cstddef>  // for size_t
#include <cpp20/r_concepts.h>
#include <chrono> // For r_date/r_psxt
#include <limits>

// R-based C++ types that closely align with their R equivalents
// Further methods (e.g. operators) are defined in r_methods.h
// Please note that constructing R types via e.g. r_dbl() r_int() does not account for NAs
// For any and all conversions, use the `as<>` template defined in r_coerce.h
// For example - to construct the integer 0, simply write r_int(0) or as<r_int>(0), 
// the latter being able to handle NA conversions between different types
// `as<>` is the de-facto tool for conversions between all types in cpp20

namespace cpp20 {


namespace internal {

// Collection of run-time SEXP type helpers
// This is necessary for registering C++ functions between the C++/R boundary
// To do this we create a run-time type ID (using `TYPEPOF()`) and add 
// custom values for objects we want to differentiate from their storage type
// For example, dates are internally REALSXP but we need a unique ID for dates
// Since dates can be either integer or numeric storage, we create 2 unique tags, INTDATESXP and REALDATESXP

// Custom SEXP tags, differentiating integer64, dates (int/double), date-times (int64/double) and factors
inline constexpr SEXPTYPE CPP20_INT64SXP = 64;
inline constexpr SEXPTYPE CPP20_INTDATESXP = 200;
inline constexpr SEXPTYPE CPP20_REALDATESXP = 201;
inline constexpr SEXPTYPE CPP20_INT64PSXTSXP = 202;
inline constexpr SEXPTYPE CPP20_REALPSXTSXP = 203;
inline constexpr SEXPTYPE CPP20_FCTSXP = 204;
inline constexpr SEXPTYPE CPP20_DFSXP = 205;

inline SEXPTYPE CPP20_TYPEOF(SEXP x) noexcept {

  auto xtype = TYPEOF(x);

  switch (xtype){
    case INTSXP: {
      if (!Rf_isObject(x)) return xtype;
      if (Rf_inherits(x, "Date")) return CPP20_INTDATESXP;
      if (Rf_inherits(x, "factor")) return CPP20_FCTSXP;
      return xtype;
    }
    case REALSXP: {
      if (!Rf_isObject(x)) return xtype;
      if (Rf_inherits(x, "Date")) return CPP20_REALDATESXP;
      if (Rf_inherits(x, "POSIXct") && Rf_inherits(x, "integer64")) return CPP20_INT64PSXTSXP;
      if (Rf_inherits(x, "POSIXct")) return CPP20_REALPSXTSXP;
      if (Rf_inherits(x, "integer64")) return CPP20_INT64SXP; 
      return xtype;
    }
    default: {
      return xtype;
    }
  }
}

inline const char* r_type_to_str(SEXPTYPE x){

  switch (x){
    case CPP20_INT64SXP: return "CPP20_INT64SXP";
    case CPP20_INTDATESXP: return "CPP20_INTDATESXP";
    case CPP20_REALDATESXP: return "CPP20_REALDATESXP";
    case CPP20_INT64PSXTSXP: return "CPP20_INT64PSXTSXP";
    case CPP20_REALPSXTSXP: return "CPP20_REALPSXTSXP";
    case CPP20_FCTSXP: return "CPP20_FCTSXP";
    case CPP20_DFSXP: return "CPP20_DFSXP";
    default: return Rf_type2char(x);
  }
}

template <typename T>
inline const char* type_str() {
    return "Unknown";
}

template <> inline const char* type_str<r_lgl>(){return "r_lgl";}
template <> inline const char* type_str<r_int>(){return "r_int";}
template <> inline const char* type_str<r_int64>(){return "r_int64";}
template <> inline const char* type_str<r_dbl>(){return "r_dbl";}
template <> inline const char* type_str<r_str>(){return "r_str";}
template <> inline const char* type_str<r_str_view>(){return "r_str_view";}
template <> inline const char* type_str<r_cplx>(){return "r_cplx";}
template <> inline const char* type_str<r_raw>(){return "r_raw";}
template <> inline const char* type_str<r_sym>(){return "r_sym";}
template <> inline const char* type_str<r_sexp>(){return "r_sexp";}
template <> inline const char* type_str<r_date_t<r_int>>(){return "r_date_t<r_int>";}
template <> inline const char* type_str<r_date_t<r_dbl>>(){return "r_date_t<r_dbl>";}
template <> inline const char* type_str<r_psxct_t<r_int64>>(){return "r_psxct_t<r_int64>";}
template <> inline const char* type_str<r_psxct_t<r_dbl>>(){return "r_psxct_t<r_dbl>";}
template <> inline const char* type_str<r_factors>(){return "r_factors";}

template<RVector T>
inline const char* type_str(){
    using r_t = typename T::data_type;
    static const std::string out = std::string("r_vec<") + type_str<r_t>() + ">";
    return out.c_str();
}

template<CppFloatType T> 
inline const char* type_str(){
    return "C++ float";
}
template<CppIntegerType T>
inline const char* type_str(){
    return "C/C++ integer";
}
template<> 
inline const char* type_str<const char*>(){
    return "C string";
}
template<>
inline const char* type_str<std::string>(){
    return "C++ string";
}
template<CppComplexType T> 
inline const char* type_str(){
    return "C++ complex";
}

// Mapping from C++ type to R TYPEOF

template <typename T> constexpr uint16_t r_typeof_impl =              std::numeric_limits<uint16_t>::max();
template<> constexpr uint16_t r_typeof_impl<r_vec<r_lgl>> =          LGLSXP;
template<> constexpr uint16_t r_typeof_impl<r_vec<r_int>> =          INTSXP;
template<> constexpr uint16_t r_typeof_impl<r_vec<r_dbl>> =          REALSXP;
template<> constexpr uint16_t r_typeof_impl<r_vec<r_str_view>> =     STRSXP;
template<> constexpr uint16_t r_typeof_impl<r_vec<r_str>> =          STRSXP;
template<> constexpr uint16_t r_typeof_impl<r_vec<r_cplx>> =         CPLXSXP;
template<> constexpr uint16_t r_typeof_impl<r_vec<r_raw>> =          RAWSXP;
template<RListVector T> constexpr uint16_t r_typeof_impl<T> =        VECSXP;
template<> constexpr uint16_t r_typeof_impl<r_str_view> =            CHARSXP;
template<> constexpr uint16_t r_typeof_impl<r_str> =                 CHARSXP;
template<> constexpr uint16_t r_typeof_impl<r_sym> =                 SYMSXP;
template<> constexpr uint16_t r_typeof_impl<r_vec<r_int64>> =             REALSXP;
template<> constexpr uint16_t r_typeof_impl<r_vec<r_date_t<r_int>>> =            INTSXP;
template<> constexpr uint16_t r_typeof_impl<r_vec<r_date_t<r_dbl>>> =            REALSXP;
template<> constexpr uint16_t r_typeof_impl<r_vec<r_psxct_t<r_int64>>> =         REALSXP;
template<> constexpr uint16_t r_typeof_impl<r_vec<r_psxct_t<r_dbl>>> =           REALSXP;


// The above mappings represent the plain TYPEOF values of cpp20 objects, this enables r_vec<T> to check the primitive type id during construction
// without rejecting objects such as `r_factors`
// The below represents the actual cpp20 type id mapping
template <typename T> constexpr uint16_t r_typeof =              r_typeof_impl<T>;
template<> constexpr uint16_t r_typeof<r_vec<r_int64>> =        CPP20_INT64SXP;
template<> constexpr uint16_t r_typeof<r_vec<r_date_t<r_int>>> =            CPP20_INTDATESXP;
template<> constexpr uint16_t r_typeof<r_vec<r_date_t<r_dbl>>> =            CPP20_REALDATESXP;
template<> constexpr uint16_t r_typeof<r_vec<r_psxct_t<r_int64>>> =         CPP20_INT64PSXTSXP;
template<> constexpr uint16_t r_typeof<r_vec<r_psxct_t<r_dbl>>> =           CPP20_REALPSXTSXP;
template<> constexpr uint16_t r_typeof<r_factors> =             CPP20_FCTSXP;

// Low-level type ID check, primarily used in constructing classed cpp20 objects from SEXP
template <typename T>
inline void check_valid_construction(SEXP x){
    if (r_typeof_impl<T> != TYPEOF(x)){
        abort("Bad construction from R type %s to C++ type %s", Rf_type2char(TYPEOF(x)), type_str<T>());
    }
}

// Helper struct to allow for overloading SEXP-based constructors without re-protecting them via cpp11::sexp
struct view_tag {};

}

// ----- Start of C++ R types -----

// General SEXP, reserved for everything except CHARSXP and SYMSXP
// Wrapper around cpp11::sexp to benefit from automatic protection (cpp11-managed linked list)
// All credits go to cpp11 authors/maintainers for `cpp11::sexp`
struct r_sexp {

  public:

  SEXP value = R_NilValue;
  using value_type = SEXP;

  private:

  SEXP preserve_token_ = R_NilValue;

  public: 

  r_sexp() = default;
  r_sexp(SEXP data) : value(data), preserve_token_(detail::store::insert(value)) {}

  // We maintain our own new `preserve_token_`
  r_sexp(const r_sexp& rhs) : value(rhs.value), preserve_token_(detail::store::insert(rhs.value)) {}

  // We take ownership over the `rhs.preserve_token_`.
  // Importantly we clear it in the `rhs` so it can't release the object upon destruction.
  r_sexp(r_sexp&& rhs) noexcept : value(rhs.value), preserve_token_(rhs.preserve_token_) {
    rhs.value = R_NilValue;
    rhs.preserve_token_ = R_NilValue;
  }

  r_sexp& operator=(const r_sexp& rhs) noexcept {

    if (this != &rhs) {
      detail::store::release(preserve_token_);
  
      value = rhs.value;
      preserve_token_ = detail::store::insert(value);
    }
      

    return *this;
  }

  r_sexp& operator=(r_sexp&& rhs) noexcept {
    if (this != &rhs) {
      detail::store::release(preserve_token_);
      value = rhs.value;
      
      // Steal the token, do not create a new one
      preserve_token_ = rhs.preserve_token_;
      
      rhs.value = R_NilValue;
      rhs.preserve_token_ = R_NilValue;
    }
    return *this;
  }

  ~r_sexp() { detail::store::release(preserve_token_); }

  // Implicit conversion to SEXP
  constexpr operator SEXP() const noexcept { return value; }
  constexpr SEXP data() const noexcept { return value; }

  // Optimized constructor
  // convert SEXP -> r_sexp directly without extra protection
  explicit r_sexp(SEXP s, internal::view_tag) : value(s) {}

  r_size_t length() const noexcept {
    return Rf_xlength(value);
  }

  r_size_t size() const noexcept {
    return length();
  }

  bool is_null() const { return value == R_NilValue; }
  
  r_str address() const;
};

// bool type, similar to Rboolean
// Can only implicitly convert to bool in if statements
// If during implicit conversion, an NA is detected, an error is thrown
// Detect NA manually via the `is_na` member function
struct r_lgl {
  int value;
  using value_type = int;
  r_lgl() : value{0} {}
  explicit constexpr r_lgl(int x) : value{x} {}
  explicit constexpr r_lgl(bool x) : value{x} {}  
  explicit constexpr operator int() const { return value; }

  explicit operator bool() const;
  constexpr bool is_true() const;
  constexpr bool is_false() const;
  constexpr bool is_na() const;
};

// The 3 possible values of r_lgl
inline constexpr r_lgl r_true{1};
inline constexpr r_lgl r_false{0};
inline constexpr r_lgl r_na{std::numeric_limits<int>::min()};

  inline constexpr bool r_lgl::is_true() const {
    return value == 1;
  }

  inline constexpr bool r_lgl::is_false() const {
    return value == 0;
  }

  inline constexpr bool r_lgl::is_na() const {
    return value == r_na.value;
  }

  inline r_lgl::operator bool() const {
    if (is_na()){
    abort("Cannot implicitly convert r_lgl NA to bool, please check");
    }
    return static_cast<bool>(value);
  }

// is_na is defined later as a template

// R integer
struct r_int {
  int value;
  using value_type = int;
  r_int() : value{0} {}
  template <CppMathType T>
  requires (internal::can_definitely_be_int<T>())
  explicit constexpr r_int(T x) : value{static_cast<int>(x)} {}
  constexpr operator int() const { return value; }
};
// R double
struct r_dbl {
  double value;
  using value_type = double;
  r_dbl() : value{0} {}
  template <CppMathType T>
  explicit constexpr r_dbl(T x) : value{static_cast<double>(x)} {}
  constexpr operator double() const { return value; }
};
// R integer64 (closely mimicking how bit64 defines it)
struct r_int64 {
  int64_t value;
  using value_type = int64_t;
  r_int64() : value{0} {}
  template <CppMathType T>
  requires (internal::can_definitely_be_int64<T>())
  explicit constexpr r_int64(T x) : value{static_cast<int64_t>(x)} {}
  constexpr operator int64_t() const { return value; }
};

// Alias type for CHARSXP
// r_str must never be converted to `SEXP`/`r_sexp`
// all templates assume that `SEXP`/`r_sexp` is reserved for objects that can safely fit into an R list vector
// Furthermore CHARSXP is a special case because it is essentially the only SEXP that already fits into a non-list vector: a character vector
struct r_str {
  r_sexp value;
  using value_type = r_sexp;
  // r_str() : value{internal::blank_string_constant, internal::view_tag{}} {}
  r_str() : value{R_BlankString, internal::view_tag{}} {}
  // Explicit SEXP/const char* -> r_str
  explicit r_str(SEXP x) : value{x} {
    internal::check_valid_construction<r_str>(value);
  }
  explicit r_str(SEXP x, internal::view_tag) : value(x, internal::view_tag{}) {
    internal::check_valid_construction<r_str>(value);
  }
  explicit r_str(r_sexp x) : value(std::move(x)) {
    internal::check_valid_construction<r_str>(value);
  }
  explicit r_str(const char *x) : value(Rf_mkCharCE(x, CE_UTF8)) {}
  // Implicit r_str -> SEXP 
  constexpr operator SEXP() const noexcept { return value; }

  // Explicit r_str_view -> r_str
  explicit r_str(r_str_view x);

  const char *c_str() const {
    return CHAR(value);
  }

  std::string_view cpp_str() const {
    return std::string_view{c_str()};
  }
  
  // Explicit conversions
  explicit operator const char*() const { return c_str(); }
  explicit operator std::string_view() const { return cpp_str(); }
};

// Unsafe (but fast) r_str type
// Similar to std::string_view, it is a view of an r_str/CHARSXP whose lifetime must be shorter than the object it's viewing
struct r_str_view {
  SEXP value; 
  using value_type = SEXP;

  // Constructors
  r_str_view() : value{R_BlankString} {}
  explicit r_str_view(SEXP x) : value{x} {
    internal::check_valid_construction<r_str_view>(value);
  }
  explicit r_str_view(SEXP x, internal::view_tag) : value(x) {
    internal::check_valid_construction<r_str_view>(value);
  }
  // explicit r_str_view(const char *x) : value(Rf_mkCharCE(x, CE_UTF8)) {}
  // Implicit r_str_view -> SEXP
  constexpr operator SEXP() const noexcept { return value; }
  
  // Implicit r_str -> r_str_view
  r_str_view(const r_str& x) : value(static_cast<SEXP>(x)) {}
  
  const char* c_str() const { return CHAR(value); }
  std::string_view cpp_str() const noexcept { return std::string_view{c_str()}; }


  // Explicit conversions
  explicit operator const char*() const { return c_str(); }
  explicit operator std::string_view() const { return cpp_str(); }
};

inline r_str::r_str(r_str_view x) : value(static_cast<SEXP>(x)) {}

// Alias type for SYMSXP
struct r_sym {
  SEXP value;
  using value_type = r_sexp;

  r_sym() : value{R_MissingArg} {}
  explicit r_sym(SEXP x) : value{x} {
    internal::check_valid_construction<r_sym>(value);
  }
  explicit r_sym(SEXP x, internal::view_tag) : value(x) {
    internal::check_valid_construction<r_sym>(value);
  }
  explicit r_sym(const char *x) : value(Rf_installChar(Rf_mkCharCE(x, CE_UTF8))) {}
  constexpr operator SEXP() const noexcept { return value; }
};


// Complex number - uses std::complex<double> under the hood
struct r_cplx {
  std::complex<double> value;
  using value_type = std::complex<double>;

  // Constructors
  constexpr r_cplx() : value{0.0, 0.0} {}
  constexpr r_cplx(r_dbl r, r_dbl i) : value{r, i} {}
  
  // Conversion handling
  explicit constexpr r_cplx(std::complex<double> x) : value{x} {}
  constexpr operator std::complex<double>() const { return value; }

  // Get real and imaginary parts
  constexpr r_dbl re() const { return r_dbl{value.real()}; }
  constexpr r_dbl im() const { return r_dbl{value.imag()}; }
};

// Alias type for Rbyte
// In the future we will use std::byte
struct r_raw {
  unsigned char value;
  using value_type = unsigned char;

  // Constructors
  constexpr r_raw() : value{static_cast<unsigned char>(0)} {}

  // Conversion handling
  explicit constexpr r_raw(unsigned char x) : value{x} {}
  constexpr operator unsigned char() const { return value; }
};

inline r_str r_sexp::address() const {
  char buf[1000];
  std::snprintf(buf, 1000, "%p", static_cast<void*>(value));
  return r_str(buf);
}


namespace internal {
  // Construct r_date from year/month/day
  inline int64_t get_days_since_epoch(int32_t year, uint32_t month, uint32_t day) {
      namespace chrono = std::chrono;
      auto ymd = chrono::year{year} / chrono::month{month} / chrono::day{day};
      if (!ymd.ok()) {
          abort("Invalid date: %d-%u-%u", year, month, day);
      }
      return chrono::sys_days{ymd}.time_since_epoch().count();
  }

  inline int64_t get_seconds_since_epoch(int32_t year, uint32_t month, uint32_t day, uint64_t hour, uint64_t min, uint64_t sec) {
    namespace chrono = std::chrono;
    auto ymd = chrono::year{year} / chrono::month{month} / chrono::day{day};
    if (!ymd.ok()) {
        abort("Invalid date: %d-%u-%u", year, month, day);
    }
    auto tp = chrono::sys_days{ymd} + chrono::hours{hour} + chrono::minutes{min} + chrono::seconds{sec};
    return tp.time_since_epoch().count();
  }
}
// R date that captures the number of days since epoch (1st Jan 1970)
template <typename T>
requires (any<T, r_int, r_dbl>)
struct r_date_t : T {

  using inherited_type = T;
  
  private: 

  auto chrono_ymd() const {
    return std::chrono::year_month_day{
      std::chrono::sys_days{std::chrono::days{static_cast<int32_t>(T::value)}}
    };
  }

  public: 

  r_date_t() : T{0} {}
  template <CppMathType U>
  explicit constexpr r_date_t(U days_since_epoch) : T{days_since_epoch} {}
  explicit constexpr r_date_t(T days_since_epoch) : T{days_since_epoch} {}

  // Construct r_date year/month/day
  explicit r_date_t(int32_t year, uint32_t month, uint32_t day) : T(internal::get_days_since_epoch(year, month, day)) {}

  r_str date_str() const {
    auto ymd = chrono_ymd();
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%04d-%02u-%02u", static_cast<int32_t>(ymd.year()), static_cast<uint32_t>(ymd.month()), static_cast<uint32_t>(ymd.day()));
    return r_str(static_cast<const char*>(buf));
  }
};

// R date-time that captures the number of seconds since epoch (1st Jan 1970)
template <typename T>
requires (any<T, r_int64, r_dbl>)
struct r_psxct_t : T {

  using inherited_type = T;

  r_psxct_t() : T{0} {}
  template <CppMathType U>
  explicit constexpr r_psxct_t(U seconds_since_epoch) : T{seconds_since_epoch} {}
  explicit constexpr r_psxct_t(T seconds_since_epoch) : T{seconds_since_epoch} {}

  // Construct r_date year/month/day
  explicit r_psxct_t(
    int32_t year, uint32_t month, uint32_t day, 
    uint32_t hour, uint32_t minute, uint32_t second
  ) : T(internal::get_seconds_since_epoch(year, month, day, hour, minute, second)) {}

  private: 
  
  auto chrono_tp() const {
    return std::chrono::time_point{
      std::chrono::sys_seconds{std::chrono::seconds{static_cast<int64_t>(T::value)}}
    };
  }

  // Decomposed date + time-of-day
  auto chrono_ymd() const {
    using namespace std::chrono;
    auto tp = chrono_tp();
    auto dp = floor<days>(tp);
    return year_month_day{dp};
  }

  auto chrono_hms() const {
    using namespace std::chrono;
    auto tp = chrono_tp();
    auto dp = floor<days>(tp);
    return hh_mm_ss{tp - dp};
  }

  public: 

  r_str datetime_str() const {
    auto ymd = chrono_ymd();
    auto hms = chrono_hms();
    char buf[20];
    std::snprintf(buf, sizeof(buf),
      "%04d-%02u-%02u %02u:%02u:%02u",
      static_cast<int32_t>(ymd.year()),
      static_cast<uint32_t>(ymd.month()),
      static_cast<uint32_t>(ymd.day()),
      static_cast<uint32_t>(hms.hours().count()),
      static_cast<uint32_t>(hms.minutes().count()),
      static_cast<uint32_t>(hms.seconds().count())
    );
    return r_str(static_cast<const char*>(buf));
  }
};


// Important (recursive) helper to extract the underlying NON-RVal value
// Recursively unwrap until we hit a primitive type
template <typename T>
inline constexpr auto unwrap(const T& x){
if constexpr (RVal<T>){
    return unwrap(x.value);
  } else if constexpr (RObject<T>){
    return static_cast<SEXP>(x);
  } else {
    return x;
  }
}

// Constants

// R C NULL constant
inline const r_sexp r_null = r_sexp();
// Blank string ''
inline const r_str_view blank_r_string = r_str_view();
inline const r_str_view na_str = r_str_view(NA_STRING);

// // Lazy loaded version
// // R C NULL constant
// inline const r_sexp& r_null() { static const r_sexp s; return s; }
// // Blank string ''
// inline const r_str_view& r_blank_string() { static const r_str_view s; return s; };
  
// Coerce to an R type based on the C type (useful for RVal templates)
template<typename T>
inline constexpr auto as_r_val(T const& x) { 
  if constexpr (RVal<T>){
    return x;
  } else if constexpr (CastableToRVal<T>){
    return static_cast<as_r_val_t<T>>(x);
  } else {
    static_assert(
      always_false<T>,
      "Unsupported type for `as_r_val`"
    );
    return r_null;
  } 
}

template<typename T>
inline constexpr auto as_r_scalar(T const& x) {
  if constexpr (RVector<T>){
    if (x.length() != 1){
      abort("Vector must be length-1 to be coerced to a scalar");
    }
    auto out = x.get(0);
    
    // Only happens if x is a list
    if (!RScalar<decltype(out)>){
      abort("`x` cannot be coerced to a scalar, first list-element is not a scalar");
    }
    return out;
  }
  else {
    return as_r_val(x);
  } 
}

}

#endif
