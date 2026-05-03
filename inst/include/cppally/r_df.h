#ifndef CPPALLY_R_DF_H
#define CPPALLY_R_DF_H

#include <cppally/r_vec.h>
#include <cppally/r_attrs.h>
#include <cppally/r_factor.h>
#include <variant>
#include <vector>

namespace cppally {

namespace internal {

// view variant cached per r_df column
using col_view_t = std::variant<
    r_vec<r_lgl>,
    r_vec<r_int>,
    r_vec<r_int64>,
    r_vec<r_dbl>,
    r_vec<r_str>,
    r_vec<r_cplx>,
    r_vec<r_raw>,
    r_vec<r_date>,
    r_vec<r_psxct>,
    r_factors,
    r_vec<r_sexp>,
    r_sexp
>;

template <typename T, typename Variant>
inline constexpr bool is_variant_alt_v = false;

template <typename T, typename... Ts>
inline constexpr bool is_variant_alt_v<T, std::variant<Ts...>> = (std::is_same_v<T, Ts> || ...);
    
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

    mutable std::vector<internal::col_view_t> m_cols;
    mutable bool m_cache_built = false;

    // Defined in r_df_methods.h
    void build_col_cache() const;

    public:
    
    // Default constructor (empty data frame)
    r_df() : value(internal::new_df_impl(0)) {
        nrow_ = 0;
        build_col_cache();
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
        build_col_cache();
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

    // Defined in r_df_methods.h.
    // template <class F> decltype(auto) visit_col(int c, F&& f) const;
    template <class F> decltype(auto) view_col(int c, F&& f) const;
    // template <class F> void for_each_col(F&& f) const;
};

}

#endif
