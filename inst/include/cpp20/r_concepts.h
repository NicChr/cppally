#ifndef CPP20_R_CONCEPTS_H
#define CPP20_R_CONCEPTS_H

#include <type_traits> // For concepts
#include <complex> // For complex<double>
#include <limits>


// Forward declarations
struct SEXPREC;
using SEXP = SEXPREC*;
using SEXPTYPE = unsigned int;

namespace cpp20 {

template <class... T>
inline constexpr bool always_false = false;

template <class... T>
inline constexpr bool always_true = true;

// Compile-time type check `is<>`
template <typename T, typename U>
inline constexpr bool is = std::same_as<std::remove_cvref_t<T>, std::remove_cvref_t<U>>;

template <typename T, typename... Args>
inline constexpr bool any = (is<T, Args> || ...);

// Forward declare R types

namespace internal {
struct view_tag;
}

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

template <typename T>
requires (any<T, r_int, r_dbl>)
struct r_date_t;

template <typename T>
requires (any<T, r_int64, r_dbl>)
struct r_psxct_t;

// By default, r_date uses doubles to match R conventions
using r_date = r_date_t<r_dbl>;
// By default, r_psxct uses doubles to match R conventions
using r_psxct = r_psxct_t<r_dbl>;

// Concepts to enable R type templates

template <typename T>
concept RLogicalType = is<T, r_lgl>; 

template <typename T>
concept RShortIntegerType = is<T, r_int>;

template <typename T>
concept RLongIntegerType = is<T, r_int64>;

template <typename T>
concept RIntegerType = RLogicalType<T> || RShortIntegerType<T> || RLongIntegerType<T>;

template <typename T>
concept CppIntegerType = std::is_integral_v<std::remove_cvref_t<T>>;

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
concept RDateType = any<T, r_date_t<r_int>, r_date_t<r_dbl>>;

template <typename T>
concept RPsxctType = any<T, r_psxct_t<r_int64>, r_psxct_t<r_dbl>>;

template <typename T>
concept RTimeType = RDateType<T> || RPsxctType<T>;

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
concept RNumericType = RMathType<T> || RTimeType<T>;
// This would be cleaner but template subsumption (and therefore partial specialisation) doesn't work
// concept NumericType = MathType<unwrap_t<T>>;

template <typename T>
concept CppNumericType = CppMathType<T>;


template <typename T>
concept NumericType = RNumericType<T> || CppNumericType<T>;

template <typename T>
concept RStringType = any<T, r_str, r_str_view>;

template <typename T>
concept RSortableType = RNumericType<T> || RStringType<T>;

template <typename T>
concept CppSortableType = CppNumericType<T> || any<T, std::string, std::string_view>;

template <typename T>
concept SortableType = RSortableType<T> || CppSortableType<T>;

template <typename T>
concept RSymbolType = is<T, r_sym>;

template <typename T>
concept RRawType = is<T, r_raw>;

template <typename T>
concept RAtomicScalar = RNumericType<T> || RComplexType<T> || RStringType<T> || RRawType<T>;

// Excludes r_sexp because it can represent vectors
template <typename T>
concept RCompositeScalar = RSymbolType<T>;

template <typename T>
concept RScalar = RAtomicScalar<T> || RCompositeScalar<T>;

// RVal is anything that can be stored in `r_vec<>`
template <typename T>
concept RVal = RScalar<T> || is<T, r_sexp>;

template <typename T, typename U>
concept AtLeastOneRMathType =
(RMathType<T> || RMathType<U>) && (MathType<T> && MathType<U>);

// Forward declare vector-based structs
template<RVal T>
struct r_vec;
struct r_factors;
// struct r_df;

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
template <typename T>
struct is_time_vector_impl : std::false_type {};
    
template <RTimeType T>
struct is_time_vector_impl<r_vec<T>> : std::true_type {};

template <typename T>
inline constexpr bool is_time_vector_v = is_time_vector_impl<std::remove_cvref_t<T>>::value;

}

template <typename T>
concept RAtomicVector = internal::is_atomic_r_vector_v<T> || internal::is_time_vector_v<T>;

template <typename T>
concept RVector = internal::is_r_vector_v<T>;

template <typename T>
concept RListVector = RVector<T> && (is<typename T::data_type, r_sexp> || RCompositeScalar<typename T::data_type>);

template <typename T>
concept RFactor = is<T, r_factors>;

template <typename T>
concept RMetaVector = RFactor<T>;

template <typename T>
concept RSortableVector = RVector<T> && RSortableType<typename T::data_type>;

