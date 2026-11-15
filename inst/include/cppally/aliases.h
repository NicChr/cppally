#ifndef CPPALLY_ALIASES_H
#define CPPALLY_ALIASES_H

#include <cppally/r_concepts.h>

namespace cppally {

// cppally type aliases

// R logical
using r_logical = r_lgl;
// R integer
using r_integer = r_int;
// R 64-bit integer
using r_integer64 = r_int64;
// R double
using r_double = r_dbl;
// R string
using r_string = r_str;
// R string view
using r_string_view = r_str_view;
// R complex number
using r_complex = r_cplx;
// R POSIXct (UNIX-based date-time)
using r_posixct = r_psxct;

// R symbol
using r_symbol = r_sym;

// R vector
template <RVal T>
using r_vector = r_vec<T>;

// R factor
using r_factor = r_factors;

// R data frame
using r_data_frame = r_df;

}

#endif
