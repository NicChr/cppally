#ifndef CPPALLY_R_APPLY_BY_GROUP_H
#define CPPALLY_R_APPLY_BY_GROUP_H

#include <cppally/r_vec.h>
#include <cppally/sugar/r_groups.h>
#include <vector>
#include <algorithm>

namespace cppally {

// Efficiently apply a function by-group.
template <RVector T, typename F>
requires (std::invocable<F&, const r_vec<typename T::data_type>&>)
auto apply_by_group(const T& x, const groups& g, F fn) {

    if (x.length() != g.ids.length()) [[unlikely]] {
        abort("`apply_by_group`: `x` and group IDs must have equal lengths");
    }

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

        auto pos = std::make_unique_for_overwrite<int[]>(ng);
        pos[0] = 0;
        for (int j = 1; j < ng; ++j){
            pos[j] = p_bounds[j - 1];
        }
        for (int i = 0; i < n; ++i){
            scratch.set(pos[unwrap(g.ids.get(i))]++, x.view(i));
        }
    }

    const T& src = g.sorted ? x : scratch;

    r_vec<ret_t> out(ng);

    T buf(0);
    int buf_len = 0;

    for (int k = 0; k < ng; ++k){

        int j = group_order[k];
        int start = j > 0 ? p_bounds[j - 1] : 0;
        int count = p_bounds[j] - start;

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
