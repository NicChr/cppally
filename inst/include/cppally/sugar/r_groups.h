#ifndef CPPALLY_R_GROUPS_H
#define CPPALLY_R_GROUPS_H

#include <cppally/r_vec.h>
#include <cppally/sugar/r_stats.h>
#include <cppally/sugar/r_hash.h>
#include <cppally/sugar/r_sort.h>
#include <ankerl/unordered_dense.h> // Hash maps for group IDs + unique + match

namespace cppally {

// 0-indexed group IDs: [0, n - 1]
struct groups {
  r_vec<r_int> ids;
  int n_groups = 0;
  bool ordered = true;
  bool sorted = true;

  // Default constructor
  groups() = default;
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
        // out.fill(0, n_groups, 0);
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
    groups g;
    g.ids = r_vec<r_int>(n);

    if (n == 0) return g;

    auto* RESTRICT p_id = g.ids.data();
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

    g.n_groups = current_group + 1;
    g.ordered = true;
    g.sorted = is_sorted(g.ids).is_true();

    return g;
}

template <RVal T>
inline groups make_unordered_groups(const r_vec<T>& x) {

    using key_type = unwrap_t<T>;
    r_size_t n = x.length();
    groups g;
    g.ids = r_vec<r_int>(n);
    g.n_groups = 0;
    g.ordered = false;
    g.sorted = true;

    if (n == 0) return groups();

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
          g.ids.fill(r_int(0));
          g.n_groups = 1;
          g.sorted = true;
          return g;
       }

        if (!all_nas && range_span < MAX_TABLE_SIZE) {

            // --- FAST TABLE PATH ---

            // Table maps (value - min_val) -> group_id
            r_vec<r_int> table(range_span + 1, r_int(-1));
            int na_group_id = -1; // Special slot for NA

            auto* RESTRICT p_x = x.data();
            auto* RESTRICT p_id = g.ids.data();
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
            g.n_groups = next_id;
            // check if group IDs are sorted
            for (r_size_t i = 1; i < n; ++i) {
              if (p_id[i] < p_id[i - 1]){
                  g.sorted = false;
                  break;
              }
            }
            return g;
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
      auto* RESTRICT p_id = g.ids.data();

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
            g.sorted = false;
            break;
        }
      }
      g.n_groups = next_id;
      return g;
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


}

#endif
