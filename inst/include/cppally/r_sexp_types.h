
#ifndef CPPALLY_R_SEXP_TYPES_H
#define CPPALLY_R_SEXP_TYPES_H

// Runtime IDs for SEXP (via TYPEOF)

#include <cppally/r_setup.h>
#include <cppally/r_concepts.h>
#include <cppally/r_protect.h>

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
    default: {
        return xtype;
    }
    }
}

inline const char* r_type_to_str(SEXPTYPE x){

    switch (x){
    case CPPALLY_INT64SXP: return "CPPALLY_INT64SXP";
    case CPPALLY_REALDATESXP: return "CPPALLY_REALDATESXP";
    case CPPALLY_REALPSXTSXP: return "CPPALLY_REALPSXTSXP";
    case CPPALLY_FCTSXP: return "CPPALLY_FCTSXP";
    case CPPALLY_DFSXP: return "CPPALLY_DFSXP";
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
template <> inline const char* type_str<r_date>(){return "r_date";}
template <> inline const char* type_str<r_psxct>(){return "r_psxct";}
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

template <typename T> inline constexpr uint16_t r_typeof_impl =              std::numeric_limits<uint16_t>::max();
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


// The above mappings represent the plain TYPEOF values of cppally objects, this enables r_vec<T> to check the primitive type id during construction
// without rejecting objects such as `r_factors`
// The below represents the actual cppally type id mapping
template <typename T> constexpr uint16_t r_typeof =                         r_typeof_impl<T>;
template<> inline constexpr uint16_t r_typeof<r_vec<r_int64>> =             CPPALLY_INT64SXP;
template<> inline constexpr uint16_t r_typeof<r_vec<r_date>> =              CPPALLY_REALDATESXP;
template<> inline constexpr uint16_t r_typeof<r_vec<r_psxct>> =             CPPALLY_REALPSXTSXP;
template<> inline constexpr uint16_t r_typeof<r_factors> =                  CPPALLY_FCTSXP;

// Low-level type ID check, primarily used in constructing classed cppally objects from SEXP
template <typename T>
inline void check_valid_construction(SEXP x){
    if (r_typeof_impl<T> != TYPEOF(x)) [[unlikely]] {
        abort("Bad construction from R type %s to C++ type %s", Rf_type2char(TYPEOF(x)), type_str<T>());
    }
}

// using r_sexp_tag_t = uint16_t; // cppally version of SEXPTYPE

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
