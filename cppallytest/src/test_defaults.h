#pragma once

#include <cppally.hpp>
using namespace cppally;

[[cppally::register]]
r_vec<r_lgl> test_default_lgls(r_lgl x = r_lgl{false}, r_lgl y = r_lgl(true), bool xx = false, bool yy = true, bool zz = true || false){    
    return r_vec<r_lgl>( {r_lgl(x), r_lgl(y), r_lgl(xx), r_lgl(yy), r_lgl(zz)} );
}

[[cppally::register]]
r_vec<r_int> test_default_ints(r_int x = r_int{-1}, r_int y = r_int(2), int z = 0, int xx = 1 + 2, r_int yy = r_int(2) + 3, int zz = 2.5){ 
    return r_vec<r_int>( {x, y, r_int(z), r_int(xx), yy, r_int(zz)} );
} 

[[cppally::register]]
auto test_default_dbls(r_dbl x = r_dbl{-1}, r_dbl y = r_dbl(2.5), double z = 3., double xx = 1 + 3, double yy = 1e10, double zz = std::numeric_limits<double>::max(), double zzz = std::sqrt(std::numeric_limits<double>::epsilon())){
    return r_vec<r_dbl>( {x, y, r_dbl(z), r_dbl(xx), r_dbl(yy), r_dbl(zz), r_dbl(zzz)} );
}

[[cppally::register]]
r_vec<r_str> test_default_strs(const char* a = "a", r_str b = r_str("b"), r_str_view c = r_str("c")){
    return r_vec<r_str>( {r_str(a), b, r_str(c)} );
}

template <RNumber T>
[[cppally::register]]
auto test_default_args_templated(
    T x = r_dbl(1) / r_dbl(2),
    r_int a = r_int(1) + 2,
    const r_dbl& b = r_dbl(1) / r_dbl(4),
    r_int64&& c = r_int64(5),
    bool d = 2,
    r_lgl e = r_na,
    r_str_view f = as<r_str>(r_int(42)),
    const r_str_view& g = as<r_str>(r_dbl(0.5)),
    std::optional<r_int> h = r_int(7)
){
    return r_vec<r_str>( {
        as<r_str>(x), as<r_str>(a), as<r_str>(b), as<r_str>(c), as<r_str>(r_lgl(d)),
        as<r_str>(e), r_str(f), r_str(g), as<r_str>(h.value())
    } );
}
[[cppally::register]]
r_int test_optional_default(std::optional<r_int> x = r_int(10)){
    return x.has_value() ? x.value() : r_int(100);
}

