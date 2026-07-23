#ifndef CPPALLY_R_LAZY_H
#define CPPALLY_R_LAZY_H

#include <cppally/r_concepts.h>
#include <cppally/r_sexp.h>

namespace cppally {

template <int N>
struct string_literal {
    char data[N];
    consteval string_literal(const char (&s)[N]) {
        for (int i = 0; i < N; ++i){
            data[i] = s[i];
        }
    }
};

namespace internal {

// Meyers-singleton method to cache R strings and symbols
template <string_literal T>
inline r_sexp lazy_str_impl() {
    static r_sexp s = r_sexp(Rf_mkCharCE(T.data, CE_UTF8));
    return s;
}
template <string_literal T>
inline SEXP lazy_sym_impl() {
    static SEXP s = Rf_installChar(lazy_str_impl<T>());
    return s;
}

}

}

#endif
