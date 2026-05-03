#ifndef CPPALLY_R_DF_H
#define CPPALLY_R_DF_H

#include <cppally/r_vec.h>
#include <cppally/r_attrs.h>

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

    private: 

    int get_nrow() const noexcept {
        return Rf_length(Rf_getAttrib(value, symbol::row_names_sym));
    }

    int nrow_;

    public:
    
    // Default constructor (empty data frame)
    r_df() : value(internal::new_df_impl(0)) {
        nrow_ = 0;
    }

    r_vec<r_str_view> colnames() const {
        return value.names();
    }

    int nrow() const noexcept {
        return nrow_;
    }
    int ncol() const noexcept {
        return value.length();
    }

    void set_nrow(int n) {
        attr::set_attr(value, symbol::row_names_sym, internal::create_row_names(n));
        nrow_ = n;
    }

    template <RStringType U>
    void set_colnames(const r_vec<U>& colnames) {
        attr::set_old_names(value ,colnames);
    }

    private: 

    void validate_col_sizes(){
        int ncols = ncol();
        for (int i = 0; i < ncols; ++i){
            if (length(value.view(i)) != static_cast<r_size_t>(nrow_)) [[unlikely]] {
                abort("All data frame col lengths must match `nrow()`");
            }
        }
    }

    void validate_df(){

        if (value.is_null()) return;

        if (!Rf_isDataFrame(value)) [[unlikely]] {
          abort("SEXP must be of class 'data.frame' to be constructed as a data frame");
        }
        r_vec<r_str_view> names = attr::get_old_names(value);
        if (names.length() != value.length()) [[unlikely]] {
          abort("length of colnames must match `ncol()`");
        }
        SEXP row_names = Rf_protect(Rf_getAttrib(value, symbol::row_names_sym));
        if (TYPEOF(row_names) != INTSXP && TYPEOF(row_names) != STRSXP) [[unlikely]] {
            abort("rownames must be an integer or character vector");
        }
        Rf_unprotect(1);
      }

      void init_df() {
        validate_df();
        nrow_ = get_nrow();
        validate_col_sizes();
      }

    public: 

    // Constructor from existing SEXP
    explicit r_df(SEXP s) : value(s) {
        init_df();
    }
    explicit r_df(SEXP s, internal::view_tag) : value(s, internal::view_tag{}) {
        init_df();
    }
    explicit r_df(const r_sexp& s) : value(s) {
        init_df();
    }
    explicit r_df(const r_sexp& s, internal::view_tag) : value(s, internal::view_tag{}) {
        init_df();
    }
    
    // Forward declarations, defined in r_df_methods.h
    explicit r_df(const r_vec<r_sexp>& cols, bool recycle = true);
    explicit r_df(const r_vec<r_sexp>& cols, bool recycle, int nrows);
    template <RScalar T>
    explicit r_df(const r_vec<T>& col);
    explicit r_df(const r_factors& col);

    // Implicit conversion to SEXP
    operator SEXP() const noexcept { return unwrap(value); }

    private: 

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

    template <internal::RSubscript U>
    r_df select(const r_vec<U>& cols) const;

    inline r_df get_row(int index) const;
    // inline r_vec<r_sexp> get_row(int index) const; // Probably faster
    inline r_sexp get_col(int index) const;
    inline r_sexp get_col(const char* name) const;
    template <RStringType U>
    inline r_sexp get_col(U name) const;
};

}

#endif
