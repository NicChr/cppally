#ifndef CPP20_R_CONCEPTS_H
#define CPP20_R_CONCEPTS_H

#include <cpp20/internal/r_setup.h>

namespace cpp20 {

// Forward declare structs to enable defining concepts now
struct r_lgl;
struct r_int;
struct r_int64;
struct r_dbl;
struct r_str;
struct r_str_view;
struct r_cplx;
struct r_raw;
struct r_sym;
struct r_sexp; 

template <class... T>
inline constexpr bool always_false = false;

template <class... T>
inline constexpr bool always_true = true;

// Compile-time type check `is<>`
template <typename T, typename U>
inline constexpr bool is = std::same_as<std::remove_cvref_t<T>, std::remove_cvref_t<U>>;

template <typename T, typename... Args>
inline constexpr bool any = (is<T, Args> || ...);

// Concepts to enable R type templates

template <typename T>
concept RIntegerType = any<T, r_lgl, r_int, r_int64>;

template <typename T>
concept CppIntegerType = std::is_integral_v<std::remove_cvref_t<T>> && !any<T, Rboolean, Rbyte>;

template <typename T>
concept IntegerType = RIntegerType<T> || CppIntegerType<T>;

template <typename T>
concept RFloatType = is<T, r_dbl>;

template <typename T>
concept CppFloatType = std::is_floating_point_v<std::remove_cvref_t<T>>;

template <typename T>
concept FloatType = RFloatType<T> || CppFloatType<T>;

namespace internal {

template <typename T>
struct is_cpp_complex : std::false_type {};

template <typename U>
struct is_cpp_complex<std::complex<U>> : std::true_type {};

}

template <typename T>
concept RComplexType = is<T, r_cplx>;

template <typename T>
concept CppComplexType = internal::is_cpp_complex<std::remove_cvref_t<T>>::value;

template <typename T>
concept ComplexType = RComplexType<T> || CppComplexType<T>;

template <typename T>
concept RMathType = RIntegerType<T> || RFloatType<T>;

template <typename T>
concept CppMathType = std::is_arithmetic_v<std::remove_cvref_t<T>>;

template <typename T>
concept MathType = RMathType<T> || CppMathType<T>;

template <typename T>
concept RStringType = any<T, r_str, r_str_view>;

template <typename T, typename U>
concept AtLeastOneRMathType =
(RMathType<T> || RMathType<U>) && (MathType<T> && MathType<U>);

template <typename T>
concept RSymbolType = is<T, r_sym>;

template <typename T>
concept RRawType = is<T, r_raw>;

template <typename T>
concept RAtomicScalar = RMathType<T> || any<T, r_cplx, r_str, r_str_view, r_raw>;

template <typename T>
concept RScalar = RAtomicScalar<T> || is<T, r_sym>;

// RVal is anything that can be stored in `r_vec<>`
template <typename T>
concept RVal = RScalar<T> || is<T, r_sexp>;


namespace internal {
// A `SEXP` which we can write data to directly via a pointer
template <typename T>
concept RPtrWritableType = RMathType<T> || any<T, r_cplx, r_raw>;
}

// Forward declare structs to define concepts now
template<RVal T>
struct r_vec;

struct r_dates; // Inherits from r_vec<r_int>
struct r_posixcts; // Inherits from r_vec<r_dbl>
struct r_factors;
struct r_df;

namespace internal {

template <typename T>
struct is_r_vector : std::false_type {};

template <typename T>
struct is_r_vector<r_vec<T>> : std::true_type {};

template <typename T>
inline constexpr bool is_r_vector_v = is_r_vector<std::remove_cvref_t<T>>::value;

template <typename T>
struct is_atomic_r_vector : std::false_type {};

template <RAtomicScalar T>
struct is_atomic_r_vector<r_vec<T>> : std::true_type {};

template <typename T>
inline constexpr bool is_atomic_r_vector_v = is_atomic_r_vector<std::remove_cvref_t<T>>::value;

}

template <typename T>
concept RAtomicVector = internal::is_atomic_r_vector_v<T>;

template <typename T>
concept RVector = internal::is_r_vector_v<T> || any<T, r_dates, r_posixcts, r_factors>;

template <typename T>
concept RFactor = is<T, r_factors>;

// RObject is any object that can be represented in R - it excludes internal R types like CHARSXP
// Also, these are all implicitly convertible to `SEXP`
template <typename T> 
concept RObject = std::is_convertible_v<T, SEXP>;

template <typename T> 
concept RType = RVal<T> || RObject<T>;

template <typename T>
concept CppType = !RType<T>;

template <typename T>
concept CppScalar = std::is_scalar_v<T>;

template <typename T, typename U>
concept AtLeastOneRVal = 
(RVal<T> && RVal<U>) ||
(RVal<T> && CppScalar<U>) ||
(CppScalar<T> && RVal<U>);

template <typename T>
concept Scalar = CppScalar<T> || RScalar<T>;

template <typename T>
inline constexpr bool is_sexp = any<T, SEXP, r_sexp>;

template <typename T>
concept RSortable = RMathType<T> || RStringType<T>;


// Wanted to use this as arg in templates but template type deduction then doesn't work (SAD)
// template <typename T>
// using arg_t = std::conditional_t<MathType<T> || is<T, const char*>, T, const T&>;

// The below smalltype/largetype method is likely a bad idea as it would mean having to duplicate a lot of code with template overloads
// If T is a small type (e.g. int/r_int) then pass-by-value
// If it's a large type (e.g. r_str_view) then pass-by-reference
// This avoids the copy overhead for large types and passes small types in CPU registers
// template <typename T>
// concept SmallType = CppScalar<std::decay_t<T>> || RMathType<std::decay_t<T>>;
// template <typename T>
// concept LargeType = !SmallType<std::decay_t<T>>;

namespace internal {

template<typename T>
inline consteval bool can_definitely_be_int(){

    constexpr int max_int = std::numeric_limits<int>::max();

    using xt = std::remove_cvref_t<T>;

    if constexpr (!CppIntegerType<xt>) return false;

    if constexpr (sizeof(xt) <= sizeof(int)) {
        if constexpr (std::is_unsigned_v<xt>) {
             return std::cmp_less_equal(std::numeric_limits<xt>::max(), max_int);
        }
        return true;
    }
    return false;
}

template<typename T>
inline consteval bool can_definitely_be_int64(){

    constexpr int64_t max_int64 = std::numeric_limits<int64_t>::max();

    using xt = std::remove_cvref_t<T>;

    if constexpr (!CppIntegerType<xt>) return false;

    if constexpr (sizeof(xt) <= sizeof(int64_t)) {
        if constexpr (std::is_unsigned_v<xt>) {
             return std::cmp_less_equal(std::numeric_limits<xt>::max(), max_int64);
        }
        return true; 
    }
    return false;
}

// C/C++ -> RVal typenames
// While these aren't the only ways of constructing RVals, they are many-to-one and non-ambiguous

// This is essentially a map of defined non-RVal to RVal conversion operators
// Allowing any of these to be cast to an RVal via static_cast<>
template <typename T>
struct r_val_mapping_impl {};

template <RVal T>
struct r_val_mapping_impl<T> { using type = T; };

template<> struct r_val_mapping_impl<bool>                      { using type = r_lgl; };
template<> struct r_val_mapping_impl<int>                       { using type = r_int; };
template<> struct r_val_mapping_impl<int64_t>                   { using type = r_int64; };
template<> struct r_val_mapping_impl<double>                    { using type = r_dbl; };
template<> struct r_val_mapping_impl<const char*>               { using type = r_str; };
template<> struct r_val_mapping_impl<std::complex<double>>      { using type = r_cplx; };
template<> struct r_val_mapping_impl<Rbyte>                     { using type = r_raw; };
template<> struct r_val_mapping_impl<SEXP>                      { using type = r_sexp; };

// R vectors & other containers
template <RVector T>
struct r_val_mapping_impl<T> { using type = r_sexp; };
template <RFactor T>
struct r_val_mapping_impl<T> { using type = r_sexp; };

template<CppMathType T>
struct r_val_mapping_impl<T> {
    using type = std::conditional_t<
    can_definitely_be_int<T>(), 
    r_int,
    std::conditional_t<
        can_definitely_be_int64<T>(), 
        r_int64,
        r_dbl
    >
>;

};

}

template <typename T>
using as_r_val_t = typename internal::r_val_mapping_impl<std::remove_cvref_t<T>>::type;

template <typename T>
concept CastableToRVal = requires {
    typename as_r_val_t<T>;
};

// Rules for determining math type promotion in binary operators

namespace internal {

template <RMathType T>
consteval uint8_t r_math_rank() {
    if constexpr (is<T, r_lgl>)   return 0;
    if constexpr (is<T, r_int>)   return 1;
    if constexpr (is<T, r_int64>) return 2;
    if constexpr (is<T, r_dbl>)   return 3;
    return std::numeric_limits<uint8_t>::max();
}

template <MathType T, MathType U>
requires AtLeastOneRMathType<T, U>
struct common_r_math_impl {
    using lhs_math_t = as_r_val_t<T>;
    using rhs_math_t = as_r_val_t<U>;

