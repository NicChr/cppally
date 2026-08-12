#ifndef CPPALLY_R_REDUCE_BY_GROUP_H
#define CPPALLY_R_REDUCE_BY_GROUP_H

#include <cppally/r_vec.h>
#include <cppally/r_coerce.h>
#include <cppally/groups/groups.h>
#include <vector>

// Methods for reduction by group
// Sum by group example: `reduce_by_group(x, groups, std::plus<>{})` (cppally ensures arithmetic is overflow safe)

namespace cppally {

namespace internal {

template <RComposite T>
void check_groups_span_data(const T& x, const groups& g){
    if (x.length() != g.ids.length()) [[unlikely]] {
        abort("check_groups_span_data: data and group IDs must have equal length");
    }
}

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

}

#endif
