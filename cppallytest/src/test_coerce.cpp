#include "cppallytest.h"
using namespace cppally;

[[cppally::register]]
SEXP test_as_sym(SEXP a, r_str b, r_dbl c, r_sym d, r_vec<r_str> e, r_vec<r_dbl> f){
    return make_vec<r_sexp>(
        as<r_sym>(a), as<r_sym>(b), as<r_sym>(c), as<r_sym>(d), as<r_sym>(e), as<r_sym>(f)
    );
}

[[cppally::register]]
SEXP test_as_int(SEXP a, r_str b, r_dbl c, r_sym d, r_vec<r_str> e, r_vec<r_dbl> f){
    return make_vec<r_int>(
        as<r_int>(a), as<r_int>(b), as<r_int>(c), as<r_int>(d), as<r_int>(e), as<r_int>(f),
        as<r_vec<r_int>>(a), as<r_vec<r_int>>(b), as<r_vec<r_int>>(c), as<r_vec<r_int>>(d), as<r_vec<r_int>>(e), as<r_vec<r_int>>(f)
    );
}


[[cppally::register]]
SEXP test_as_dbl(SEXP a, r_str b, r_int c, r_sym d, r_vec<r_str> e, r_vec<r_int> f){
    return make_vec<r_dbl>(
        as<r_dbl>(a), as<r_dbl>(b), as<r_dbl>(c), as<r_dbl>(d), as<r_dbl>(e), as<r_dbl>(f),
        as<r_vec<r_dbl>>(a), as<r_vec<r_dbl>>(b), as<r_vec<r_dbl>>(c), as<r_vec<r_dbl>>(d), as<r_vec<r_dbl>>(e), as<r_vec<r_dbl>>(f)
    );
}

[[cppally::register]]
SEXP test_as_str(SEXP a, r_int b, r_dbl c, r_sym d, r_vec<r_int> e, r_vec<r_dbl> f){
    return make_vec<r_str>(
        as<r_str>(a), as<r_str>(b), as<r_str>(c), as<r_str>(d), as<r_str>(e), as<r_str>(f),
        as<r_vec<r_str>>(a), as<r_vec<r_str>>(b), as<r_vec<r_str>>(c), as<r_vec<r_str>>(d), as<r_vec<r_str>>(e), as<r_vec<r_str>>(f)
    );
}

// Snapshot of current as<>() coercion behaviour across all RMathType combinations.
// Sources carry the value 3 (or true == 1 for logical sources). NA is only tested
// from the four R types that can hold it (r_int, r_int64, r_dbl, r_lgl).

// -> int
[[cppally::register]]
void test_to_int(){
    expect_identical(as<int>(int(3)), int(3));
    expect_identical(as<int>(int64_t(3)), int(3));
    expect_identical(as<int>(double(3)), int(3));
    expect_identical(as<int>(3u), int(3));
    expect_identical(as<int>(r_size_t(3)), int(3));
    expect_identical(as<int>(true), int(1));
    expect_identical(as<int>(r_int(3)), int(3));
    expect_identical(as<int>(r_int64(3)), int(3));
    expect_identical(as<int>(r_dbl(3)), int(3));
    expect_identical(as<int>(r_true), int(1));

    expect_identical(as<int>(na<r_int>()), unwrap(na<r_int>()));
    expect_identical(as<int>(na<r_int64>()), unwrap(na<r_int>()));
    expect_identical(as<int>(na<r_dbl>()), unwrap(na<r_int>()));
    expect_identical(as<int>(na<r_lgl>()), unwrap(na<r_int>()));
}

// -> int64_t
[[cppally::register]]
void test_to_int64(){
    expect_identical(as<int64_t>(int(3)), int64_t(3));
    expect_identical(as<int64_t>(int64_t(3)), int64_t(3));
    expect_identical(as<int64_t>(double(3)), int64_t(3));
    expect_identical(as<int64_t>(3u), int64_t(3));
    expect_identical(as<int64_t>(r_size_t(3)), int64_t(3));
    expect_identical(as<int64_t>(true), int64_t(1));
    expect_identical(as<int64_t>(r_int(3)), int64_t(3));
    expect_identical(as<int64_t>(r_int64(3)), int64_t(3));
    expect_identical(as<int64_t>(r_dbl(3)), int64_t(3));
    expect_identical(as<int64_t>(r_true), int64_t(1));

    expect_identical(as<int64_t>(na<r_int>()), unwrap(na<r_int64>()));
    expect_identical(as<int64_t>(na<r_int64>()), unwrap(na<r_int64>()));
    expect_identical(as<int64_t>(na<r_dbl>()), unwrap(na<r_int64>()));
    expect_identical(as<int64_t>(na<r_lgl>()), unwrap(na<r_int64>()));
}

