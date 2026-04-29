#ifndef CPPALLY_R_GROUPS_H
#define CPPALLY_R_GROUPS_H

#include <cppally/r_vec.h>
#include <cppally/r_df.h>
#include <cppally/r_visit.h>
#include <cppally/sugar/r_stats.h>
#include <cppally/sugar/r_hash.h>
#include <cppally/sugar/r_sort.h>
#include <cppally/sugar/r_identical.h>
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

    int curr_group;

    if (sorted){
        // Initialise just in-case there are groups with no group IDs (e.g. unused factor levels)
        out.fill(na<r_int>());
        const int* RESTRICT p_ids = ids.data();
        int* RESTRICT p_out = out.data();

        curr_group = 0;
        p_out[0] = 0;

        for (int i = 1; i < n; ++i){
            // New group
            if (p_ids[i] > curr_group){
                p_out[++curr_group] = i;
            }
            //
            // if (p_ids[i] > p_ids[i - 1]){
            //     p_out[++curr_group] = i;
            // }
        }
    } else {

        // Initialise with largest int
        // so that for each group we take the min(out[i], i)
        // After passing through all data, this should reduce to the first location for each group
        out.fill(r_limits<r_int>::max());

        const int* RESTRICT p_ids = ids.data();
        int* RESTRICT p_out = out.data();

        for (int i = 0; i < n; ++i){
            curr_group = p_ids[i];
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
        for (int i = 0; i < n; ++i) out.set(i, r_int(i));
        return out;
    } else {
        return cppally::order(ids, /*preserve_ties = */ false);
    }
}

};

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

