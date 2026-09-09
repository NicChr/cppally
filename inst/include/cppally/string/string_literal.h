#ifndef CPPALLY_STRING_LITERAL_H
#define CPPALLY_STRING_LITERAL_H

#include <cppally/r_concepts.h>
#include <cppally/r_sexp/r_sexp.h>
#include <string_view>

namespace cppally {

template <int N>
struct string_literal {
    char data[N];

    // N for NUL terminator count
    static constexpr int size = N - 1;

    consteval string_literal(const char (&s)[N]) {
        for (int i = 0; i < N; ++i){
            data[i] = s[i];
        }
        if (data[N - 1] != '\0'){
            throw "string_literal must be NUL-terminated";
        }
    }

    constexpr std::string_view view() const noexcept {
        return std::string_view(data, N - 1);
    }

    // Concatenation - safe because we know the size at compile-time
    template <int M>
    consteval auto concat(const string_literal<M>& other) const {
        
        constexpr int new_size = N + M - 1;
        char buf[new_size]{}; // Only need space for 1-nul terminator, so subtract 1

        for (int i = 0; i < N - 1; ++i){
            buf[i] = data[i];
        }
        for (int i = N - 1; i < new_size; ++i){
            buf[i] = other.data[i - (N - 1)];
        }
        return string_literal<new_size>(buf);
    }

    template <int M>
    consteval auto concat(const char (&s)[M]) const {
        return concat(string_literal<M>(s));
    }
};

namespace internal {

// Meyers-singleton method to cache R strings and symbols
template <string_literal T>
inline r_sexp lazy_str_impl() {
    static r_sexp& s = *new r_sexp(Rf_mkCharCE(T.data, CE_UTF8));
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
