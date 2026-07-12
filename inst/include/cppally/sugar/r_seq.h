#ifndef CPPALLY_R_SEQ_H
#define CPPALLY_R_SEQ_H

#include <cppally/r_utils.h>
#include <cppally/r_vec.h>
#include <cppally/sugar/r_math.h>
#include <cppally/r_coerce.h>

namespace cppally {

template <RNumber T, RNumber U>
auto sequence(int size, T from, U by){

    using common_t   = common_r_t<T, U>;

    if (size < 0){
        abort("size must be non-negative");
    }
    if (is_na(from)){
        abort("from contains NA values");
    }
    if (is_na(by)){
        abort("by contains NA values");
    }

    r_vec<common_t> out(size);
    int interrupt_counter = 0;

    for (int j = 0; j < size; ++j, ++interrupt_counter){
        if (interrupt_counter == 100000000) [[unlikely]] {
            check_user_interrupt();
            interrupt_counter = 0;
        }
        out.set(j, from + (j * by) );
    }
    return out;
}

// size of the sequence from..to stepping by `by`
template <RNumber T, RNumber U, RNumber V>
int seq_size(T from, U to, V by){
    auto del = to - from;
    // from == to with a zero increment is a well-defined length-1 sequence
    r_dbl ratio = ( ((del == 0) && (by == 0)).is_true() ) ? r_dbl(0.0) : del / by;
    if (is_na(ratio)){
        abort("seq_size: `to`, `from` and `by` must all be non-NA");
    }
    if ( (ratio < 0).is_true() ){
        abort("sequence length is negative, please check the sign of `by`");
    }
    // + 1e-10 absorbs floating point error before truncating
    r_dbl out_size = trunc(ratio + r_dbl(1e-10)) + r_dbl(1.0);
    return as<int>(out_size);
}

// first value given a size, an end point and an increment
template <RNumber T, RNumber U>
auto seq_start(int size, T to, U by){
    return to - max(size - r_int(1), r_int(0)) * by;
}

// last value given a size, a start point and an increment
template <RNumber T, RNumber U>
auto seq_end(int size, T from, U by){
    return from + max(size - r_int(1), r_int(0)) * by;
}

// increment given a size and the two end points
template <RNumber T, RNumber U>
r_dbl seq_increment(int size, T from, U to){
    if ( (from == to).is_true() || (size == 1) ){
        return r_dbl(0.0);
    }
    return (to - from) / max(size - r_int(1), r_int(0));
}

// build the sequence from..to stepping by `by`
// `to` only sets the length; widen from/by to its type so the right vector is built directly
template <RNumber T, RNumber U, RNumber V>
auto seq(T from, U to, V by){
    using out_t = common_r_t<T, U, V>;
    return sequence(seq_size(from, to, by), as<out_t>(from), as<out_t>(by));
}

}

#endif
