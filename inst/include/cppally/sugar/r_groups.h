#ifndef CPPALLY_R_GROUPS_H
#define CPPALLY_R_GROUPS_H

#include <cppally/r_vec.h>
#include <cppally/r_df.h>
#include <cppally/r_visit.h>
#include <cppally/sugar/r_stats.h>
#include <cppally/sugar/r_hash.h>
#include <cppally/sugar/r_value_map.h>
#include <cppally/sugar/r_sort.h>
#include <cppally/r_identical.h>
#include <ankerl/unordered_dense.h> // Hash maps for group IDs + unique + match
#include <functional>
#include <vector>

namespace cppally {

// 0-indexed group IDs: [0, n - 1]
struct groups {
  r_vec<r_int> ids;
  int n_groups;
  bool ordered;
  bool sorted;

  // Default constructor
  groups() = delete;
  // Manual constructor
  explicit groups(r_vec<r_int> group_ids, int num_groups, bool groups_ordered, bool groups_sorted) :
    ids(std::move(group_ids)),
    n_groups(num_groups),
    ordered(groups_ordered),
    sorted(groups_sorted)
    {}

  // group start locations
  r_vec<r_int> starts() const {

    int n = ids.length();

    r_vec<r_int> out(n_groups);

    if (n_groups == 0){
        return out;
    }

    if (sorted){
        // Initialise just in-case there are groups with no group IDs (e.g. unused factor levels)
        out.fill(na<r_int>());
        const int* RESTRICT p_ids = ids.data();
        int* RESTRICT p_out = out.data();

        if (n > 0){
            p_out[p_ids[0]] = 0;
        }

        for (int i = 1; i < n; ++i){
            // New group
            if (p_ids[i] != p_ids[i - 1]){
                p_out[p_ids[i]] = i;
            }
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

          for (int i = 0; i < n_groups; ++i){
            if (p_out[i] == unwrap(r_limits<r_int>::max())) [[unlikely]] {
                p_out[i] = unwrap(na<r_int>()); // This can happen with unused factor levels for example
            }
          }

        // This will set groups with no start locations to 0
        // (e.g. undropped factor levels)
        // If uncommenting the below line, make sure to remove RESTRICT keyword from pointers above
        // out.replace(0, n_groups, fill_value, 0);
    }

  return out;
}

r_vec<r_int> counts() const {

    r_size_t n = ids.length();

    // Counts initialised to zero
    r_vec<r_int> out(n_groups, r_int(0));
    const int* RESTRICT p_group_id = ids.data();
    int* RESTRICT p_out = out.data();

    // Count groups
    for (r_size_t i = 0; i < n; ++i) p_out[p_group_id[i]]++;

    return out;
}

// 0-indexed order vector
r_vec<r_int> order() const {
    if (sorted || is_sorted(ids)){
        int n = ids.length();
        r_vec<r_int> out(n);
        out.iota();
        return out;
    } else {
        return cppally::order(ids, /*preserve_ties = */ false);
    }
}

};

// Forward decl
inline groups make_groups(const r_sexp& x, bool ordered = false);

namespace internal {

template <RSortableType T>
inline groups make_groups_from_order(const r_vec<T>& x, const r_vec<r_int>& o) {
    r_size_t n = x.length();

    if (n == 0) return groups(r_vec<r_int>(), 0, true, true);
    
    r_vec<r_int> group_ids(n);
    auto* RESTRICT p_id = group_ids.data();
    auto* RESTRICT p_o = o.data();

    int current_group = 0;

    p_id[p_o[0]] = 0;

    for (r_size_t i = 1; i < n; ++i) {
        int idx_curr = p_o[i];
        int idx_prev = p_o[i - 1];

        bool is_equal;
        is_equal = identical(x.view(idx_curr), x.view(idx_prev));

        if (!is_equal) {
            current_group++;
        }

        p_id[idx_curr] = current_group;
    }

    int n_groups = current_group + 1;
    bool ordered = true;
    bool sorted = is_sorted(group_ids).is_true();
    return groups(group_ids, n_groups, ordered, sorted);
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

template <RVal T>
inline groups make_unordered_groups(const r_vec<T>& x) {

    using key_type = unwrap_t<T>;
    r_size_t n = x.length();

    if (n == 0) return groups(r_vec<r_int>(), 0, false, true);

    r_vec<r_int> group_ids(n);
    int n_groups;
    bool ordered = false;
    bool sorted = false;

    auto* RESTRICT p_x = x.data();
    auto* RESTRICT p_id = group_ids.data();

    // Try the dense int table first (For int with small range)
    // An all-NA vector falls through to the hash map, which handles NA keys
    if constexpr (is<T, r_int>) {

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

        if (done) {
            n_groups = next_id;
            // check if group IDs are sorted
            sorted = true;
            for (r_size_t i = 1; i < n; ++i) {
              if (p_id[i] < p_id[i - 1]){
                  sorted = false;
                  break;
              }
            }
            return groups(group_ids, n_groups, ordered, sorted);
        }
    }

        ankerl::unordered_dense::map<
        key_type,
        int,
        internal::r_hash<T>,
        internal::r_hash_eq<T>
      > lookup;
      lookup.reserve(internal::get_hash_map_reserve_size<T>(n));

      int next_id = 0;

      for (r_size_t i = 0; i < n; ++i) {
        key_type key = p_x[i];
        auto [it, inserted] = lookup.try_emplace(key, next_id);
        if (inserted) {
            p_id[i] = next_id++;
        } else {
            p_id[i] = it->second;
        }
      }

      // check if group IDs are sorted
      sorted = true;
      for (r_size_t i = 1; i < n; ++i) {
        if (p_id[i] < p_id[i - 1]){
            sorted = false;
            break;
        }
      }
      n_groups = next_id;
      return groups(group_ids, n_groups, ordered, sorted);
}

// template <RVal T>
// inline groups make_unordered_groups(const r_vec<T>& x) {

//     r_size_t n = x.length();

//     if (n == 0) return groups(r_vec<r_int>(), 0, false, true);

//     r_vec<r_int> group_ids(n);

//     auto* RESTRICT p_x = x.data();
//     auto* RESTRICT p_id = group_ids.data();

//     // Group IDs are assigned in order of first appearance
//     int n_groups = internal::with_value_map<int>(x, -1, [p_x, p_id, n](auto&& try_emplace, auto&&) -> int {
//         int next_id = 0;
//         for (r_size_t i = 0; i < n; ++i) {
//             auto [id, inserted] = try_emplace(p_x[i], next_id);
//             next_id += inserted;
//             p_id[i] = id;
//         }
//         return next_id;
//     });

//     // check if group IDs are sorted
//     bool sorted = true;
//     for (r_size_t i = 1; i < n; ++i) {
//         if (p_id[i] < p_id[i - 1]){
//             sorted = false;
//             break;
//         }
//     }
//     return groups(group_ids, n_groups, /*ordered=*/false, sorted);
// }

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
    lookup.reserve(get_hash_map_reserve_size<r_int>(static_cast<uint64_t>(nrow)));

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
    bool sorted = is_sorted(group_ids).is_true();
    return groups(group_ids, n_groups, false, sorted);
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
    bool sorted = is_sorted(group_ids).is_true();
    return groups(group_ids, n_groups, true, sorted);
}



template <RVal T>
inline groups make_ordered_groups(const r_vec<T>& x) {
    if constexpr (!RSortableType<T>){
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
    if (ordered && RSortableType<typename T::data_type>){
        return internal::make_ordered_groups(x);
    } else {
        return internal::make_unordered_groups(x);
    }
}

inline groups make_groups(const r_factors& x, bool ordered = false) {
    if (ordered){
        return internal::make_ordered_groups(x.value);
    } else {
        return internal::make_unordered_groups(x.value);
    }
}

inline groups make_groups(const r_df& x, bool ordered = false) {
    if (ordered){
        return internal::make_ordered_groups(x);
    } else {
        return internal::make_unordered_groups(x);
    }
}

inline groups make_groups(const r_sexp& x, bool ordered);

}

#endif
