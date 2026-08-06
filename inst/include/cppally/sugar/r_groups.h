#ifndef CPPALLY_R_GROUPS_H
#define CPPALLY_R_GROUPS_H

#include <cppally/r_vec.h>
#include <cppally/r_df.h>
#include <cppally/r_visit.h>
#include <cppally/sugar/r_stats.h>
#include <cppally/sugar/r_hash.h>
#include <cppally/sugar/r_dense_int_map.h>
#include <cppally/sugar/r_sort.h>
#include <cppally/random/random_stream.h>
#include <cppally/r_identical.h>
#include <ankerl/unordered_dense.h> // Hash maps for group IDs + unique + match
#include <functional>
#include <vector>

namespace cppally {

namespace internal {

inline bool ids_are_sorted(const int* RESTRICT p, r_size_t n) noexcept {
    for (r_size_t i = 1; i < n; ++i) {
        if (p[i] < p[i - 1]){
            return false;
        }
    }
    return true;
}

// Rows `run_end` scans linearly before galloping
inline constexpr int min_gallop = 32;

// Minimum average run length (rows per group) for `starts()` to jump from run
// to run; below it the branchless scan wins. Break-even measured in (10, 100)
inline constexpr int min_gallop_run = 64;

// End of the run starting at `i`: the smallest j > i with p[j] != p[i], or `n`
// when the run reaches the end. Requires non-decreasing `p`.
// Cost scales with the run being skipped, not with `n`

// 1. Scan linearly from `i` over the first `min_gallop` data points
// 2. If the run is longer than that, gallop by doubling the probe distance from `i` until we overshoot the run end
// 3. Binary search the bracket left behind: from just past the last probe still inside the run, up to the probe that overshot
inline int run_end(const int* RESTRICT p, int i, int n) noexcept {

    int curr = p[i];

    // Short runs are the common case and stream well: scan them directly.
    // Written via `n - i` to avoid overflowing `i + min_gallop`
    int linear_end = n - i > min_gallop ? i + min_gallop : n;

    // Linear-search in-case short-runs are common because in that scenario this is faster
    // By doing this we also warm up the min gallop size
    for (int j = i + 1; j < linear_end; ++j){
        if (p[j] != curr){
            return j;
        }
    }

    if (linear_end == n){
        return n;
    }

    // Long run: gallop to bracket its end. Sampling suffices because `p` is
    // non-decreasing, so p[hi] == curr pins all of [i, hi] to `curr`
    int lo = linear_end;      // run end is at or past `lo`
    int hi = lo;           // next probe
    int step = min_gallop; // always `hi - i`

    while (hi < n && p[hi] == curr){
        lo = hi + 1;
        // Double the probe distance, clamped so `hi` cannot pass `n`
        step = step < ((n - i) >> 1) ? step << 1 : n - i;
        hi = i + step;
    }

    // Binary search: the run end is now in [lo, hi]
    while (lo < hi){

        int mid = lo + ((hi - lo) >> 1);

        if (p[mid] == curr){
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    return lo;
}

}

// A class for storing group information.
// `ids` - Integer group IDs. Each unique ID corresponds to a unique data element.
// `n_groups` - Number of unique groups.
// `ordered` - Do the group IDs convey a sorting order? If true then this implies that unique group IDs 0...n_groups-1 correspond to unique + sorted data.
// If `ordered` is false then it implies `order-of-first-appearance` and group IDs 0...n_groups-1 correspond to unique data collected based on first appearance.
// `sorted` - Are the group IDs already sorted? This is calculated automatically when constructing groups(ids, n_groups, ordered) or can be explicitly 
// passed as the 4th argument if you already know that your group IDs are sorted.
// `sorted` is not to be confused with `ordered`, as it is entirely possible to have `ordered=false` and `sorted=true`, which would imply
// sorted order-of-first appearance group IDs.
struct groups {
  r_vec<r_int> ids;
  int n_groups;
  bool ordered;
  bool sorted;

  // Default constructor
  groups() = delete;

  explicit groups(r_vec<r_int> group_ids, int num_groups, bool groups_ordered, bool groups_sorted) :
    ids(std::move(group_ids)),
    n_groups(num_groups),
    ordered(groups_ordered),
    sorted(groups_sorted)
    {}

    explicit groups(r_vec<r_int> group_ids, int num_groups, bool groups_ordered) :
    ids(std::move(group_ids)),
    n_groups(num_groups),
    ordered(groups_ordered),
    sorted(internal::ids_are_sorted(ids.data(), ids.length()))
    {}

  // group start locations
  r_vec<r_int> starts() const {

    int n = ids.length();

    r_vec<r_int> out(n_groups);

    if (n_groups == 0){
        return out;
    }

    // Sorted ids make each group one contiguous run whose first row is the
    // group start. Only jump from run to run when the average run is long
    // enough to be worth skipping; below that the branchless scan wins
    if (sorted && n / n_groups >= internal::min_gallop_run){

        out.fill(na<r_int>());

        const int* RESTRICT p_ids = ids.data();
        int* RESTRICT p_out = out.data();

        int i = 0;

        while (i < n){
            p_out[p_ids[i]] = i;
            i = internal::run_end(p_ids, i, n);
        }
    } else {

        // Initialise with largest int
        // so that for each group we take the min(out[i], i)
        // After passing through all data, this should reduce to the first location for each group
        out.fill(r_limits<r_int>::max());

        const int* RESTRICT p_ids = ids.data();
        int* RESTRICT p_out = out.data();

        for (int i = 0; i < n; ++i){
            int curr_group = p_ids[i];
            p_out[curr_group] = std::min(p_out[curr_group], i);
          }

        //   for (int i = 0; i < n_groups; ++i){
        //     if (p_out[i] == unwrap(r_limits<r_int>::max())) [[unlikely]] {
        //         p_out[i] = unwrap(na<r_int>()); // This can happen with unused factor levels for example
        //     }
        //   }

        // This will set groups with no start locations to 0
        // (e.g. undropped factor levels)
        // If uncommenting the below line, make sure to remove RESTRICT keyword from pointers above
        // out.replace(0, n_groups, fill_value, 0);
    }

  return out;
}

r_vec<r_int> counts() const {

    int n = ids.length();

    // Initialise counts to zero
    r_vec<r_int> out(n_groups, r_int(0));

    if (n_groups == 0){
        return out;
    }

    const int* RESTRICT p_ids = ids.data();
    int* RESTRICT p_out = out.data();

    // Sorted ids make each group one contiguous run, so its count is the run length.
    if (sorted && n / n_groups >= internal::min_gallop_run){

        int i = 0;

        while (i < n){
            int end = internal::run_end(p_ids, i, n);
            p_out[p_ids[i]] = end - i;
            i = end;
        }

    } else {
        for (int i = 0; i < n; ++i){
            p_out[p_ids[i]]++;
        }
    }

    return out;
}

// 0-indexed order vector
r_vec<r_int> order() const {
    
    if (sorted){
        
        int n = ids.length();
        r_vec<r_int> out(n);
        out.iota();
        return out;

    } else {
        
        // Count sort

        // No need to use order() because we can write a faster custom method due to 
        // the fact that group IDs have no NAs, and we know the range upfront (range = n_groups)

        std::vector<uint32_t> counts(n_groups, uint32_t(0));
        uint32_t n = ids.length();

        auto* RESTRICT p_x = ids.data();

        // Count occurrences
        for (uint32_t i = 0; i < n; ++i) counts[p_x[i]]++;

        // Prefix sum: counts[i] becomes the starting position for value i
        uint32_t total = 0;
        for (int i = 0; i < n_groups; ++i) {
            uint32_t old_count = counts[i];
            counts[i] = total;
            total += old_count;
        }

        // Write indices in sorted order
        
        r_vec<r_int> out(static_cast<r_size_t>(n));
        int* RESTRICT p_out = out.data();

        for (uint32_t i = 0; i < n; ++i) {
            p_out[counts[p_x[i]]++] = static_cast<int>(i);
        }
        return out;
    }
}

};

// Forward decl
inline groups make_groups(const r_sexp& x, bool ordered = false);

namespace internal {

template <RSortableType T>
inline groups make_groups_from_order(const r_vec<T>& x, const r_vec<r_int>& o) {
    r_size_t n = x.length();

    if (n != o.length()) [[unlikely]] {
        abort("`x.length()` must match `o.length()`");
    }

    if (n == 0) return groups(r_vec<r_int>(), 0, true, true);
    
    r_vec<r_int> group_ids(n);

    int current_group = 0;

    group_ids.set(unwrap(o.get(0)), r_int(0));

    for (r_size_t i = 1; i < n; ++i) {
        int idx_curr = unwrap(o.get(i));
        int idx_prev = unwrap(o.get(i - 1));

        if (!identical(x.view(idx_curr), x.view(idx_prev))) {
            current_group++;
        }
        group_ids.set(idx_curr, r_int(current_group));
    }

    int n_groups = current_group + 1;
    return groups(group_ids, n_groups, /*ordered=*/ true);
}

// Build a per-column equality probe for every column of `x`.
// Each probe takes two row indices and returns whether the column values
// at those rows are identical(). Columns whose type doesn't support
// `identical(col.view(i), col.view(j))` cause an abort.
inline std::vector<std::function<bool(int, int)>> build_col_eq_probes(const r_df& x) {
    int ncol = x.ncol();
    std::vector<std::function<bool(int, int)>> eqs;
    eqs.reserve(ncol);
    for (int c = 0; c < ncol; ++c) {
        internal::view_sexp(x.value.view(c), [&]<typename ColT>(const ColT& col) {
            if constexpr (requires (int i, int j) {
                identical(col.view(i), col.view(j));
            }) {
                eqs.emplace_back([col](int i, int j) {
                    return identical(col.view(i), col.view(j));
                });
            } else {
                abort("make_groups(r_df): unsupported column type");
            }
        });
    }
    return eqs;
}

template <RVector T>
inline groups make_unordered_groups(const T& x) {

    using data_t = typename T::data_type;
    using key_type = unwrap_t<data_t>;
    r_size_t n = x.length();

    if (n == 0) return groups(r_vec<r_int>(), 0, false, true);

    r_vec<r_int> group_ids(n);
    int n_groups;

    auto* RESTRICT p_x = x.data();
    auto* RESTRICT p_id = group_ids.data();

    // Try the dense int table first (For int storage with small range)
    // An all-NA vector falls through to the hash map, which handles NA keys

    int next_id = 0;

    bool done = internal::try_dense_int_map(x, -1, [&, p_x, p_id](auto&& try_emplace, auto&&) {
        for (r_size_t i = 0; i < n; ++i) {
            auto [id, inserted] = try_emplace(p_x[i], next_id);
            // Branch instead of `next_id += inserted` so the common (found) path
            // carries no dependency between iterations
            if (inserted) {
                ++next_id;
            }
            p_id[i] = id;
        }
    });

    if (!done) {

        uint64_t hash_map_reserve_guess = internal::get_hash_map_reserve_size<T>(p_x, n);
        
        ankerl::unordered_dense::map<
        key_type,
        int,
        internal::r_hash<data_t>,
        internal::r_hash_eq<data_t>
      > lookup;

      lookup.reserve(hash_map_reserve_guess);

      for (r_size_t i = 0; i < n; ++i) {
        key_type key = p_x[i];
        auto [it, inserted] = lookup.try_emplace(key, next_id);
        if (inserted) {
            p_id[i] = next_id++;
        } else {
            p_id[i] = it->second;
        }
      }

    }
      n_groups = next_id;
      return groups(group_ids, n_groups, false, ids_are_sorted(p_id, n));
}

// hash each row directly into a single key
inline groups make_unordered_groups(const r_df& x) {
    int nrow = x.nrow();
    int ncol = x.ncol();

    if (nrow == 0) {
        return groups(r_vec<r_int>(), 0, false, true); 
    }
    if (ncol == 0) {
        return groups(r_vec<r_int>(nrow, r_int(0)), 1, false, true);
    }
    // If 1-col, use regular make_groups()
    if (ncol == 1){
        return make_groups(x.value.view(0), false);
    }

    r_vec<r_int> group_ids(nrow);

    std::vector<uint64_t> row_ids = row_hashes(x);
    std::vector<std::function<bool(int, int)>> eqs = build_col_eq_probes(x);

    auto rows_equal = [&](int i, int j) {
        for (auto& fn : eqs) {
            if (!fn(i, j)) return false;
        }
        return true;
    };

    // Map: row-hash -> chain of (representative_row, group_id)
    ankerl::unordered_dense::map<uint64_t, std::vector<std::pair<int, int>>> lookup;
    lookup.reserve(static_cast<uint64_t>(nrow / 4));

    int* RESTRICT p_id = group_ids.data();
    int next_id = 0;

    for (int i = 0; i < nrow; ++i) {
        auto& chain = lookup[row_ids[i]];
        int found = -1;
        for (auto& [rep, gid] : chain) {
            if (rows_equal(i, rep)) {
                found = gid; 
                break; 
            }
        }
        if (found < 0) {
            chain.emplace_back(i, next_id);
            p_id[i] = next_id++;
        } else {
            p_id[i] = found;
        }
    }
    int n_groups = next_id;
    return groups(group_ids, n_groups, false, ids_are_sorted(p_id, nrow));
}

inline groups make_ordered_groups(const r_df& x) {
    int nrow = x.nrow();
    int ncol = x.ncol();

    if (nrow == 0) {
        return groups(r_vec<r_int>(), 0, true, true); 
    }
    if (ncol == 0) {
        return groups(r_vec<r_int>(nrow, r_int(0)), 1, true, true);
    }
    if (ncol == 1){
        return make_groups(x.value.view(0), true);
    }

    r_vec<r_int> group_ids(nrow);
    r_vec<r_int> o = order(x);

    std::vector<std::function<bool(int, int)>> eqs = build_col_eq_probes(x);

    const int* RESTRICT p_o = o.data();
    int* RESTRICT p_id = group_ids.data();

    int current = 0;
    p_id[p_o[0]] = 0;

    for (int i = 1; i < nrow; ++i) {
        int cur = p_o[i];
        int prev = p_o[i - 1];

        bool all_eq = true;
        for (auto& eq : eqs) {
            if (!eq(cur, prev)) {
                all_eq = false; 
                break; 
            }
        }
        current += !all_eq;
        p_id[cur] = current;
    }

    int n_groups = current + 1;
    return groups(group_ids, n_groups, true, ids_are_sorted(p_id, nrow));
}



template <RVector T>
inline groups make_ordered_groups(const T& x) {
    if constexpr (!RSortableType<typename T::data_type>){
        return make_unordered_groups(x);
    } else {
        return make_groups_from_order(x, order(x, /*preserve_ties = */ false));
    }
}

}

template <RVector T>
inline groups make_groups(const T& x, bool ordered = false) {
    if (x.is_long()){
        abort("Cannot group a long-vector");
    }
    if (ordered){
        return internal::make_ordered_groups(x);
    } else {
        return internal::make_unordered_groups(x);
    }
}

inline groups make_groups(const r_factors& x, bool ordered = false) {
    return make_groups(x.value, ordered);
}

inline groups make_groups(const r_df& x, bool ordered = false) {
    if (ordered){
        return internal::make_ordered_groups(x);
    } else {
        return internal::make_unordered_groups(x);
    }
}

inline groups make_groups(const r_sexp& x, bool ordered);

template <RComposite T>
r_vec<r_str> group_names(const T& x, const groups& g) {

    int ng = g.n_groups;
    int n = g.ids.length();

    r_vec<r_str> out(ng);

    const int* RESTRICT p_ids = g.ids.data();

    if (g.sorted){

        // Each group is one contiguous run: name it from its first row
        int i = 0;

        while (i < n){
            out.set(p_ids[i], as<r_str>(x.view(i)));
            i = internal::run_end(p_ids, i, n);
        }

    } else {

        std::vector<uint8_t> seen(ng, uint8_t(0));
        int n_seen = 0;

        // Name each group at its first occurrence, stopping once all are seen
        for (int i = 0; i < n && n_seen < ng; ++i) {

            int curr_group = p_ids[i];

            if (!seen[curr_group]) {
                out.set(curr_group, as<r_str>(x.view(i)));
                seen[curr_group] = uint8_t(1);
                ++n_seen;
            }
        }
    }

    return out;
}

// inline std::vector<r_vec<r_int>> group_indices(const groups& g){
  
//     int n_groups = g.n_groups;
//     r_vec<r_int> sizes = g.counts();
//     r_vec<r_int> ord = g.order();

//     std::vector<r_vec<r_int>> group_locs;
//     group_locs.reserve(n_groups);

//     int k = 0;
//     int group_size = 0;
    
//     for (int i = 0; i < n_groups; ++i, k += group_size){
//       group_size = sizes.get(i);
//       r_vec<r_int> locs = r_vec<r_int>(group_size);
//       r_copy_n(locs, ord, 0, group_size, k);
//       group_locs.push_back(std::move(locs));
//     }
//     return group_locs;
// }

// namespace internal {

// inline std::vector<std::vector<int>> group_indices(const groups& g){
  
//     int n_groups = g.n_groups;

//     r_vec<r_int> sizes = g.counts();

//     std::vector<std::vector<int>> group_locs;
//     group_locs.reserve(n_groups);

//     // Initialise locations
//     for (int i = 0; i != n_groups; ++i){
//         group_locs.emplace_back().reserve(unwrap(sizes.get(i)));
//     }

//     int n = g.ids.length();
//     int curr_group;

//     for (int i = 0; i < n; ++i){
//         curr_group = g.ids.get(i);
//         group_locs[curr_group].push_back(i);
//     }

//     return group_locs;
// }

// }

// template <RVector T>
// inline std::vector<T> split(const T& x, const groups& g){
  
//     std::vector<std::vector<int>> indices = internal::group_indices(g);

//     int n_groups = g.n_groups;
//     std::vector<T> out;
//     out.reserve(n_groups);

//     for (int i = 0; i < n_groups; ++i){
//         const std::vector<int>& locs = indices[i];
//         int m = static_cast<int>(locs.size());

//         T group(m);
//         for (int j = 0; j < m; ++j){
//             group.set(j, x.get(locs[j]));
//         }
//         out.push_back(std::move(group));
//     }
//     return out;
// }


}

#endif
