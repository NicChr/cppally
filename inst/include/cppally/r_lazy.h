#ifndef CPPALLY_R_LAZY_H
#define CPPALLY_R_LAZY_H

#include <cppally/r_concepts.h>
#include <cppally/r_sexp.h>

namespace cppally {

namespace internal {
template <int N>
struct name {
    char data[N];
    constexpr name(const char (&s)[N]) {
        for (int i = 0; i < N; ++i) data[i] = s[i];
    }
};

// Meyers-singleton method to cache R strings and symbols
template <name T>
inline r_sexp lazy_str_impl() {
    static r_sexp s = r_sexp(Rf_mkCharCE(T.data, CE_UTF8));
    return s;
}
template <name T>
inline SEXP lazy_sym_impl() {
    static SEXP s = Rf_installChar(lazy_str_impl<T>());
    return s;
}

}

}

#endif
