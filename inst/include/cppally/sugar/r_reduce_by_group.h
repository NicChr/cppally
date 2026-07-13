#ifndef CPPALLY_R_REDUCE_BY_GROUP_H
#define CPPALLY_R_REDUCE_BY_GROUP_H

#include <cppally/r_vec.h>
#include <cppally/r_coerce.h>
#include <cppally/sugar/r_groups.h>
#include <vector>

// Methods for reduction by group
// Sum by group example: `reduce_by_group(x, std::plus<>{}, groups)` (cppally ensures arithmetic is overflow safe)

namespace cppally {

namespace internal {

template <RComposite T>
void check_groups_span_data(const T& x, const groups& g){
    if (x.length() != g.ids.length()) [[unlikely]] {
        abort("check_groups_span_data: data and group IDs must have equal length");
    }
}

}

// Reduce each group to a single value: out[g] = left-to-right fold of fn over x's elements in group g
// Groups with no elements (e.g. unused factor levels) are NA
template <RVal T, typename F>
requires std::invocable<F&, T, T>
auto reduce_by_group(const r_vec<T>& x, F fn, const groups& g, bool na_skip = false) {

    using acc_t = std::remove_cvref_t<std::invoke_result_t<F&, T, T>>;
    static_assert(!internal::fold_result<acc_t>::stops, "`reduce_by_group` combiner must not short-circuit (no `done()`)");

    internal::check_groups_span_data(x, g);

    r_size_t n = x.length();

    std::vector<acc_t> accs(g.n_groups, na<acc_t>());
    std::vector<bool> seen(g.n_groups, false);

    const int* RESTRICT p_id = g.ids.data();

    for (r_size_t i = 0; i < n; ++i){
        if (na_skip && is_na(x.view(i))){
            continue;
        }
        int gid = p_id[i];
        if (seen[gid]){
            accs[gid] = fn(std::move(accs[gid]), x.view(i));
        } else {
            accs[gid] = as<acc_t>(x.view(i));
            seen[gid] = true;
        }
    }

    return as<r_vec<acc_t>>(accs);
}

template <RVal T, typename F, RComposite G>
requires std::invocable<F&, T, T>
auto reduce_by_group(const r_vec<T>& x, F fn, const G& g, bool na_skip = false) {
    return reduce_by_group(x, std::move(fn), make_groups(g), na_skip);
}

// Groups with no elements reduce to `init`
template <RVal T, typename Acc, typename F>
requires std::invocable<F&, Acc, T>
auto reduce_by_group(const r_vec<T>& x, F fn, const groups& g, Acc init, bool na_skip = false) {

    using acc_t = std::remove_cvref_t<std::invoke_result_t<F&, Acc, T>>;
    static_assert(!internal::fold_result<acc_t>::stops, "`reduce_by_group` combiner must not short-circuit (no `done()`)");

    r_size_t n = x.length();

    internal::check_groups_span_data(x, g);

    std::vector<acc_t> accs(g.n_groups, as<acc_t>(init));

    const int* RESTRICT p_id = g.ids.data();

    for (r_size_t i = 0; i < n; ++i){
        if (na_skip && is_na(x.view(i))){
            continue;
        }
        int gid = p_id[i];
        accs[gid] = fn(std::move(accs[gid]), x.view(i));
    }

    return as<r_vec<acc_t>>(accs);
}

template <RVal T, typename Acc, typename F, RComposite G>
requires std::invocable<F&, Acc, T>
auto reduce_by_group(const r_vec<T>& x, F fn, const G& g, Acc init, bool na_skip = false) {
    return reduce_by_group(x, std::move(fn), make_groups(g), std::move(init), na_skip);
}

}

#endif
