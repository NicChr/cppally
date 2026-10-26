#ifndef CPPALLY_R_GROUP_FUNCTIONALS_H
#define CPPALLY_R_GROUP_FUNCTIONALS_H

#include <cppally/vector/r_vector.h>
#include <cppally/coerce.h>
#include <cppally/group/groups.h>
#include <cppally/r_function.h>
#include <vector>
#include <algorithm>

// Methods for reduction by group
// Sum by group example: `reduce_by_group(x, groups, std::plus<>{})` (cppally ensures arithmetic is overflow safe)

namespace cppally {

namespace internal {

template <bool SeedFromFirst, RVal T, typename Acc, typename F>
void reduce_by_group_impl(const r_vec<T>& x, const groups& g, std::vector<Acc>& accs, F& fn, bool na_skip){

    r_size_t n = x.length();

    const int* RESTRICT p_id = g.ids.data();

    // Sorted IDs make each group one contiguous run, so the accumulator can stay
    // in a register for the whole run rather than round-tripping through `accs`
    // on every element. `run_end` indexes with int, so long vectors keep the generic loop
    if (g.sorted && !x.is_long()){

        int n_int = static_cast<int>(n);
        int i = 0;

        while (i < n_int){

            int gid = p_id[i];
            int end = run_end(p_id, i, n_int);
            int k = i;

            i = end;

            if constexpr (SeedFromFirst){
                if (na_skip){
                    while (k < end && is_na(x.view(k))){
                        ++k;
                    }
                    if (k == end){
                        continue; // Whole group skipped, so it keeps its starting value
                    }
                }
            }

            Acc acc = [&]{
                if constexpr (SeedFromFirst){
                    return as<Acc>(x.view(k));
                } else {
                    return std::move(accs[gid]);
                }
            }();

            if constexpr (SeedFromFirst){
                ++k; // Already folded in as the seed
            }

            for (; k < end; ++k){
                if (na_skip && is_na(x.view(k))){
                    continue;
                }
                acc = fn(std::move(acc), x.view(k));
            }
            accs[gid] = std::move(acc);
        }
        return;
    }

    // Unsorted: a group's elements are scattered, so first-touch has to be tracked
    std::vector<bool> seen;

    if constexpr (SeedFromFirst){
        seen.assign(g.n_groups, false);
    }

    for (r_size_t i = 0; i < n; ++i){
        if (na_skip && is_na(x.view(i))){
            continue;
        }
        int gid = p_id[i];
        if constexpr (SeedFromFirst){
            if (!seen[gid]){
                accs[gid] = as<Acc>(x.view(i));
                seen[gid] = true;
                continue;
            }
        }
        accs[gid] = fn(std::move(accs[gid]), x.view(i));
    }
}

}

// Reduce each group to a single value: out[g] = left-to-right fold of fn over x's elements in group g
// Groups with no elements (e.g. unused factor levels) are NA
template <RVal T, typename F>
requires std::invocable<F&, T, T>
auto reduce_by_group(const r_vec<T>& x, const groups& g, F fn, bool na_skip = false) {

    using acc_t = std::remove_cvref_t<std::invoke_result_t<F&, T, T>>;
    static_assert(!internal::fold_result<acc_t>::stops, "`reduce_by_group` combiner must not short-circuit (no `done()`)");

    internal::check_groups_span_data(x, g);

    std::vector<acc_t> accs(g.n_groups, na<acc_t>());

    internal::reduce_by_group_impl<true>(x, g, accs, fn, na_skip);

    return as<r_vec<acc_t>>(accs);
}

// Groups with no elements reduce to `init`
template <RVal T, RVal Acc, typename F>
requires std::invocable<F&, Acc, T>
auto reduce_by_group(const r_vec<T>& x, const groups& g, F fn, Acc init, bool na_skip = false) {

    using acc_t = std::remove_cvref_t<std::invoke_result_t<F&, Acc, T>>;
    static_assert(!internal::fold_result<acc_t>::stops, "`reduce_by_group` combiner must not short-circuit (no `done()`)");

    internal::check_groups_span_data(x, g);

    std::vector<acc_t> accs(g.n_groups, as<acc_t>(init));

    internal::reduce_by_group_impl<false>(x, g, accs, fn, na_skip);

    return as<r_vec<acc_t>>(accs);
}

// Apply fn to each group, in group ID order
template <RVector T, typename F>
requires (std::invocable<F&, const r_vec<typename T::data_type>&>)
auto apply_by_group(const T& x, const groups& g, F fn) {

    internal::check_groups_span_data(x, g);

    if (x.is_long()) [[unlikely]] {
        abort("`apply_by_group`: long vectors are not supported");
    }

    using data_t = typename T::data_type;
    using ret_t = std::remove_cvref_t<std::invoke_result_t<F&, const r_vec<data_t>&>>;

    int n = static_cast<int>(x.length());
    int ng = g.n_groups;

    // If N groups is 1, just call the function on all the data
    if (ng == 1){
        return r_vec<ret_t>( {fn(x)} );
    }

    r_vec<r_int> group_sizes = g.counts();

    std::vector<int> group_starts;
    group_starts.reserve(ng);
    int max_size = 0;
    for (int k = 0, start = 0; k < ng; ++k){
        int group_size = group_sizes.get(k);
        group_starts.push_back(start);
        start += group_size;
        max_size = std::max(max_size, group_size);
    }

    T data_sorted_by_group_buffer(0);

    // We want to iterate over data sorted by group order
    // such that data_sorted_by_group[i] == x[order(g)[i]]
    if (!g.sorted){
        data_sorted_by_group_buffer = T(n);
        std::vector<int> pos(group_starts);
        for (int i = 0; i < n; ++i){
            data_sorted_by_group_buffer.set(pos[unwrap(g.ids.get(i))]++, x.view(i));
        }
    }

    const T& data_sorted_by_group = g.sorted ? x : data_sorted_by_group_buffer;

    #ifdef CPPALLY_HAS_RESIZABLE_VECTORS
        T buffer(0, resizable{}, max_size);
    #else
        // A buffer for each unique group size
        std::vector<int> buffer_slot(max_size + 1, -1);
        std::vector<T> buffers;
    #endif

    r_vec<ret_t> out(ng);

    for (int k = 0; k < ng; ++k){

        int size = group_sizes.get(k);

        #ifdef CPPALLY_HAS_RESIZABLE_VECTORS
            // fn may have kept the buffer, so don't overwrite it
            if (!buffer.is_exclusive()) [[unlikely]] {
                buffer = T(0, resizable{}, max_size);
            }
            buffer.set_length(size);
        #else
            int& slot = buffer_slot[size];
            if (slot < 0){
                slot = static_cast<int>(buffers.size());
                buffers.emplace_back(size);
            } else if (!buffers[slot].is_exclusive()) [[unlikely]] {
                // fn may have kept the buffer, so don't overwrite it
                buffers[slot] = T(size);
            }
            T& buffer = buffers[slot];
        #endif

        // Every group is a contiguous run of `data_sorted_by_group`
        r_copy_n(buffer, data_sorted_by_group, 0, size, group_starts[k]);
        out.set(k, fn(buffer));
    }
    return out;
}

namespace internal {

// Call an R function on a buffer without leaving a reference to the buffer behind, so it stays exclusive and re-usable.
// Binds x to a symbol rather than putting it in the call, as builtins like sum() duplicate their call
struct r_function_caller {
    r_sexp env;
    r_sexp call;
    explicit r_function_caller(const r_function& fn) :
        env(safe[R_NewEnv](env::global_env, FALSE, 1)),
        call(safe[Rf_lang2](fn, cached_sym<".cppally_x">())) {
        safe[Rf_defineVar](cached_sym<".cppally_x">(), R_NilValue, env);
    }
    r_sexp operator()(SEXP x) const {
        Rf_defineVar(cached_sym<".cppally_x">(), x, env);
        r_sexp res = eval(call, env);
        Rf_defineVar(cached_sym<".cppally_x">(), R_NilValue, env);
        return res;
    }
};

}

// Apply an R function by group
// Not as fast as applying a C++ function/lambda but can be useful in some cases
template <typename ret_t, RVector T>
auto apply_by_group(const T& x, const groups& g, r_function fn) {
    internal::r_function_caller call_fn(fn);
    return apply_by_group(x, g, [&](const auto& vec){
        return as<ret_t>(call_fn(vec));
    });
}

}

#endif
