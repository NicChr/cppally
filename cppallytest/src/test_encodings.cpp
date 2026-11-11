#include <cppally.hpp>
using namespace cppally;

[[cppally::register]]
r_vec<r_lgl> str_eq(r_vec<r_str> x, r_vec<r_str> y){
    return x == y;
}

[[cppally::register]]
r_vec<r_lgl> str_ne(r_vec<r_str> x, r_vec<r_str> y){
    return x != y;
}

[[cppally::register]]
r_vec<r_lgl> str_lt(r_vec<r_str> x, r_vec<r_str> y){
    return x < y;
}

[[cppally::register]]
r_vec<r_lgl> str_le(r_vec<r_str> x, r_vec<r_str> y){
    return x <= y;
}

[[cppally::register]]
r_vec<r_lgl> str_gt(r_vec<r_str> x, r_vec<r_str> y){
    return x > y;
}

[[cppally::register]]
r_vec<r_lgl> str_ge(r_vec<r_str> x, r_vec<r_str> y){
    return x >= y;
}

// "\xc3\xa9" is e-acute in UTF-8, spelled out so this file stays ASCII
[[cppally::register]]
r_vec<r_lgl> enc_eq_utf8_literal(r_vec<r_str> x){
    return pmap([](const auto& s){ return r_str_view(s) == std::string_view("\xc3\xa9"); }, x);
}

[[cppally::register]]
r_vec<r_lgl> enc_lt_utf8_literal(r_vec<r_str> x){
    return pmap([](const auto& s){ return r_str_view(s) < std::string_view("\xc3\xa9"); }, x);
}

[[cppally::register]]
r_vec<r_str> enc_unique(r_vec<r_str> x){
    return unique(x);
}

[[cppally::register]]
r_vec<r_lgl> enc_duplicated(r_vec<r_str> x){
    return duplicated(x);
}

[[cppally::register]]
r_int enc_n_unique(r_vec<r_str> x){
    return r_int(static_cast<int>(n_unique(x)));
}

[[cppally::register]]
r_vec<r_int> enc_match(r_vec<r_str> x, r_vec<r_str> table){
    return match(x, table);
}

[[cppally::register]]
r_vec<r_int> enc_group_ids(r_vec<r_str> x){
    return make_groups(x).ids;
}

[[cppally::register]]
r_vec<r_int> enc_ordered_group_ids(r_vec<r_str> x){
    return make_groups(x, true).ids;
}

[[cppally::register]]
r_int enc_n_groups(r_vec<r_str> x){
    return r_int(make_groups(x).n_groups);
}

[[cppally::register]]
r_vec<r_str> enc_sort(r_vec<r_str> x){
    return sort(x);
}

[[cppally::register]]
r_factors enc_factor(r_vec<r_str> x){
    return r_factors(x);
}

[[cppally::register]]
r_factors enc_factor_levels(r_vec<r_str> x, r_vec<r_str> levels){
    return r_factors(x, levels);
}

[[cppally::register]]
r_vec<r_int> enc_get_code(r_factors x, r_vec<r_str> vals){
    r_size_t n = vals.length();
    r_vec<r_int> out(n);
    for (r_size_t i = 0; i < n; ++i){
        out.set(i, x.get_code(vals.view(i)));
    }
    return out;
}

[[cppally::register]]
r_vec<r_int> enc_match_factors(r_factors x, r_factors table){
    return match(x, table);
}

[[cppally::register]]
r_vec<r_lgl> enc_factor_eq(r_factors x, r_factors y){
    return x == y;
}

[[cppally::register]]
r_vec<r_int> enc_name_index(r_vec<r_int> x, r_vec<r_str> keys){
    r_size_t n = keys.length();
    r_vec<r_int> out(n);
    for (r_size_t i = 0; i < n; ++i){
        out.set(i, x.name_index(keys.view(i), false));
    }
    return out;
}

// Encoding names match what R's Encoding() reports
[[cppally::register]]
r_vec<r_str> enc_encoding(r_vec<r_str> x){
    r_size_t n = x.length();
    r_vec<r_str> out(n);
    for (r_size_t i = 0; i < n; ++i){
        const char* name = "unsupported";
        switch (x.get(i).encoding()){
            case str_encoding::native: name = "unknown"; break;
            case str_encoding::utf8: name = "UTF-8"; break;
            case str_encoding::latin1: name = "latin1"; break;
            case str_encoding::bytes: name = "bytes"; break;
            default: break;
        }
        out.set(i, r_str(name));
    }
    return out;
}

[[cppally::register]]
r_vec<r_str> enc_with_encoding(r_vec<r_str> x, const char* encoding){
    std::string_view name(encoding);
    str_encoding enc = str_encoding::native;
    if (name == "UTF-8"){
        enc = str_encoding::utf8;
    } else if (name == "latin1"){
        enc = str_encoding::latin1;
    } else if (name == "bytes"){
        enc = str_encoding::bytes;
    }
    r_size_t n = x.length();
    r_vec<r_str> out(n);
    for (r_size_t i = 0; i < n; ++i){
        out.set(i, x.get(i).with_encoding(enc));
    }
    return out;
}

[[cppally::register]]
r_vec<r_str> enc_as_utf8(r_vec<r_str> x){
    r_size_t n = x.length();
    r_vec<r_str> out(n);
    for (r_size_t i = 0; i < n; ++i){
        out.set(i, x.get(i).as_utf8());
    }
    return out;
}

[[cppally::register]]
r_vec<r_lgl> enc_is_ascii(r_vec<r_str> x){
    return pmap([](const auto& s){ return r_lgl(s.is_ascii()); }, x);
}

[[cppally::register]]
r_vec<r_lgl> enc_is_utf8(r_vec<r_str> x){
    return pmap([](const auto& s){ return r_lgl(s.is_utf8()); }, x);
}

[[cppally::register]]
r_vec<r_lgl> enc_is_latin1(r_vec<r_str> x){
    return pmap([](const auto& s){ return r_lgl(s.is_latin1()); }, x);
}

[[cppally::register]]
r_vec<r_lgl> enc_needs_translation(r_vec<r_str> x){
    return pmap([](const auto& s){ return r_lgl(s.needs_utf8_translation()); }, x);
}

[[cppally::register]]
r_vec<r_str> enc_paste(r_vec<r_str> x, r_vec<r_str> y, const char* sep){
    return str_paste({x, y}, sep);
}

[[cppally::register]]
r_str enc_collapse(r_vec<r_str> x, const char* sep){
    return str_collapse(x, sep);
}
