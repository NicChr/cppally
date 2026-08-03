#ifndef CPPALLY_R_APPLY_BY_GROUP_H
#define CPPALLY_R_APPLY_BY_GROUP_H

#include <cppally/r_vec.h>
#include <cppally/sugar/r_groups.h>
#include <vector>
#include <algorithm>

namespace cppally {

namespace internal {

// offsets[j] = start of group j in group-contiguous order, offsets[n_groups] = n
inline std::vector<int> group_offsets(const groups& g){

    int n = static_cast<int>(g.ids.length());
    const int* RESTRICT p_id = g.ids.data();

    std::vector<int> offsets(g.n_groups + 1, 0);

    for (int i = 0; i < n; ++i){
        ++offsets[p_id[i] + 1];
    }
    for (int j = 0; j < g.n_groups; ++j){
        offsets[j + 1] += offsets[j];
    }
    return offsets;
}

}

// Efficiently apply a function by-group.
template <RVector T, typename F>
requires (std::invocable<F&, const r_vec<typename T::data_type>&>)
auto apply_by_group(const T& x, F fn, const groups& g) {

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
    std::vector<int> offsets = internal::group_offsets(g);

    // When group IDs are sorted, each group is already a contiguous run of x
    // Otherwise gather x into group-contiguous order once, so that every group
    // is a contiguous run of `src`
    T scratch(0);

    if (!g.sorted){
        scratch = T(n);
        std::vector<int> pos(offsets.begin(), offsets.end() - 1);
        for (int i = 0; i < n; ++i){
            scratch.set(pos[unwrap(g.ids.get(i))]++, x.view(i));
        }
    }

    const T& src = g.sorted ? x : scratch;

    // Group locations sorted by group size
    std::vector<int> group_order(g.n_groups);
    for (int j = 0; j < g.n_groups; ++j){
        group_order[j] = j;
    }
    std::sort(group_order.begin(), group_order.end(), [&offsets](int a, int b){
        return (offsets[a + 1] - offsets[a]) < (offsets[b + 1] - offsets[b]);
    });

    r_vec<ret_t> out(g.n_groups);

    T buf(0);
    int buf_len = 0;

    for (int k = 0; k < g.n_groups; ++k){

        int j = group_order[k];
        int start = offsets[j];
        int count = offsets[j + 1] - start;

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
