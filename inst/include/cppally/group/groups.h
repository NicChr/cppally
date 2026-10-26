#ifndef CPPALLY_R_GROUPS_H
#define CPPALLY_R_GROUPS_H

#include <cppally/vector/r_vector.h>
#include <cppally/coerce.h>
#include <cppally/hash/hash.h>
#include <cppally/group/dense_int_map.h>
#include <cppally/sort/counting_order.h>
#include <cppally/identical.h>
#include <ankerl/unordered_dense.h> // Hash maps for group IDs + unique + match
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

// End of the run starting at `start`: the smallest j > start with p[j] != p[start], or `n`
// when the run reaches the end. Requires non-decreasing `p`.
// Cost scales with the run being skipped, not with `n`

// 1. Scan linearly from `start` over the first `min_gallop` data points
// 2. If the run is longer than that, gallop by doubling the probe distance from `start` until we overshoot the run end
// 3. Binary search the bracket left behind: from just past the last probe still inside the run, up to the probe that overshot
inline int run_end(const int* RESTRICT p, int start, int n) noexcept {

    int curr = p[start];
    int span = n - start;

    // Short runs are the common case and stream well: scan them directly.
    // Written via `span` to avoid overflowing `start + min_gallop`
    int linear_end = span > min_gallop ? start + min_gallop : n;

    // ----- Linear search first few elements -----
    // Also warms up starting gallop size
    for (int j = start + 1; j < linear_end; ++j){
        // If value != curr, then we have reached the end of the current run
        if (p[j] != curr){
            return j;
        }
    }

    // Linear scan didn't find run end and we're at the end of the data, run end is `n`
    if (linear_end == n){
        return n;
    }

    // ----- Doubling steps -----
    // Safe to skip indices since `p` is in ascending order.
    // p[hi] == curr implies all [start, hi] is `curr`
    int lo = linear_end;   // run end is at or past `lo`
    int hi = lo;           // next probe
    int step = min_gallop; // always `hi - start`

    while (hi < n && p[hi] == curr){
        lo = hi + 1;
        // Double the probe distance, clamped so `hi` cannot pass `n`
        step = step < (span >> 1) ? step << 1 : span;
        hi = start + step;
    }

    // ----- Binary search -----
    // run end is in [lo, hi]. 
    // Halve range until lo == hi == run end
    while (lo < hi){

        int mid = lo + ((hi - lo) >> 1);

        if (p[mid] == curr){
            // [lo, hi] -> [mid + 1, hi] --> mid = (mid + 1 + hi) / 2
            lo = mid + 1;
        } else {
            // [lo, hi] -> [lo, mid] --> mid = (lo + mid) / 2
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
  
  const r_vec<r_int> ids;
  const int n_groups;
  const bool ordered;
  const bool sorted;

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

      int avg_group_size = n / n_groups;

      const int* RESTRICT p_ids = ids.data();
      int* RESTRICT p_out = out.data();
  
      // If group IDs are sorted, we can use a gallop search.
      // This works by skipping steps, with each step doubling in size.
      // Once we overshoot, use a binary search to find the start index.
      if (sorted && avg_group_size >= internal::min_gallop_run){
          int i = 0;
          while (i < n){
              p_out[p_ids[i]] = i;
              i = internal::run_end(p_ids, i, n);
          }
      } else {

        if (ordered){
            // Last index (in reverse order) for each group is the start index
            for (int i = n - 1; i >= 0; --i){
                p_out[p_ids[i]] = i;
            }
        } else {
            int target_id = 0;
            for (int i = 0; i < n && target_id < n_groups; ++i){
                p_out[target_id] = i;
                target_id += (p_ids[i] == target_id);
              }
        }
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

    int avg_group_size = n / n_groups;

    const int* RESTRICT p_ids = ids.data();
    int* RESTRICT p_out = out.data();

    // Sorted ids make each group one contiguous run, so its count is the run length.
    if (sorted && avg_group_size >= internal::min_gallop_run){

        int i = 0;

        while (i < n){
            int end = internal::run_end(p_ids, i, n);
            p_out[p_ids[i]] = end - i;
            i = end;
        }

    } else if (n_groups <= 2048 && avg_group_size >= 16){

        // When N groups is small and group IDs are repeated, it is better to use multiple histograms
        // 4 histograms of size N groups each
        uint32_t ng = static_cast<uint32_t>(n_groups);
        std::vector<uint32_t> histograms(4 * static_cast<std::size_t>(ng), uint32_t(0));
        internal::sort::count_over_histograms(
            [p_ids](int i) noexcept { return static_cast<uint32_t>(p_ids[i]); },
            0, n, ng, histograms.data()
        );

        const uint32_t* RESTRICT hist1 = histograms.data();
        const uint32_t* RESTRICT hist2 = hist1 + ng;
        const uint32_t* RESTRICT hist3 = hist2 + ng;
        const uint32_t* RESTRICT hist4 = hist3 + ng;

        // Final group count equals the group counts of each histogram
        for (uint32_t g = 0; g < ng; ++g){
            p_out[g] = static_cast<int>(hist1[g] + hist2[g] + hist3[g] + hist4[g]);
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
          int n = ids.length();
          r_vec<r_int> out(n);
          const int* RESTRICT p_ids = ids.data();
          internal::sort::counting_order(
              [p_ids](int i) noexcept { return static_cast<uint32_t>(p_ids[i]); },
              n, static_cast<uint32_t>(n_groups), out.data()
          );
          return out;
      }
  }

};

namespace internal {

template <RVector T>
void check_groups_span_data(const T& x, const groups& g){
    if (x.length() != g.ids.length()) [[unlikely]] {
        abort("check_groups_span_data: data and group IDs must have equal length");
    }
}

template <RVector T>
groups make_unordered_groups(const T& x) {

    using data_t = typename T::data_type;
    using key_type = unwrap_t<data_t>;
    r_size_t n = x.length();

    if (n == 0) return groups(r_vec<r_int>(), 0, false, true);

    r_vec<r_int> group_ids(n);
    int n_groups;

    // Try the dense int table first (For int storage with small range)
    // An all-NA vector falls through to the hash map, which handles NA keys

    int next_id = 0;

    bool done = internal::try_dense_int_map(x, -1, [&](auto&& try_emplace, auto&&) {
        
        auto* RESTRICT px = x.data();
        auto* RESTRICT pid = group_ids.data();

        for (r_size_t i = 0; i < n; ++i) {
            auto [id, inserted] = try_emplace(px[i], next_id);
            // Branch instead of `next_id += inserted` so the common (found) path
            // carries no dependency between iterations
            if (inserted) {
                ++next_id;
            }
            pid[i] = id;
        }
    });

    auto* RESTRICT p_x = x.data();
    auto* RESTRICT p_id = group_ids.data();

    if (!done) {

        uint64_t hash_map_reserve_guess = internal::get_hash_map_reserve_size<T>(p_x, n);
        
        ankerl::unordered_dense::map<
        key_type,
        int,
        internal::r_hash_fn<data_t>,
        internal::r_hash_eq<data_t>
      > lookup;

      lookup.reserve(hash_map_reserve_guess);

      int prev_id = -1;
      for (r_size_t i = 0; i < n; ++i) {

        // Skip consecutive values that are the same (and avoid hash lookup)
        if (i > 0 && internal::r_hash_eq<data_t>()(p_x[i], p_x[i - 1])) {
            p_id[i] = prev_id;
            continue;
        }
        auto [id, inserted] = lookup.try_emplace(p_x[i], next_id);
        if (inserted) {
            ++next_id;
        }
        prev_id = id->second;
        p_id[i] = prev_id;
      }

    }
    
    n_groups = next_id;
    return groups(group_ids, n_groups, false, ids_are_sorted(p_id, n));
}

// Make groups from cppally::order(x)
template <RSortableType T>
groups make_groups_from_order(const r_vec<T>& x, const r_vec<r_int>& o) {
    r_size_t n = x.length();

    if (n != o.length()) [[unlikely]] {
        abort("`x.length()` must match `o.length()`");
    }

    if (n == 0) return groups(r_vec<r_int>(), 0, true, true);
    
    r_vec<r_int> group_ids(n);
    int* RESTRICT p_group_ids = group_ids.data();

    int n_chunks = calc_threads(n);

    // Materialise up-front so view() is multi-thread safe
    (void) x.data();
    const int* RESTRICT p_o = o.data();

    // order() ties NA/NaN even though they are distinct groups
    // so we deal with these ranges separately
    uint32_t tail = static_cast<uint32_t>(n);
    while (tail > 0 && is_na(x.view(p_o[tail - 1]))) {
        --tail;
    }

    int n_groups = 0;

    if (tail > 0) {

        std::unique_ptr<uint8_t[]> new_group(new uint8_t[tail]);
        uint8_t* RESTRICT p_new = new_group.get();
        p_new[0] = 0;
        std::vector<int> chunk_counts(n_chunks);

        // Walk through x in sorted order and identify group IDs when value changes from previous value
        for_each_chunk(tail, n_chunks, [&](int c, uint32_t start, uint32_t end) noexcept {
            int count = 0;
            for (uint32_t i = start == 0u ? 1u : start; i < end; ++i) {
                uint8_t is_new = !identical(x.view(p_o[i]), x.view(p_o[i - 1]));
                p_new[i] = is_new;
                count += is_new;
            }
            chunk_counts[c] = count;
        });

        // Prefix sum
        int total = 0;
        for (int& count : chunk_counts) {
            int chunk_count = count;
            count = total;
            total += chunk_count;
        }

        // Convert new group flags into group IDs and scatter back to x's original order
        for_each_chunk(tail, n_chunks, [&](int c, uint32_t start, uint32_t end) noexcept {
            int id = chunk_counts[c];
            for (uint32_t i = start; i < end; ++i) {
                id += p_new[i];
                p_group_ids[p_o[i]] = id;
            }
        });

        n_groups = total + 1;
    }

    if (tail < n) {
        bool first_is_nan = is_nan(x.view(p_o[tail]));
        bool both_kinds = false;
        for (uint32_t i = tail; i < n; ++i) {
            int idx = p_o[i];
            bool second_kind = is_nan(x.view(idx)) != first_is_nan;
            both_kinds = both_kinds || second_kind;
            p_group_ids[idx] = n_groups + second_kind;
        }
        n_groups += 1 + both_kinds;
    }

    return groups(group_ids, n_groups, /*ordered=*/ true);
}

}

template <RVector T>
inline groups make_groups(const T& x) {
    if (x.is_long()) [[unlikely]] {
        abort("Cannot group a long-vector");
    }
    return internal::make_unordered_groups(x);
}

inline groups make_groups(const r_factors& x) {
    return make_groups(x.value);
}

template <typename T>
requires requires (const T& vec) { make_groups(vec); }
r_vec<r_str> group_names(const T& x, const groups& g) {

    internal::check_groups_span_data(x, g);

    int ng = g.n_groups;
    int n = g.ids.length();

    r_vec<r_str> out(ng);

    const int* RESTRICT p_ids = g.ids.data();

    if (g.sorted){

        // Each group is one contiguous run: name it from its first row
        int i = 0;

        while (i < n){
            out.set(p_ids[i], r_str(as<r_str_view>(x.view(i)), internal::view_tag{}));
            i = internal::run_end(p_ids, i, n);
        }

    } else {

        std::vector<uint8_t> seen(ng, uint8_t(0));
        int n_seen = 0;

        // Name each group at its first occurrence, stopping once all are seen
        for (int i = 0; i < n && n_seen < ng; ++i) {

            int curr_group = p_ids[i];

            if (!seen[curr_group]) {
                out.set(curr_group, r_str(as<r_str_view>(x.view(i)), internal::view_tag{}));
                seen[curr_group] = uint8_t(1);
                ++n_seen;
            }
        }
    }

    return out;
}

// Split x into a std::vector of r_vector chunks (1 for each group).
// To return an R list, simply call as<r_vector<r_sexp>>() on the result.
template <RVector T>
CPPALLY_NOINLINE std::vector<T> split(const T& x, const groups& g){

    internal::check_groups_span_data(x, g);

    int n_groups = g.n_groups;
    r_vec<r_int> sizes = g.counts();

    std::vector<T> chunks;
    chunks.reserve(n_groups);

    if (g.sorted){
        for (int i = 0, start = 0; i < n_groups; ++i){
            int size = sizes.get(i);
            T& chunk = chunks.emplace_back(size);
            r_copy_n(chunk, x, 0, size, start);
            start += size;
        }
        return chunks;
    }

    for (int i = 0; i < n_groups; ++i){
        chunks.emplace_back(unwrap(sizes.get(i)));
    }

    r_vec<r_int> group_ids = g.ids;
    int n = group_ids.length();

    std::vector<int> pos(n_groups, 0);
    for (int i = 0; i < n; ++i){
        int group_id = group_ids.get(i);
        chunks[group_id].set(pos[group_id]++, x.view(i));
    }
    return chunks;
}

}

#endif
