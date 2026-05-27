#ifndef CPPALLY_R_PASTE_H
#define CPPALLY_R_PASTE_H

#include <cppally/r_vec.h>
#include <cppally/r_list_helpers.h>
#include <string>
#include <vector>

// Concatenate vectors of strings

namespace cppally {

namespace internal {

// Concatenate all character vectors of a list together
inline r_vec<r_str> str_paste_list(const r_vec<r_sexp>& x, const char* sep = ""){
    
    r_vec<r_sexp> characters = x.remove(r_null);
    r_size_t n_vecs = characters.length();
    if (n_vecs == 0){
        return r_vec<r_str>();
    }

    r_size_t n = recycle_size(characters);
    if (n == 0){
        return r_vec<r_str>();
    }

    // Pre-cast to character vectors and recycle all to common length
    std::vector<r_vec<r_str_view>> vecs;
    vecs.reserve(n_vecs);
    for (r_size_t i = 0; i < n_vecs; ++i){
        r_vec<r_str_view> v = as<r_vec<r_str_view>>(characters.view(i)).rep_len(n);
        vecs.push_back(std::move(v));
    }

    std::string buf = "";
    r_str_view e;
    r_vec<r_str_view> out(n);

    for (r_size_t j = 0; j < n; ++j){
        for (r_size_t i = 0; i < n_vecs; ++i){
            e = vecs[i].view(j);
            if (is_na(e)){
                break;
            }
            if (i > 0){
                buf += sep;
            }
            buf += e.c_str();
        }
        out.set(j, is_na(e) ? na<r_str_view>() : c_str_to_r_str_view(buf.c_str()));
        buf.clear();
    }
    return as<r_vec<r_str>>(out);
}

}

template <typename... Args>
r_vec<r_str> str_paste(Args... args){
    return internal::str_paste_list(make_vec<r_sexp>(args...));
}
inline r_str str_collapse(const r_vec<r_str>& x, const char* sep = ""){
    r_size_t n = x.length();

    std::string res = "";

    for (r_size_t i = 0; i < n; ++i){
        r_str elem = x.view(i);
        if (is_na(elem)){
            return na<r_str>();
        }
        if (i > 0){
            res += sep;
        }
        res += elem.c_str();
    }
    return r_str(res.c_str());
}


}

#endif