// -> double
[[cppally::register]]
void test_to_double(){
    expect_identical(as<double>(int(3)), double(3));
    expect_identical(as<double>(int64_t(3)), double(3));
    expect_identical(as<double>(double(3)), double(3));
    expect_identical(as<double>(3u), double(3));
    expect_identical(as<double>(r_size_t(3)), double(3));
    expect_identical(as<double>(true), double(1));
    expect_identical(as<double>(r_int(3)), double(3));
    expect_identical(as<double>(r_int64(3)), double(3));
    expect_identical(as<double>(r_dbl(3)), double(3));
    expect_identical(as<double>(r_true), double(1));

    expect_identical(as<double>(na<r_int>()), unwrap(na<r_dbl>()));
    expect_identical(as<double>(na<r_int64>()), unwrap(na<r_dbl>()));
    expect_identical(as<double>(na<r_dbl>()), unwrap(na<r_dbl>()));
    expect_identical(as<double>(na<r_lgl>()), unwrap(na<r_dbl>()));
}

// -> unsigned int
[[cppally::register]]
void test_to_uint(){
    expect_identical(as<unsigned int>(int(3)), 3u);
    expect_identical(as<unsigned int>(int64_t(3)), 3u);
    expect_identical(as<unsigned int>(double(3)), 3u);
    expect_identical(as<unsigned int>(3u), 3u);
    expect_identical(as<unsigned int>(r_size_t(3)), 3u);
    expect_identical(as<unsigned int>(true), 1u);
    expect_identical(as<unsigned int>(r_int(3)), 3u);
    expect_identical(as<unsigned int>(r_int64(3)), 3u);
    expect_identical(as<unsigned int>(r_dbl(3)), 3u);
    expect_identical(as<unsigned int>(r_true), 1u);

    // unsigned int has no NA representation; NA maps to 0
    expect_identical(as<unsigned int>(na<r_int>()), 0u);
    expect_identical(as<unsigned int>(na<r_int64>()), 0u);
    expect_identical(as<unsigned int>(na<r_dbl>()), 0u);
    expect_identical(as<unsigned int>(na<r_lgl>()), 0u);
}

// -> r_size_t
[[cppally::register]]
void test_to_r_size_t(){
    expect_identical(as<r_size_t>(int(3)), r_size_t(3));
    expect_identical(as<r_size_t>(int64_t(3)), r_size_t(3));
    expect_identical(as<r_size_t>(double(3)), r_size_t(3));
    expect_identical(as<r_size_t>(3u), r_size_t(3));
    expect_identical(as<r_size_t>(r_size_t(3)), r_size_t(3));
    expect_identical(as<r_size_t>(true), r_size_t(1));
    expect_identical(as<r_size_t>(r_int(3)), r_size_t(3));
    expect_identical(as<r_size_t>(r_int64(3)), r_size_t(3));
    expect_identical(as<r_size_t>(r_dbl(3)), r_size_t(3));
    expect_identical(as<r_size_t>(r_true), r_size_t(1));

    // NA leaks the int64 sentinel through (r_size_t has no NA concept)
    expect_identical(as<r_size_t>(na<r_int>()), static_cast<r_size_t>(unwrap(na<r_int64>())));
    expect_identical(as<r_size_t>(na<r_int64>()), static_cast<r_size_t>(unwrap(na<r_int64>())));
    expect_identical(as<r_size_t>(na<r_dbl>()), static_cast<r_size_t>(unwrap(na<r_int64>())));
    expect_identical(as<r_size_t>(na<r_lgl>()), static_cast<r_size_t>(unwrap(na<r_int64>())));
}

// -> bool
[[cppally::register]]
void test_to_bool(){
    expect_identical(as<bool>(int(3)), true);
    expect_identical(as<bool>(int64_t(3)), true);
    expect_identical(as<bool>(double(3)), true);
    expect_identical(as<bool>(3u), true);
    expect_identical(as<bool>(r_size_t(3)), true);
    expect_identical(as<bool>(true), true);
    expect_identical(as<bool>(r_int(3)), true);
    expect_identical(as<bool>(r_int64(3)), true);
    expect_identical(as<bool>(r_dbl(3)), true);
    expect_identical(as<bool>(r_true), true);
}

