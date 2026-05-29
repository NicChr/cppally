#ifndef CPPALLY_R_COUNT_H
#define CPPALLY_R_COUNT_H

#include <cppally/r_vec.h>
#include <cppally/sugar/r_match.h>

namespace cppally {

template <RVal T>
r_size_t r_vec<T>::count(const r_vec<T>& values) const {

  r_size_t 
    n_values = values.length(),
    out = 0;

  if (n_values == 0){
    return out;
  } else if (n_values == 1){
    // Just simple count loop
    return count(values.view(0)); 
  }

  // Have to explicitly request 64-bit matches (annoying)
  if (is_long()){
    r_vec<r_int64> matches = match<r_int64>(*this, values);
    out = matches.length() - matches.count(na<r_int64>());
    out = std::max(out, r_size_t(0));
    return out;
  } else {
    r_vec<r_int> matches = match(*this, values);
    out = matches.length() - matches.count(na<r_int>());
    out = std::max(out, r_size_t(0));
    return out;
  }
}

// r_size_t count(const r_df& x, const r_df& rows){

//   r_size_t 
//     n_values = rows.nrow(),
//     out = 0;

//   if (n_values == 0){
//     return out;
//   }
//     r_vec<r_int> matches = match(x, rows);
//     out = matches.length() - matches.na_count(); // Number of matches
//     out = std::max(out, r_size_t(0));
//     return out;
// }



}

#endif
