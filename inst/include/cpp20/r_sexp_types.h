
#ifndef CPP20_R_SEXP_TYPES_H
#define CPP20_R_SEXP_TYPES_H

// Runtime IDs for SEXP (via TYPEOF)

#include <cpp20/r_setup.h>
#include <cpp20/r_concepts.h>
#include <cpp20/r_protect.h>

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

// using r_sexp_tag_t = uint16_t; // cpp20 version of SEXPTYPE

// Currently unfinished
// enum : r_sexp_tag_t {
//     r_lgl_id = 1,
//     r_int_id = 2,
//     r_int64_id = 3,
//     r_dbl_id = 4,
//     r_cplx_id = 5,
//     r_raw_id = 6,
//     r_dates_id = 7,
//     r_pxt_id = 8,
//     r_chr = 9,
//     r_fct = 10,
//     r_list = 11,
//     r_df = 12,
//     r_unk = 13
// };

}

}

#endif