template <RVal T>
inline groups make_unordered_groups(const r_vec<T>& x) {

    using key_type = unwrap_t<T>;
    r_size_t n = x.length();

    if (n == 0) return groups(r_vec<r_int>(), 0, false, true);

    r_vec<r_int> group_ids(n);
    int n_groups;
    bool ordered = false;
    bool sorted;

    // Table Method (For int with small range)
    if constexpr (is<T, r_int>) {

        r_vec<T> rng = range(x, /*na_rm=*/true);

        int min_val = unwrap(rng.get(0));
        int max_val = unwrap(rng.get(1));

        // Check range results
        // If x had only NAs, result would also be c(NA, NA)

        bool all_nas = is_na(min_val) && is_na(max_val);
        int64_t range_span = 0;

        if (!all_nas) {
             range_span = static_cast<int64_t>(max_val) - static_cast<int64_t>(min_val);
        }

        // Table vs Hash Map
        // Table is faster if range is reasonably small (e.g. < 20M)
        constexpr int64_t MAX_TABLE_SIZE = 20000000;


        if (all_nas) {
          // If all NAs, just return all zeroes
          group_ids.fill(r_int(0));
          n_groups = 1;
          sorted = true;
          return groups(group_ids, n_groups, ordered, sorted);
       }

        if (!all_nas && range_span < MAX_TABLE_SIZE) {

            // --- FAST TABLE PATH ---

            // Table maps (value - min_val) -> group_id
            r_vec<r_int> table(range_span + 1, r_int(-1));
            int na_group_id = -1; // Special slot for NA

            auto* RESTRICT p_x = x.data();
            auto* RESTRICT p_id = group_ids.data();
            auto* RESTRICT p_table = table.data();

            int next_id = 0;

            for(r_size_t i = 0; i < n; ++i) {
                int val = p_x[i];

                int id;
                if (val == NA_INTEGER) {
                    if (na_group_id == -1) {
                        na_group_id = next_id++;
                    }
                    id = na_group_id;
                } else {
                    // Safe subtraction because we validated range
                    size_t idx = static_cast<size_t>(val - min_val);

                    id = p_table[idx];
                    if (id == -1) {
                        id = next_id++;
                        p_table[idx] = id;
                    }
                }
                p_id[i] = id;
            }
            n_groups = next_id;
            // check if group IDs are sorted
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

      auto* RESTRICT p_x = x.data();
      auto* RESTRICT p_id = group_ids.data();

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
      for (r_size_t i = 1; i < n; ++i) {
        if (p_id[i] < p_id[i - 1]){
            sorted = false;
            break;
        }
      }
      n_groups = next_id;
      return groups(group_ids, n_groups, ordered, sorted);
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

template <RVal T>
inline groups make_groups(const r_vec<T>& x, bool ordered = false) {
    if (ordered){
        return internal::make_ordered_groups(x);
    } else {
        return internal::make_unordered_groups(x);
    }
}

template <RFactor T>
inline groups make_groups(const T& x, bool ordered = false) {
    return make_groups(x.value, ordered);
}
template <RSexpType T>
inline groups make_groups(const T& x, bool ordered = false) {
    return CPPALLY_VIEW_AND_APPLY(x, /*return_type = */ groups, /*fn = */ make_groups, /*rest of args = */ ordered);
}

// namespace internal {

// // Build a per-column equality probe for every column of `x`.
// // Each probe takes two row indices and returns whether the column values
// // at those rows are identical(). Columns whose type doesn't support
// // `identical(col.view(i), col.view(j))` cause an abort.
// inline std::vector<std::function<bool(int, int)>> build_col_eq_probes(const r_df& x) {
//     int ncol = x.ncol();
//     std::vector<std::function<bool(int, int)>> eqs;
//     eqs.reserve(ncol);
//     for (int c = 0; c < ncol; ++c) {
//         view_sexp(x.value.view(c), [&]<typename ColT>(const ColT& col) {
//             if constexpr (requires (int i, int j) {
//                 identical(col.view(i), col.view(j));
//             }) {
//                 eqs.emplace_back([col](int i, int j) {
//                     return identical(col.view(i), col.view(j));
//                 });
//             } else {
//                 abort("make_groups(r_df): unsupported column type");
//             }
//         });
//     }
//     return eqs;
// }

// // Build vector of row hashes by combining hashes across cols
// inline std::vector<uint64_t> compute_row_hashes(const r_df& x) {
//     int nrow = x.nrow();
//     int ncol = x.ncol();
//     std::vector<uint64_t> row_hashes(size_t(nrow), 0U);
//     for (int c = 0; c < ncol; ++c) {
//         view_sexp(x.value.view(c), [&]<typename ColT>(const ColT& col) {
//             if constexpr (requires (int i) { r_hash_impl(col.view(i)); }) {
//                 for (int i = 0; i < nrow; ++i) {
//                     row_hashes[i] = hash_combine(row_hashes[i], r_hash_impl(col.view(i)));
//                 }
//             } else {
//                 abort("make_groups(r_df): unsupported column type");
//             }
//         });
//     }
//     return row_hashes;
// }

// // Lexicographic order across all columns of a data frame.
// // NAs sort first (consistent with cpp_stable_order).
// inline r_vec<r_int> multi_col_order(const r_df& x) {
//     int nrow = x.nrow();
//     int ncol = x.ncol();

//     r_vec<r_int> out(nrow);
//     int* RESTRICT p_out = out.data();
//     for (int i = 0; i < nrow; ++i) p_out[i] = i;

//     if (nrow <= 1 || ncol == 0){
//         return out;
//     }

//     // Build per-column 3-way comparators once
//     // returns -1 if a<b, 0 if equal, 1 if a>b
//     std::vector<std::function<int(int, int)>> cmps;
//     cmps.reserve(ncol);

//     for (int c = 0; c < ncol; ++c) {
//         view_sexp(x.value.view(c), [&]<typename ColT>(const ColT& col) {
//             if constexpr (requires (int i, int j) {
//                 identical(col.view(i), col.view(j));
//                 is_na(col.view(i));
//                 col.view(i) < col.view(j);
//             }) {
//                 cmps.emplace_back([col](int i, int j) -> int {
//                     if (identical(col.view(i), col.view(j))) return 0;
//                     bool i_na = is_na(col.view(i));
//                     bool j_na = is_na(col.view(j));
//                     if (i_na || j_na) {
//                         // NA sorts first
//                         return i_na ? -1 : 1;
//                     }
//                     auto lt = col.view(i) < col.view(j);
//                     return static_cast<bool>(unwrap(lt)) ? -1 : 1;
//                 });
//             } else {
//                 abort("make_groups(r_df): ordered grouping requires sortable columns");
//             }
//         });
//     }

//     std::stable_sort(p_out, p_out + nrow, [&](int a, int b) {
//         for (auto& cmp : cmps) {
//             int r = cmp(a, b);
//             if (r != 0) return r < 0;
//         }
//         return false;
//     });
//     return out;
// }

// inline groups make_ordered_df_groups(const r_df& x) {
//     int nrow = x.nrow();
//     int ncol = x.ncol();

//     if (nrow == 0) {
//         return groups(r_vec<r_int>(), 0, true, true); 
//     }
//     if (ncol == 0) {
//         return groups(r_vec<r_int>(nrow, r_int(0)), 1, true, true);
//     }
//     if (ncol == 1){
//         return make_groups(x.value.view(0), true);
//     }

//     r_vec<r_int> group_ids(nrow);
//     r_vec<r_int> o = multi_col_order(x);

//     std::vector<std::function<bool(int, int)>> eqs = build_col_eq_probes(x);

//     const int* RESTRICT p_o = o.data();
//     int* RESTRICT p_id = group_ids.data();

//     int current = 0;
//     p_id[p_o[0]] = 0;

//     for (int i = 1; i < nrow; ++i) {
//         int cur = p_o[i];
//         int prev = p_o[i - 1];

//         bool all_eq = true;
//         for (auto& eq : eqs) {
//             if (!eq(cur, prev)) {
//                 all_eq = false; 
//                 break; 
//             }
//         }
//         current += !all_eq;
//         p_id[cur] = current;
//     }

//     int n_groups = current + 1;
//     bool sorted = is_sorted(group_ids).is_true();
//     return groups(group_ids, n_groups, true, sorted);
// }

// // hash each row directly into a single key
// inline groups make_unordered_df_groups(const r_df& x) {
//     int nrow = x.nrow();
//     int ncol = x.ncol();

//     if (nrow == 0) {
//         return groups(r_vec<r_int>(), 0, false, true); 
//     }
//     if (ncol == 0) {
//         return groups(r_vec<r_int>(nrow, r_int(0)), 1, false, true);
//     }
//     // If 1-col, use regular make_groups()
//     if (ncol == 1){
//         return make_groups(x.value.view(0), false);
//     }

//     r_vec<r_int> group_ids(nrow);

//     std::vector<uint64_t> row_hashes = compute_row_hashes(x);
//     std::vector<std::function<bool(int, int)>> eqs = build_col_eq_probes(x);

//     auto rows_equal = [&](int i, int j) {
//         for (auto& fn : eqs) {
//             if (!fn(i, j)) return false;
//         }
//         return true;
//     };

//     // Map: row-hash -> chain of (representative_row, group_id)
//     ankerl::unordered_dense::map<uint64_t, std::vector<std::pair<int, int>>> lookup;
//     lookup.reserve(get_hash_map_reserve_size<r_int>(static_cast<uint64_t>(nrow)));

//     int* RESTRICT p_id = group_ids.data();
//     int next_id = 0;

//     for (int i = 0; i < nrow; ++i) {
//         auto& chain = lookup[row_hashes[i]];
//         int found = -1;
//         for (auto& [rep, gid] : chain) {
//             if (rows_equal(i, rep)) {
//                 found = gid; 
//                 break; 
//             }
//         }
//         if (found < 0) {
//             chain.emplace_back(i, next_id);
//             p_id[i] = next_id++;
//         } else {
//             p_id[i] = found;
//         }
//     }
//     int n_groups = next_id;
//     bool sorted = is_sorted(group_ids).is_true();
//     return groups(group_ids, n_groups, false, sorted);
// }

// }

// template <RDataFrame T>
// inline groups make_groups(const T& x, bool ordered = false) {
//     if (ordered) {
//         return internal::make_ordered_df_groups(x);
//     } else {
//         return internal::make_unordered_df_groups(x);
//     }
// }


}

#endif
