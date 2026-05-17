#ifndef CPPALLY_R_FIND_H
#define CPPALLY_R_FIND_H

#include <cppally/r_vec.h>
#include <cppally/sugar/r_match.h>
#include <cppally/sugar/r_stats.h>

namespace cppally {

template <RVal T>
template <internal::RNumericSubscript V>
r_vec<V> r_vec<T>::find(const r_vec<T>& values, bool invert) const {

  if constexpr (is<V, r_int>){
    if (is_long()){
      abort("`x` is a long vector, please use `find<r_int64>` for 64-bit locations");
    }
  }

  r_size_t n_values = values.length();

  if (n_values == 0){
    if (invert){
      r_size_t n = length();
      r_vec<V> out(n);
      out.iota();
      return out;
    } else {
      return r_vec<V>();
    }
  } else if (n_values == 1){
    // Just simple find loop
    return find<V>(values.view(0), /*invert=*/ invert); 
  }
  r_vec<V> matches = match<V>(*this, values);
  return matches.template find<V>(na<V>(), !invert);
}

template <typename T, internal::RNumericSubscript V = r_int>
requires requires(const T& a, const T& b, bool invert) { 
  a.template find<V>(b, invert); 
}
r_vec<V> find(const T& x, const T& values, bool invert = false) {
    return x.template find<V>(values, invert);
}

template <RStringType U>
r_vec<r_int> find(const r_factors& x, const r_vec<U>& values, bool invert = false){
    r_vec<U> lvls = r_vec<U>(unwrap(x.levels()));
    r_vec<r_int> matches = match(values, lvls);
    matches += r_int(1);
    return x.value.find(matches, invert);
}

template <typename T, typename U, internal::RNumericSubscript V = r_int>
requires (is<T, r_sexp> || is<U, r_sexp>)
inline r_vec<V> find(const T& x, const U& values, bool invert = false) {
  return CPPALLY_VIEW_PAIR_AND_APPLY(x, values, r_vec<V>, find, invert);
}

}

#endif
