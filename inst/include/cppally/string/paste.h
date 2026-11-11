#ifndef CPPALLY_R_PASTE_H
#define CPPALLY_R_PASTE_H

#include <cppally/vector/r_vector.h>
#include <cppally/list/list_utils.h>
#include <cppally/coerce.h>
#include <initializer_list>
#include <string>
#include <vector>

// Concatenate vectors of strings

namespace cppally {

namespace internal {

// std::string + R encoding
struct encoded_string {
    std::string text = "";
    str_encoding enc = str_encoding::native;

    // bytes takes priority over UTF-8, which takes priority over native
    void set_encoding(str_encoding new_enc) {
        if (enc != str_encoding::bytes) {
            enc = new_enc;
        }
    }

    void append(r_str_view x) {
        if (x.is_utf8()) {
            if (x.encoding() == str_encoding::utf8) {
                set_encoding(str_encoding::utf8);
            }
            text += x.c_str();
        } else {
            set_encoding(x.encoding() == str_encoding::bytes ? str_encoding::bytes : str_encoding::utf8);
            text += utf8_string(x);
        }
    }
};

// Concatenate all character vectors of a list together
inline r_vec<r_str> str_paste_list(const std::vector<r_vec<r_str_view>>& x, r_str_view sep){
    
    if (is_na(sep)) [[unlikely]] {
        abort("`sep` must not be NA");
    }

    r_size_t n_vecs = x.size();
    if (n_vecs == 0){
        return r_vec<r_str>();
    }

    std::vector<r_size_t> lens;
    lens.reserve(n_vecs);

    r_size_t n = 0;
    for (const auto& v : x){
        r_size_t l = v.length();
        lens.push_back(l);
        if (l == 0){
            n = 0; 
            break; 
        }
        n = std::max(n, l);
    }

    if (n == 0){
        return r_vec<r_str>();
    }

    std::vector<r_size_t> idx(n_vecs, 0);
    encoded_string buf;
    r_str_view e;
    r_vec<r_str_view> out(n);

    for (r_size_t j = 0; j < n; ++j){
        buf.text.clear();
        buf.enc = str_encoding::native;
        for (r_size_t i = 0; i < n_vecs; ++i){
            e = x[i].view(idx[i]);
            if (is_na(e)){
                break;
            }
            if (i > 0){
                buf.append(sep);
            }
            buf.append(e);
        }
        out.set(j, is_na(e) ? na<r_str_view>() : r_str_view(mk_char(buf.text, buf.enc, false), no_checks_tag{}));
        // Advance every counter, even on early break, to keep recycling aligned
        for (r_size_t i = 0; i < n_vecs; ++i){
            recycle_index(idx[i], lens[i]);
        }
    }
    return as<r_vec<r_str>>(out);
}

}

template <RAtomicVector T>
inline r_vec<r_str> str_paste(std::initializer_list<T> args, r_str_view sep){
    std::vector<r_vec<r_str_view>> characters;
    characters.reserve(args.size());
    for (const T& v : args){
        if (!v.is_null()){
            characters.push_back(as<r_vec<r_str_view>>(v));
        }
    }
    return internal::str_paste_list(characters, sep);
}

// C++ strings are assumed to be UTF-8
template <RAtomicVector T>
inline r_vec<r_str> str_paste(std::initializer_list<T> args, const char* sep = ""){
    return str_paste(args, r_str(sep));
}


inline r_str str_collapse(const r_vec<r_str>& x, r_str_view sep){
    if (is_na(sep)) [[unlikely]] {
        abort("`sep` must not be NA");
    }

    r_size_t n = x.length();

    internal::encoded_string res;

    for (r_size_t i = 0; i < n; ++i){
        r_str elem = x.view(i);
        if (is_na(elem)){
            return na<r_str>();
        }
        if (i > 0){
            res.append(sep);
        }
        res.append(r_str_view(elem));
    }
    return r_str(res.text, res.enc, false);
}

// C++ strings are assumed to be UTF-8
inline r_str str_collapse(const r_vec<r_str>& x, const char* sep = ""){
    return str_collapse(x, r_str(sep));
}


}

#endif
