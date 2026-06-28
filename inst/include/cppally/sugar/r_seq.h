#ifndef CPPALLY_R_SEQ_H
#define CPPALLY_R_SEQ_H

#include <cppally/r_utils.h>
#include <cppally/r_vec.h>
#include <cppally/sugar/r_math.h>
#include <cppally/r_coerce.h>

namespace cppally {

namespace internal {
template <typename T>
concept RPlainNumber = any<T, r_int, r_int64, r_dbl>;
}

template <internal::RPlainNumber T, internal::RPlainNumber U>
r_vec<common_r_t<T, U>> sequence(r_int size, T from, U by){

    using common_t   = common_r_t<T, U>;
    using common_cpp = unwrap_t<common_t>;

    if (is_na(size)){
        abort("size must not be NA");
    }
    if ( (size < 0).is_true() ){
        abort("size must be non-negative");
    }

    if (is_na(from)){
        abort("from contains NA values");
    }
    if (is_na(by)){
        abort("by contains NA values");
    }

    auto start     = unwrap(from);
    auto increment = unwrap(by);

    auto seq_size = unwrap(size);
    auto out = r_vec<common_t>(seq_size);

    common_cpp current_val = start;
    r_size_t interrupt_counter = 0;

    for (r_size_t j = 0; j < seq_size; ++j, ++interrupt_counter){
        if (interrupt_counter == 100000000) [[unlikely]] {
            check_user_interrupt();
            interrupt_counter = 0;
        }
        if constexpr (RIntegerType<common_t>){
            out.set(j, common_t(current_val));
            if (j < seq_size - 1){
                if (__builtin_add_overflow(current_val, increment, &current_val)) [[unlikely]] {
                    abort("Integer overflow in sequence, please use doubles");
                }
            }
        } else {
            out.set(j, common_t( start + (j * increment) ));
        }
    }
    return out;
}

// size of the sequence from..to stepping by `by`
template <internal::RPlainNumber T, internal::RPlainNumber U, internal::RPlainNumber V>
r_int seq_size(T from, U to, V by){
    auto del = to - from;
    // from == to with a zero increment is a well-defined length-1 sequence
    r_dbl ratio = ( ((del == 0) && (by == 0)).is_true() ) ? r_dbl(0.0) : del / by;
    if ( (ratio < 0).is_true() ){
        abort("sequence length is negative, please check the sign of `by`");
    }
    // + 1e-10 absorbs floating point error before truncating
    return as<r_int>( trunc(ratio + r_dbl(1e-10)) + r_dbl(1.0) );
}

// first value given a size, an end point and an increment
template <internal::RPlainNumber T, internal::RPlainNumber U>
auto seq_start(r_int size, T to, U by){
    return to - max(size - r_int(1), r_int(0)) * by;
}

// last value given a size, a start point and an increment
template <internal::RPlainNumber T, internal::RPlainNumber U>
auto seq_end(r_int size, T from, U by){
    return from + max(size - r_int(1), r_int(0)) * by;
}

// increment given a size and the two end points
template <internal::RPlainNumber T, internal::RPlainNumber U>
r_dbl seq_increment(r_int size, T from, U to){
    if ( (from == to).is_true() || (size == r_int(1)).is_true() ){
        return r_dbl(0.0);
    }
    return (to - from) / max(size - r_int(1), r_int(0));
}

// build the sequence from..to stepping by `by`
template <internal::RPlainNumber T, internal::RPlainNumber U, internal::RPlainNumber V>
r_vec<common_r_t<T, V>> seq(T from, U to, V by){
    return sequence(seq_size(from, to, by), from, by);
}

}

#endif