// RObject is any object that can be represented in R - it excludes internal R types like CHARSXP
// Also, these are all implicitly convertible to `SEXP`
template <typename T> 
concept RObject = std::is_convertible_v<T, SEXP>;

template <typename T>
concept CppType = !(RVal<T> || RObject<T>);

template <typename T>
concept CppScalar = std::is_scalar_v<T>;

template <typename T>
concept Scalar = CppScalar<T> || RScalar<T>;

template <typename T>
inline constexpr bool is_sexp = any<T, SEXP, r_sexp>;

// Internal helpers
namespace internal {

// A `SEXP` which we can write data to directly via a pointer
template <typename T>
concept RPtrWritableType = RVal<T> && !RObject<T>;

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

template<typename T>
inline consteval bool can_definitely_be_int(){

    constexpr int max_int = std::numeric_limits<int>::max();

    using xt = std::remove_cvref_t<T>;

    if constexpr (!CppIntegerType<xt>) return false;

    if constexpr (sizeof(xt) <= sizeof(int)) {
        if constexpr (std::is_unsigned_v<xt>) {
             return std::numeric_limits<xt>::max() <= static_cast<unsigned int>(max_int);
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
             return std::numeric_limits<xt>::max() <= static_cast<uint64_t>(max_int64);
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
template<> struct r_val_mapping_impl<unsigned char>             { using type = r_raw; };
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

template <typename T>
struct unwrapped_type {
    using type = T;
};

template <RVal T>
struct unwrapped_type<T> {
    // Recursively call unwrapped_type on the inner type
    using type = typename unwrapped_type<typename T::value_type>::type;
};
template <typename T>
requires (RVector<T> || RMetaVector<T>)
struct unwrapped_type<T> {
    // All R vectors + other R objects contain SEXP
    using type = SEXP;
};

template <typename T>
struct inherited_type_impl { 
    using type = T; 
};
template <typename T>
requires requires { typename T::inherited_type; }
struct inherited_type_impl<T> { 
    using type = typename T::inherited_type; 
};

template <RVal T>
using inherited_type_t = typename internal::inherited_type_impl<std::remove_cvref_t<T>>::type;

}

// Type -> RVal type
template <typename T>
using as_r_val_t = typename internal::r_val_mapping_impl<std::remove_cvref_t<T>>::type;

// Can type be constructed/static_cast to RVal type?
template <typename T>
concept CastableToRVal = requires {
    typename as_r_val_t<T>;
};

// Recursively unwrap to inner C/C++ type
template <typename T>
using unwrap_t = typename internal::unwrapped_type<T>::type;

// Rules for determining math type promotion in binary operators

namespace internal {

template <RVal T>
consteval uint8_t r_type_rank() {
    if constexpr (is<T, r_lgl>)                     return 0;
    if constexpr (is<T, r_int>)                     return 1;
    if constexpr (is<T, r_int64>)                   return 2;
    if constexpr (is<T, r_dbl>)                     return 3;
    if constexpr (is<T, r_cplx>)                    return 4;
    if constexpr (is<T, r_raw>)                     return 5;
    if constexpr (is<T, r_date_t<r_int>>)           return 6;
    if constexpr (is<T, r_date_t<r_dbl>>)           return 7;
    if constexpr (is<T, r_psxct_t<r_int64>>)        return 8;
    if constexpr (is<T, r_psxct_t<r_dbl>>)          return 9;
    if constexpr (is<T, r_str>)                     return 10;
    if constexpr (is<T, r_str_view>)                return 11;
    if constexpr (is<T, r_sexp>)                    return 12;
    return std::numeric_limits<uint8_t>::max();
}

template <MathType T, MathType U>
requires (RMathType<T> || RMathType<U>) // At least one RMathType
struct common_r_math_impl {
    using lhs_math_t = as_r_val_t<T>;
    using rhs_math_t = as_r_val_t<U>;

    static constexpr uint8_t rank_t = r_type_rank<lhs_math_t>();
    static constexpr uint8_t rank_u = r_type_rank<rhs_math_t>();
    
    using type = std::conditional_t<(rank_t >= rank_u), lhs_math_t, rhs_math_t>;
};


template <RVal T, RVal U>
struct common_r_type_impl {
    static constexpr uint8_t rank_t = r_type_rank<T>();
    static constexpr uint8_t rank_u = r_type_rank<U>();
    using type = std::conditional_t<(rank_t >= rank_u), T, U>;
};

}

template <MathType T, MathType U>
requires (RMathType<T> || RMathType<U>) // At least one RMathType
using common_math_t = typename internal::common_r_math_impl<T, U>::type;

template <RVal T, RVal U>
using common_r_t = typename internal::common_r_type_impl<T, U>::type;


}

#endif
