
#ifndef CPPALLY_R_SEXP_TYPES_H
#define CPPALLY_R_SEXP_TYPES_H

// Runtime IDs for SEXP (via TYPEOF)

#include <cppally/r_setup.h>
#include <cppally/r_concepts.h>
#include <cppally/r_sexp/protect.h>
#include <cppally/string/string_literal.h>

namespace cppally {

namespace internal {

// Collection of run-time SEXP type helpers
// This is necessary for registering C++ functions between the C++/R boundary
// To do this we create a run-time type ID (using `TYPEPOF()`) and add 
// custom values for objects we want to differentiate from their storage type
// For example, (double-storage) dates are REALSXP but here we create a new id CPPALLY_REALDATESXP

// Integer dates and integer64 date-times were earlier fully supported but the added template bloat was deemed not worth it for the niche use-case

// Custom SEXP tags, differentiating integer64, dates, date-times and factors
inline constexpr SEXPTYPE CPPALLY_INT64SXP = 64;
inline constexpr SEXPTYPE CPPALLY_REALDATESXP = 200;
inline constexpr SEXPTYPE CPPALLY_REALPSXTSXP = 201;
inline constexpr SEXPTYPE CPPALLY_FCTSXP = 202;
inline constexpr SEXPTYPE CPPALLY_DFSXP = 203;
inline constexpr SEXPTYPE CPPALLY_FUNCTIONSXP = 204;

inline SEXPTYPE CPPALLY_TYPEOF(SEXP x) noexcept {

    auto xtype = TYPEOF(x);

    switch (xtype){
    case INTSXP: {
        if (!Rf_isObject(x)) return xtype;
        if (Rf_inherits(x, "factor")) return CPPALLY_FCTSXP;
        return xtype;
    }
    case REALSXP: {
        if (!Rf_isObject(x)) return xtype;
        if (Rf_inherits(x, "Date")) return CPPALLY_REALDATESXP;
        if (Rf_inherits(x, "POSIXct")) return CPPALLY_REALPSXTSXP;
        if (Rf_inherits(x, "integer64")) return CPPALLY_INT64SXP; 
        return xtype;
    }
    case VECSXP: {
        if (!Rf_isObject(x)) return xtype;
        if (Rf_inherits(x, "data.frame")) return CPPALLY_DFSXP;
        return xtype;
    }
    case CLOSXP:
    case BUILTINSXP:
    case SPECIALSXP: {
        return CPPALLY_FUNCTIONSXP;
    }
    default: {
        return xtype;
    }
    }
}

inline const char* r_type_to_str(SEXPTYPE x){

    switch (x){
    case CPPALLY_INT64SXP: return "integer64";
    case CPPALLY_REALDATESXP: return "date (double storage)";
    case CPPALLY_REALPSXTSXP: return "date-time (double storage)";
    case CPPALLY_FCTSXP: return "factor";
    case CPPALLY_DFSXP: return "data frame";
    case CPPALLY_FUNCTIONSXP: return "function";
    default: return Rf_type2char(x);
    }
}

template <typename T> inline constexpr auto type_name = string_literal("Unknown");

template <> inline constexpr auto type_name<void> = string_literal("void");
template <> inline constexpr auto type_name<r_lgl> = string_literal("r_lgl");
template <> inline constexpr auto type_name<r_int> = string_literal("r_int");
template <> inline constexpr auto type_name<r_int64> = string_literal("r_int64");
template <> inline constexpr auto type_name<r_dbl> = string_literal("r_dbl");
template <> inline constexpr auto type_name<r_str> = string_literal("r_str");
template <> inline constexpr auto type_name<r_str_view> = string_literal("r_str_view");
template <> inline constexpr auto type_name<r_cplx> = string_literal("r_cplx");
template <> inline constexpr auto type_name<r_raw> = string_literal("r_raw");
template <> inline constexpr auto type_name<r_sym> = string_literal("r_sym");
template <> inline constexpr auto type_name<r_sexp> = string_literal("r_sexp");
template <> inline constexpr auto type_name<r_date> = string_literal("r_date");
template <> inline constexpr auto type_name<r_psxct> = string_literal("r_psxct");
template <> inline constexpr auto type_name<r_factors> = string_literal("r_factors");
template <> inline constexpr auto type_name<r_df> = string_literal("r_df");
template <> inline constexpr auto type_name<r_function> = string_literal("r_function");

template <RVector T>
inline constexpr auto type_name<T> = string_literal("r_vec<").concat(type_name<typename T::data_type>).concat(">");

template <CppFloatType T> inline constexpr auto type_name<T> = string_literal("C++ float");
template <CppIntegerType T> inline constexpr auto type_name<T> = string_literal("C/C++ integer");
template <CStringType T> inline constexpr auto type_name<T> = string_literal("C string");
template <CppStringType T> inline constexpr auto type_name<T> = string_literal("C++ string");
template <CppComplexType T> inline constexpr auto type_name<T> = string_literal("C++ complex");

template <typename T>
constexpr const char* type_str(){
    return type_name<T>.data;
}

// Mapping from C++ type to R TYPEOF

template <typename T> inline constexpr uint16_t r_typeof_impl =             std::numeric_limits<uint16_t>::max();
template<> inline constexpr uint16_t r_typeof_impl<r_vec<r_lgl>> =          LGLSXP;
template<> inline constexpr uint16_t r_typeof_impl<r_vec<r_int>> =          INTSXP;
template<> inline constexpr uint16_t r_typeof_impl<r_vec<r_dbl>> =          REALSXP;
template<> inline constexpr uint16_t r_typeof_impl<r_vec<r_str_view>> =     STRSXP;
template<> inline constexpr uint16_t r_typeof_impl<r_vec<r_str>> =          STRSXP;
template<> inline constexpr uint16_t r_typeof_impl<r_vec<r_cplx>> =         CPLXSXP;
template<> inline constexpr uint16_t r_typeof_impl<r_vec<r_raw>> =          RAWSXP;
template <RListVector T> inline constexpr uint16_t r_typeof_impl<T> =       VECSXP;
template<> inline constexpr uint16_t r_typeof_impl<r_str_view> =            CHARSXP;
template<> inline constexpr uint16_t r_typeof_impl<r_str> =                 CHARSXP;
template<> inline constexpr uint16_t r_typeof_impl<r_sym> =                 SYMSXP;
template<> inline constexpr uint16_t r_typeof_impl<r_vec<r_int64>> =        REALSXP;
template<> inline constexpr uint16_t r_typeof_impl<r_vec<r_date>> =         REALSXP;
template<> inline constexpr uint16_t r_typeof_impl<r_vec<r_psxct>> =        REALSXP;
template<> inline constexpr uint16_t r_typeof_impl<r_factors> =             INTSXP;
template<> inline constexpr uint16_t r_typeof_impl<r_df> =                  VECSXP;


// The above mappings represent the plain TYPEOF values of cppally objects, this enables r_vec<T> to check the primitive type id during construction
// without rejecting objects such as `r_factors`
// The below represents the actual cppally type id mapping
template <typename T> constexpr uint16_t r_typeof =                         r_typeof_impl<T>;
template<> inline constexpr uint16_t r_typeof<r_vec<r_int64>> =             CPPALLY_INT64SXP;
template<> inline constexpr uint16_t r_typeof<r_vec<r_date>> =              CPPALLY_REALDATESXP;
template<> inline constexpr uint16_t r_typeof<r_vec<r_psxct>> =             CPPALLY_REALPSXTSXP;
template<> inline constexpr uint16_t r_typeof<r_factors> =                  CPPALLY_FCTSXP;
template<> inline constexpr uint16_t r_typeof<r_df> =                       CPPALLY_DFSXP;

// Low-level type ID check, primarily used in constructing classed cppally objects from SEXP
template <typename T>
inline void check_valid_construction(SEXP x){
    if (r_typeof_impl<T> != TYPEOF(x)) [[unlikely]] {
        abort("Bad construction from R type %s to C++ type %s", Rf_type2char(TYPEOF(x)), type_str<T>());
    }
}

}

}

#endif
