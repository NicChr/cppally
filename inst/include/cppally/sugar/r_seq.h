#ifndef CPPALLY_R_SEQ_H
#define CPPALLY_R_SEQ_H

#include <cppally/r_utils.h>
#include <cppally/r_vec.h>
#include <cppally/sugar/r_stats.h>
#include <cppally/r_coerce.h>

namespace cppally {

template <RNumericType T, RNumericType U>
r_vec<r_sexp> sequences(const r_vec<r_int>& size, const r_vec<T>& from, const r_vec<U>& by){

    using common_t = common_r_t<T, U>;
    using commont_cpp_t = unwrap_t<common_t>;

    int size_n = size.length();
    int from_n = from.length();
    int by_n = by.length();

    if (size_n > 0 && (from_n <= 0 || by_n <= 0)){
        abort("from and by must both have length > 0");
    }
    
    r_int min_size = min(size, /*na_rm=*/ false);

    if (is_na(min_size)){
        abort("size must not contain NA values");
    }
    if (min_size < 0){
        abort("size must be a vector of non-negative integers");
    }
    
    r_size_t interrupt_counter = 0;
    
    r_vec<r_sexp> out(size_n);

    if (size_n > 0){
        
        for (int i = 0, bi = 0, fi = 0; i < size_n;
        recycle_index(bi, by_n),
        recycle_index(fi, from_n),
        ++i){
            auto seq_size = unwrap(size.get(i));
            auto curr_seq = r_vec<common_t>(seq_size);
            auto start = unwrap(from.get(fi));
            auto increment = unwrap(by.get(bi));
            if (is_na(start)){
                abort("from contains NA values");
            }
            if (is_na(increment)){
                abort("by contains NA values");
            }

            commont_cpp_t current_val = start;

            for (int j = 0; j < seq_size; ++j, ++interrupt_counter){
                if (interrupt_counter == 100000000){
                    check_user_interrupt();
                    interrupt_counter = 0;
                }
                if constexpr (RIntegerType<common_t>){
                    curr_seq.set(j, common_t(current_val));

                    // Only add if we need the next value
                    if (j < seq_size - 1) {
                        if (__builtin_add_overflow(current_val, increment, &current_val)) [[unlikely]] {
                            abort("Integer overflow in sequence %d, please use doubles", i + 1);
                        }
                    }
                } else {
                    curr_seq.set(j, common_t( start + (j * increment) ));
                }

            }
            out.set(i, curr_seq.sexp);
        }
    }
    return out;
}

}

#endif
