
#ifndef CPPALLY_R_DF_H
#define CPPALLY_R_DF_H

#include <cppally/r_vec.h>
#include <cppally/r_attrs.h>
#include <cppally/r_list_helpers.h>

namespace cppally {

namespace internal {

inline r_vec<r_int> create_row_names(int n){

    if (n == 0){
        return r_vec<r_int>();
    } else {
        r_vec<r_int> out(2); 
        out.set(0, na<r_int>());
        out.set(1, r_int(-n));
        return out;
    }
}


inline r_vec<r_sexp> new_df_impl(const r_vec<r_sexp>& cols, bool recycle, int nrows){

    r_size_t n = cols.length();
    r_vec<r_sexp> out(n);

    if (nrows < 0){
        abort("Supply a valid `nrows`");
    }

    if (recycle){
        for (r_size_t i = 0; i < n; ++i){
            out.set(i, 
            view_sexp(cols.view(i), [nrows](const auto& vec) -> r_sexp {
                if constexpr (!RVector<decltype(vec)> && !RMetaVector<decltype(vec)>){
                    abort("Don't know how to visit this r_sexp!");
                } else {
                    return r_sexp(vec.rep_len(nrows), internal::view_tag{});
                }
        })
    );
        }
    } else {
        for (r_size_t i = 0; i < n; ++i){
            out.set(i, cols.view(i));
        }
    }

    // Always provide names
    r_vec<r_str_view> names = cols.names();
    if (names.is_null()){
        names = r_vec<r_str_view>(n, cached_str<"">());
    }
    out.set_names(names); 

    r_vec<r_int> row_names = create_row_names(nrows);
    attr::set_attr(out, symbol::class_sym, r_vec<r_str_view>(1, r_str_view(cached_str<"data.frame">())));
    attr::set_attr(out, symbol::row_names_sym, row_names);
    return out;
}

inline r_vec<r_sexp> new_df_impl(const r_vec<r_sexp>& cols, bool recycle = true){
    r_size_t nrows;
    if (recycle){
        nrows = internal::recycle_size(cols);
    } else {
        if (cols.length() == 0){
            nrows = 0;
        } else {
            nrows = cols.view(0).length();
        }
    }
    return new_df_impl(cols, recycle, nrows);
}

inline r_vec<r_sexp> new_df_impl(int nrows){

    r_vec<r_sexp> out{};
    r_vec<r_int> row_names = create_row_names(nrows);
    out.set_names(r_vec<r_str_view>());
    attr::set_attr(out, symbol::class_sym, r_vec<r_str_view>(1, r_str_view(cached_str<"data.frame">())));
    attr::set_attr(out, symbol::row_names_sym, row_names);
    return out;
}
}

struct r_df {

    r_vec<r_sexp> value;
    
    // Default constructor (empty data frame)
    r_df() : value(internal::new_df_impl(0)) {}

    private: 

    void validate_col_sizes(const r_vec<r_sexp>& x){
        r_size_t n = x.length();
        if (n > 0){
            r_size_t init_size = x.view(0).length();
            if (init_size > unwrap(r_limits<r_int>::max())) [[unlikely]] {
                abort("Data frames can only contain short vectors, please check");
            }
            for (r_size_t i = 0; i < n; ++i){
                if (init_size != x.view(i).length()){
                    abort("All lengths of a data frame must be equal");
                }
            }
        }
    }

    void validate_df(SEXP x){

        if (TYPEOF(x) == NILSXP) return;

        if (TYPEOF(x) != VECSXP){
          abort("SEXP must have vector storage to be constructed as a data frame");
        }
        if (!Rf_inherits(x, "data.frame")){
          abort("SEXP must be of class 'data.frame' to be constructed as a data frame");
        }
        r_vec<r_str_view> names = attr::get_old_names(x);
        if (names.length() != Rf_xlength(x)){
          abort("data frame length of names must match ncol");
        }
        SEXP row_names = Rf_protect(Rf_getAttrib(x, symbol::row_names_sym));
        if (TYPEOF(row_names) != INTSXP && TYPEOF(row_names) != STRSXP){
            abort("rownames must be an integer or character vector");
        }
        validate_col_sizes(r_vec<r_sexp>(x, internal::view_tag{}));
        Rf_unprotect(1);
      }

    public: 

    // Constructor from existing SEXP
    explicit r_df(SEXP s) : value(s) {validate_df(value);}
    explicit r_df(SEXP s, internal::view_tag) : value(s, internal::view_tag{}) {validate_df(value);}

    explicit r_df(const r_sexp& s) : value(s) {validate_df(value);}
    explicit r_df(const r_sexp& s, internal::view_tag) : value(s, internal::view_tag{}) {validate_df(value);}
    
    // Constructor from list of cols
    // Supply a nrows value for a custom recycle length
    explicit r_df(const r_vec<r_sexp>& cols, bool recycle = true) : value(internal::new_df_impl(cols, recycle)){}
    explicit r_df(const r_vec<r_sexp>& cols, bool recycle, int nrows) : value(internal::new_df_impl(cols, recycle, nrows)){}

    // Implicit conversion to SEXP
    operator SEXP() const noexcept { return unwrap(value); }

    private: 

    // For methods that just return a non-factor (like length())
    #define FORWARD_METHOD(NAME)                               \
        template <typename... Args>                            \
        decltype(auto) NAME(Args&&... args) const {            \
            return value.NAME(std::forward<Args>(args)...);    \
        }

    public: 

    // Inherit standard methods from r_vec<>

    FORWARD_METHOD(is_null)
    FORWARD_METHOD(data)
    FORWARD_METHOD(address)
  
    // Undefine the macros so they don't leak out of the struct
    #undef FORWARD_METHOD

    r_vec<r_str_view> colnames() const {
        return value.names();
    }

    int nrow() const noexcept {
        return Rf_length(Rf_getAttrib(value, symbol::row_names_sym));
    }
    int ncol() const noexcept {
        return value.length();
    }
    template <RStringType U>
    void set_colnames(const r_vec<U>& colnames) {
        value.set_names(colnames);
    }
    template <internal::RSubscript U>
    r_df subset(const r_vec<U>& indices) const {
        if (ncol() == 0){
            // We don't have a function atm that tells us what the resulting size should be here
            // So subset a dummy vector
            r_vec<r_int> dummy(nrow()); // Uninitialised dummy vector
            return r_df(r_vec<r_sexp>(), false, dummy.subset(indices).length());
        }
        r_vec<r_sexp> out(ncol());
        internal::view_elements(value, [&]<typename T>(r_size_t i, const T& elem) {
            out.set(i, elem.subset(indices));
        });
        return r_df(out, false, out.view(0).length());
    }
};

}

#endif