    static constexpr uint8_t rank_t = r_math_rank<lhs_math_t>();
    static constexpr uint8_t rank_u = r_math_rank<rhs_math_t>();
    
    using type = std::conditional_t<(rank_t >= rank_u), lhs_math_t, rhs_math_t>;
};

}

template <MathType T, MathType U>
requires AtLeastOneRMathType<T, U>
using common_r_math_t = typename internal::common_r_math_impl<T, U>::type;


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
template <> inline const char* type_str<r_dates>(){return "r_dates";}
template <> inline const char* type_str<r_posixcts>(){return "r_posixcts";}
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
    return "C++ integer";
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
template<> inline const char* type_str<Rboolean>(){return "Rboolean";}
template<> inline const char* type_str<Rbyte>(){return "Rbyte";}
template<> inline const char* type_str<Rcomplex>(){return "Rcomplex";}

namespace internal {

// Mapping from C++ type to R TYPEOF

// Helper to get the runtime R typeof for a C++ type
template<typename T> constexpr uint16_t r_typeof =              std::numeric_limits<uint16_t>::max();
template<> constexpr uint16_t r_typeof<r_vec<r_lgl>> =          LGLSXP;
template<> constexpr uint16_t r_typeof<r_vec<r_int>> =          INTSXP;
template<> constexpr uint16_t r_typeof<r_vec<r_int64>> =        CPP20_INT64SXP;
template<> constexpr uint16_t r_typeof<r_vec<r_dbl>> =          REALSXP;
template<> constexpr uint16_t r_typeof<r_vec<r_str_view>> =     STRSXP;
template<> constexpr uint16_t r_typeof<r_vec<r_str>> =          STRSXP;
template<> constexpr uint16_t r_typeof<r_vec<r_cplx>> =         CPLXSXP;
template<> constexpr uint16_t r_typeof<r_vec<r_raw>> =          RAWSXP;
template<> constexpr uint16_t r_typeof<r_vec<r_sexp>> =         VECSXP;

template<> constexpr uint16_t r_typeof<r_str_view> =            CHARSXP;
template<> constexpr uint16_t r_typeof<r_str> =                 CHARSXP;
template<> constexpr uint16_t r_typeof<r_sym> =                 SYMSXP;

template <typename T>
inline void check_valid_construction(SEXP x){
    if (r_typeof<T> != CPP20_TYPEOF(x)){
      abort("Bad construction from R type %s to C++ type %s", r_type_to_str(CPP20_TYPEOF(x)), type_str<T>());
    }
  }
}

}

#endif
