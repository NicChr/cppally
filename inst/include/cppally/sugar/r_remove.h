#ifndef CPPALLY_R_REMOVE_H
#define CPPALLY_R_REMOVE_H

#include <cppally/r_vec.h>
#include <cppally/r_coerce.h>
#include <cppally/sugar/r_subset.h>
#include <cppally/sugar/r_find.h>

namespace cppally {

template <RVal T>
r_vec<T> r_vec<T>::remove(const r_vec<T>& values) const {
    if (is_long()){
        r_vec<r_int64> keep = find<r_int64>(values, /*invert=*/ true);
        return subset(keep, false);
    } else {
        r_vec<r_int> keep = find<r_int>(values, /*invert=*/ true);
        return subset(keep, false);
    }
}

template <typename T>
requires requires(const T& a, const T& b) { 
  a.remove(b); 
}
T remove(const T& x, const T& values) {
    return x.remove(values);
}

template <RStringType U>
r_factors remove(const r_factors& x, const r_vec<U>& values){
    // // Remove codes directly
    r_vec<r_int> codes_to_remove = x.get_codes(values, r_int(-1));
    r_vec<r_int> new_codes = x.value.remove(codes_to_remove);
    r_factors out = x;
    out.set_codes(new_codes);
    return out;
}

inline r_factors remove(const r_factors& x, const r_factors& values){
    r_vec<r_int> new_codes = x.value.remove(values.new_codes(x.levels(), r_int(-1)));
    return r_factors(std::move(new_codes), x.levels());
}

template <typename T, typename U>
requires (is<T, r_sexp> || is<U, r_sexp>)
inline T remove(const T& x, const U& values);

}

#endif