// -> r_int
[[cppally::register]]
void test_to_r_int(){
    expect_identical(as<r_int>(int(3)), r_int(3));
    expect_identical(as<r_int>(int64_t(3)), r_int(3));
    expect_identical(as<r_int>(double(3)), r_int(3));
    expect_identical(as<r_int>(3u), r_int(3));
    expect_identical(as<r_int>(r_size_t(3)), r_int(3));
    expect_identical(as<r_int>(true), r_int(1));
    expect_identical(as<r_int>(r_int(3)), r_int(3));
    expect_identical(as<r_int>(r_int64(3)), r_int(3));
    expect_identical(as<r_int>(r_dbl(3)), r_int(3));
    expect_identical(as<r_int>(r_true), r_int(1));

    expect_identical(as<r_int>(na<r_int>()), na<r_int>());
    expect_identical(as<r_int>(na<r_int64>()), na<r_int>());
    expect_identical(as<r_int>(na<r_dbl>()), na<r_int>());
    expect_identical(as<r_int>(na<r_lgl>()), na<r_int>());
}

// -> r_int64
[[cppally::register]]
void test_to_r_int64(){
    expect_identical(as<r_int64>(int(3)), r_int64(3));
    expect_identical(as<r_int64>(int64_t(3)), r_int64(3));
    expect_identical(as<r_int64>(double(3)), r_int64(3));
    expect_identical(as<r_int64>(3u), r_int64(3));
    expect_identical(as<r_int64>(r_size_t(3)), r_int64(3));
    expect_identical(as<r_int64>(true), r_int64(1));
    expect_identical(as<r_int64>(r_int(3)), r_int64(3));
    expect_identical(as<r_int64>(r_int64(3)), r_int64(3));
    expect_identical(as<r_int64>(r_dbl(3)), r_int64(3));
    expect_identical(as<r_int64>(r_true), r_int64(1));

    expect_identical(as<r_int64>(na<r_int>()), na<r_int64>());
    expect_identical(as<r_int64>(na<r_int64>()), na<r_int64>());
    expect_identical(as<r_int64>(na<r_dbl>()), na<r_int64>());
    expect_identical(as<r_int64>(na<r_lgl>()), na<r_int64>());
}

// -> r_dbl
[[cppally::register]]
void test_to_r_dbl(){
    expect_identical(as<r_dbl>(int(3)), r_dbl(3));
    expect_identical(as<r_dbl>(int64_t(3)), r_dbl(3));
    expect_identical(as<r_dbl>(double(3)), r_dbl(3));
    expect_identical(as<r_dbl>(3u), r_dbl(3));
    expect_identical(as<r_dbl>(r_size_t(3)), r_dbl(3));
    expect_identical(as<r_dbl>(true), r_dbl(1));
    expect_identical(as<r_dbl>(r_int(3)), r_dbl(3));
    expect_identical(as<r_dbl>(r_int64(3)), r_dbl(3));
    expect_identical(as<r_dbl>(r_dbl(3)), r_dbl(3));
    expect_identical(as<r_dbl>(r_true), r_dbl(1));

    expect_identical(as<r_dbl>(na<r_int>()), na<r_dbl>());
    expect_identical(as<r_dbl>(na<r_int64>()), na<r_dbl>());
    expect_identical(as<r_dbl>(na<r_dbl>()), na<r_dbl>());
    expect_identical(as<r_dbl>(na<r_lgl>()), na<r_dbl>());
}

// -> r_lgl
[[cppally::register]]
void test_to_r_lgl(){
    expect_identical(as<r_lgl>(int(3)), r_true);
    expect_identical(as<r_lgl>(int64_t(3)), r_true);
    expect_identical(as<r_lgl>(double(3)), r_true);
    expect_identical(as<r_lgl>(3u), r_true);
    expect_identical(as<r_lgl>(r_size_t(3)), r_true);
    expect_identical(as<r_lgl>(true), r_true);
    expect_identical(as<r_lgl>(r_int(3)), r_true);
    expect_identical(as<r_lgl>(r_int64(3)), r_true);
    expect_identical(as<r_lgl>(r_dbl(3)), r_true);
    expect_identical(as<r_lgl>(r_true), r_true);

    expect_identical(as<r_lgl>(na<r_int>()), na<r_lgl>());
    expect_identical(as<r_lgl>(na<r_int64>()), na<r_lgl>());
    expect_identical(as<r_lgl>(na<r_dbl>()), na<r_lgl>());
    expect_identical(as<r_lgl>(na<r_lgl>()), na<r_lgl>());
}

