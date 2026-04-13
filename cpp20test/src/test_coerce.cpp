#include <cpp20_light.hpp>
#include <cpp20/sugar/r_make_vec.h>
using namespace cpp20;

[[cpp20::register]]
SEXP test_as_sym(SEXP a, r_str b, r_dbl c, r_sym d, r_vec<r_str> e, r_vec<r_dbl> f){
    return make_vec<r_sexp>(
        as<r_sym>(a), as<r_sym>(b), as<r_sym>(c), as<r_sym>(d), as<r_sym>(e), as<r_sym>(f)
    );
}

[[cpp20::register]]
SEXP test_as_int(SEXP a, r_str b, r_dbl c, r_sym d, r_vec<r_str> e, r_vec<r_dbl> f){
    return make_vec<r_int>(
        as<r_int>(a), as<r_int>(b), as<r_int>(c), as<r_int>(d), as<r_int>(e), as<r_int>(f),
        as<r_vec<r_int>>(a), as<r_vec<r_int>>(b), as<r_vec<r_int>>(c), as<r_vec<r_int>>(d), as<r_vec<r_int>>(e), as<r_vec<r_int>>(f)
    );
}


[[cpp20::register]]
SEXP test_as_dbl(SEXP a, r_str b, r_int c, r_sym d, r_vec<r_str> e, r_vec<r_int> f){
    return make_vec<r_dbl>(
        as<r_dbl>(a), as<r_dbl>(b), as<r_dbl>(c), as<r_dbl>(d), as<r_dbl>(e), as<r_dbl>(f),
        as<r_vec<r_dbl>>(a), as<r_vec<r_dbl>>(b), as<r_vec<r_dbl>>(c), as<r_vec<r_dbl>>(d), as<r_vec<r_dbl>>(e), as<r_vec<r_dbl>>(f)
    );
}

[[cpp20::register]]
SEXP test_as_str(SEXP a, r_int b, r_dbl c, r_sym d, r_vec<r_int> e, r_vec<r_dbl> f){
    return make_vec<r_str>(
        as<r_str>(a), as<r_str>(b), as<r_str>(c), as<r_str>(d), as<r_str>(e), as<r_str>(f),
        as<r_vec<r_str>>(a), as<r_vec<r_str>>(b), as<r_vec<r_str>>(c), as<r_vec<r_str>>(d), as<r_vec<r_str>>(e), as<r_vec<r_str>>(f)
    );
}
