#ifndef CPPALLY_R_PASTE_H
#define CPPALLY_R_PASTE_H

#include <cppally/r_vec.h>
#include <cppally/sugar/r_list_helpers.h>
#include <initializer_list>
#include <string>
#include <vector>

// Concatenate vectors of strings

namespace cppally {

namespace internal {

// Concatenate all character vectors of a list together
inline r_vec<r_str> str_paste_list(const std::vector<r_vec<r_str_view>>& x, const char* sep = ""){
    
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
    std::string buf = "";
    r_str_view e;
    r_vec<r_str_view> out(n);

    for (r_size_t j = 0; j < n; ++j){
        for (r_size_t i = 0; i < n_vecs; ++i){
            e = x[i].view(idx[i]);
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
        // Advance every counter, even on early break, to keep recycling aligned
        for (r_size_t i = 0; i < n_vecs; ++i){
            recycle_index(idx[i], lens[i]);
        }
    }
    return as<r_vec<r_str>>(out);
}

}

template <typename... Args>
r_vec<r_str> str_paste(Args... args){
    r_vec<r_sexp> vectors = make_vec<r_sexp>(args...).remove(r_null);
    std::vector<r_vec<r_str_view>> characters = as<std::vector<r_vec<r_str_view>>>(vectors);
    return internal::str_paste_list(characters);
}


template <RAtomicVector T>
inline r_vec<r_str> str_paste(std::initializer_list<T> args, const char* sep = ""){
    std::vector<r_vec<r_str_view>> characters;
    characters.reserve(args.size());
    for (const T& v : args){
        if (!v.is_null()){
            characters.push_back(as<r_vec<r_str_view>>(v));
        }
    }
    return internal::str_paste_list(characters, sep);
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
