#ifndef CPPALLY_R_DF_H
#define CPPALLY_R_DF_H

#include <cppally/r_vec.h>
#include <cppally/r_attrs.h>
#include <cppally/r_hash_names.h>

namespace cppally {

namespace internal {

struct no_checks_tag {};

// Lazily cache data frame class for re-use
inline r_vec<r_str_view> data_frame_class(){
    static r_vec<r_str_view> df_cls = r_vec<r_str_view>(1, r_str_view(cached_str<"data.frame">()));
    return df_cls;
}
    
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
    attr::set_attr(out, symbol::class_sym, data_frame_class());
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

    int cached_nrow;

    public:

    // Default constructor (empty data frame)
    r_df() : value(internal::new_df_impl(0)) {
        cached_nrow = 0;
    }

    // colnames are stored as the underlying VECSXP's names attribute, so we
    // share the names cache directly with `value` (no separate r_df cache).
    r_vec<r_str_view> colnames() const {
        return value.names();
    }

    int nrow() const noexcept {
        return cached_nrow;
    }
    int ncol() const noexcept {
        return value.length();
    }

    void set_nrow(int n) {
        attr::set_attr(value, symbol::row_names_sym, internal::create_row_names(n));
        cached_nrow = n;
    }

    template <RStringType U>
    void set_rownames(const r_vec<U>& rownames) {
        if (rownames.length() != nrow()) [[unlikely]] {
            abort("(set_rownames): `length(rownames)` must match `nrow()`");
        }
        attr::set_attr(value, symbol::row_names_sym, rownames);
    }

    template <RStringType U>
    void set_colnames(const r_vec<U>& colnames) {
        value.set_names(colnames);
    }

    private: 

    void validate_col_sizes(){
        int ncols = ncol();
        for (int i = 0; i < ncols; ++i){
            if (length(value.view(i)) != static_cast<r_size_t>(cached_nrow)) [[unlikely]] {
                abort("All data frame col lengths must match `nrow()`");
            }
        }
    }

    void validate_df(){

        if (value.is_null()) return;

        if (!Rf_isDataFrame(value)) [[unlikely]] {
          abort("SEXP must be of class 'data.frame' to be constructed as a data frame");
        }
        r_vec<r_str_view> names = value.names();
        if (names.length() != ncol()) [[unlikely]] {
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
        cached_nrow = get_nrow();
        #ifdef CPPALLY_CHECK_DATA_FRAMES
        validate_col_sizes();
        #endif
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

    explicit r_df(int nrows) : value(internal::new_df_impl(nrows)) {
        cached_nrow = nrows;
    }

    // Unsafe constructor (the list is expected to be a valid data frame with ALL necessary attributes)
    // You must also supply the correct nrows
    // Use mainly for tight loops where many r_df objects are constructed
    explicit r_df(const r_vec<r_sexp>& df, int nrows, internal::no_checks_tag) : value(df){
        cached_nrow = nrows;
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

    
    r_vec<r_str> rownames() const;
    template <internal::RSubscript U>
    r_df select(const r_vec<U>& cols) const;
    r_df get_row(int index) const;

    r_sexp get_col(int index) const {
        return value.get(index);
    }

    template <RStringType U>
    r_sexp view_col(const U& name) const {
        return value.view(name);
    }

    r_sexp view_col(std::string_view name) const {
        return view_col(r_str(name.data()));
    }

    template <RStringType U>
    r_sexp get_col(const U& name) const {
        return value.get(name);
    }

    r_sexp get_col(std::string_view name) const {
        return get_col(r_str(name.data()));
    }

    r_sexp view_col(int index) const {
        return value.view(index);
    }

    
    template <RObject col_t>
    void set_col(int index, const col_t& col) {
        value.set(index, r_sexp(col, internal::view_tag{}));
    }
    template <RObject col_t>
    void set_col(std::string_view colname, const col_t& col) {
        set_col(value.name_index(r_str(colname.data())), col);
    }

    // template <internal::RSubscript T, internal::RSubscript U>
    // void fill(const r_vec<T>& row_indices, const r_vec<U>& col_indices, const r_df& replacement);
    // template <internal::RSubscript T>
    // void fill(const r_vec<T>& row_indices, const r_df& replacement);
};

}

#endif