// Edge cases that do NOT abort: value truncation, widening, boundary values, and
// non-finite doubles. Coercions that lose information from a non-NA source (integer
// overflow, +/-Inf into an integer) abort inside scalar_coerce -- those are the
// helpers below, checked with expect_error() on the R side.
[[cppally::register]]
void test_coerce_edge(){
    // fractional double -> integer truncates toward zero (matches R's as.integer)
    expect_identical(as<r_int>(r_dbl(3.9)),    r_int(3));
    expect_identical(as<r_int>(r_dbl(-3.9)),   r_int(-3));
    expect_identical(as<r_int64>(r_dbl(-3.9)), r_int64(-3));

    // largest double that still fits each integer type (open upper bound at 2^digits)
    expect_identical(as<r_int>(r_dbl(2147483647.0)), r_limits<r_int>::max());
    // 2^63 - 1024, the largest double strictly below 2^63
    expect_identical(as<r_int64>(r_dbl(9223372036854774784.0)), r_int64(9223372036854774784LL));

    // widening is always lossless
    expect_identical(as<r_int64>(r_int(2147483647)), r_int64(2147483647));
    expect_identical(as<r_dbl>(r_int(2147483647)),   r_dbl(2147483647.0));
    expect_identical(as<r_dbl>(r_int64(2147483647)), r_dbl(2147483647.0));

    // int64 -> double may lose precision (tolerated), but never NAs: 2^53 + 1 rounds to 2^53
    expect_identical(as<r_dbl>(r_int64(9007199254740993LL)), r_dbl(9007199254740992.0));

    // INT_MIN is r_int's NA sentinel, but as an int64 value it is ordinary: widening it
    // to double is exact and lossless (the sentinel-ness is an r_int property, not the value's)
    expect_identical(as<r_dbl>(r_int64(std::numeric_limits<int>::min())),
                     r_dbl(static_cast<double>(std::numeric_limits<int>::min())));

    // NaN is treated as NA, so it propagates to the target's NA rather than aborting
    expect_identical(as<r_int>(r_dbl(R_NaN)),   na<r_int>());
    expect_identical(as<r_int64>(r_dbl(R_NaN)), na<r_int64>());

    // +/-Inf and NaN survive a double -> double (identity) coercion unchanged
    expect_identical(as<r_dbl>(r_dbl(R_PosInf)), r_dbl(R_PosInf));
    expect_identical(as<r_dbl>(r_dbl(R_NegInf)), r_dbl(R_NegInf));
    expect_identical(as<r_dbl>(r_dbl(R_NaN)),    r_dbl(R_NaN));
}

// Each loses information from a non-NA source, which scalar_coerce rejects via abort().
// Checked with expect_error() on the R side.

[[cppally::register]]
void test_coerce_int64_to_int_overflow(){ as<r_int>(r_int64(3000000000LL)); } // > INT_MAX

[[cppally::register]]
void test_coerce_dbl_to_int_overflow(){ as<r_int>(r_dbl(3e9)); }

[[cppally::register]]
void test_coerce_pos_inf_to_int(){ as<r_int>(r_dbl(R_PosInf)); }

[[cppally::register]]
void test_coerce_neg_inf_to_int(){ as<r_int>(r_dbl(R_NegInf)); }

// 2^63 exactly: (double)INT64_MAX rounds up to this, so the open upper bound must reject it
[[cppally::register]]
void test_coerce_dbl_to_int64_overflow(){ as<r_int64>(r_dbl(9223372036854775808.0)); }

// Sentinel collisions: the value is in native range but casts exactly onto the target's
// NA sentinel (INT_MIN / INT64_MIN), so it cannot be represented as a non-NA value -> abort
[[cppally::register]]
void test_coerce_int_min_to_int(){ as<r_int>(r_int64(std::numeric_limits<int>::min())); }

[[cppally::register]]
void test_coerce_int64_min_to_int64(){ as<r_int64>(r_dbl(-9223372036854775808.0)); }
