#ifndef CPPALLY_R_GROUP_FUNCTIONALS_H
#define CPPALLY_R_GROUP_FUNCTIONALS_H

#include <cppally/r_vec.h>
#include <cppally/r_coerce.h>
#include <cppally/r_length.h>
#include <cppally/groups/groups.h>
#include <vector>
#include <algorithm>

// Methods for reduction by group
// Sum by group example: `reduce_by_group(x, groups, std::plus<>{})` (cppally ensures arithmetic is overflow safe)

namespace cppally {

namespace internal {

template <RComposite T>
void check_groups_span_data(const T& x, const groups& g){
    if (length(x) != g.ids.length()) [[unlikely]] {
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

// Efficiently apply a function by-group.
template <RVector T, typename F>
requires (std::invocable<F&, const r_vec<typename T::data_type>&>)
auto apply_by_group(const T& x, const groups& g, F fn) {

    internal::check_groups_span_data(x, g);

    // Group positions are int throughout, as everywhere else in `groups`
    if (x.is_long()) [[unlikely]] {
        abort("`apply_by_group`: long vectors are not supported");
    }

    using data_t = typename T::data_type;
    using ret_t = std::remove_cvref_t<std::invoke_result_t<F&, const r_vec<data_t>&>>;
    // using ret_t = std::conditional_t<RComposite<raw_ret_t>, r_sexp, raw_ret_t>;

    int n = static_cast<int>(x.length());
    int ng = g.n_groups;

    // If N groups is 1, just call the function on all the data
    if (ng == 1){
        r_vec<ret_t> out(1);
        out.set(0, fn(x));
        return out;
    }

    // One buffer, two phases: group sizes for the sort below, then scanned in
    // place into group ends. Every group's end is the next group's start, so
    // ends alone gives both bounds - group j spans [ends[j - 1], ends[j]),
    // and group 0 starts at 0
    r_vec<r_int> group_bounds = g.counts();
    group_bounds.ensure_exclusive(); // counts() may be cached, so ensure data is safe to overwrite
    int* RESTRICT p_bounds = group_bounds.data();

    // Group locations sorted by group size
    // Sort by group size so that we can reuse the same buffer and avoid R vector allocations
    // e.g. if group_size[i] == group_size[i + 1], we can reuse the same buffer (R vectors are fixed-length once allocated).
    // This is a nice way to get around the problem of large overhead of repeated R vector allocations
    // because in high cardinality situations (where length(unique(g)) is close to length(x)),
    // We end up reusing the buffer frequently
    // and in low cardinality situations (where length(unique(g)) is small), there aren't many
    // allocations anyway.
    std::vector<int> group_order;
    group_order.reserve(ng);
    for (int j = 0; j < ng; ++j){
        group_order.push_back(j);
    }
    std::sort(group_order.begin(), group_order.end(), [p_bounds](int a, int b){
        return p_bounds[a] < p_bounds[b];
    });

    // Overwrite the group counts into group "ends" (the start indices of the next group)
    for (int j = 1; j < ng; ++j){
        p_bounds[j] += p_bounds[j - 1];
    }

    // When group IDs are sorted, each group is already a contiguous run of x
    // Otherwise gather x into group-contiguous order once, so that every group
    // is a contiguous run of `src`
    T scratch(0);

    if (!g.sorted){

        scratch = T(n);

        std::vector<int> pos;
        pos.reserve(ng);
        pos.push_back(0);
        for (int j = 1; j < ng; ++j){
            pos.push_back(p_bounds[j - 1]);
        }
        for (int i = 0; i < n; ++i){
            scratch.set(pos[unwrap(g.ids.get(i))]++, x.view(i));
        }
    }

    const T& src = g.sorted ? x : scratch;

    r_vec<ret_t> out(ng);

    T buf(0);
    int buf_len = 0;

    // At this point the data (`src`) is sorted by group ID
    // Such that the IDs are in ascending order
    // But we don't run through the data in that order - we jump around it instead
    // group_order is the permutation corresponding to ascending group sizes, and we traverse in that order,
    // picking the groups with the smallest group size first and ending with the groups with the largest size
    for (int k = 0; k < ng; ++k){

        int j = group_order[k];
        int start = j > 0 ? p_bounds[j - 1] : 0;
        int count = p_bounds[j] - start;

        // Only reuse buf if it's still exclusively ours
        // If fn() returned or captured buf itself, reusing it here would silently
        // overwrite an already-stored result instead of allocating a fresh buffer
        if (count != buf_len || !buf.is_exclusive()){
            buf = T(count);
            buf_len = count;
        }

        // Every group is a contiguous run of `src`
        r_copy_n(buf, src, 0, count, start);
        out.set(j, fn(buf));
    }
    return out;
}

}

#endif
