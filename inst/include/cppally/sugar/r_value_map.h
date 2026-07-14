#ifndef CPPALLY_R_VALUE_MAP_H
#define CPPALLY_R_VALUE_MAP_H

#include <cppally/r_vec.h>
#include <cppally/sugar/r_stats.h>
#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

namespace cppally {

namespace internal {

// Single source of truth for when a dense int table beats a hash map:
// span must be small in absolute terms (memory) and relative to n (build time)
inline bool use_int_table(int64_t range_span, r_size_t n) {
    return range_span <= std::min<int64_t>(20000000, std::max<int64_t>(65536, static_cast<int64_t>(n) * 4));
}

// The 'try the integer optimisation first' entry point.
// Runs body(try_emplace, find_or) against a dense int table and returns true,
// or returns false WITHOUT calling body when the keys don't suit a table
// (all-NA, or too wide a range) - the caller then runs its own fallback.
// Results should escape through body's captures, not its return value.
//
//   try_emplace(key, v) -> std::pair<int, bool>
//       Inserts v under key if absent. Returns the value now stored under key,
//       and whether this call inserted it. Only call with values of `keys`.
//   find_or(key, not_found) -> int
//       The value stored under key, or `not_found` if absent. Any key is fine.
//
// NA is an ordinary key (it gets a side slot).
// The table is a plain array of int, so it needs one int to mark unoccupied
// slots: `empty_value` must be a value the caller never stores
// (-1 when storing 0-indexed positions/ids, 0 when storing presence flags)
template <typename F>
bool try_dense_int_map(const r_vec<r_int>& keys, int empty_value, F&& body) {

    r_size_t n = keys.length();

    r_vec<r_int> rng = range(keys, /*na_rm=*/true);

    int min_val = unwrap(rng.get(0));
    int max_val = unwrap(rng.get(1));

    // If keys had only NAs, result would also be c(NA, NA)
    bool all_nas = is_na(min_val) && is_na(max_val);
    int64_t range_span = all_nas ? 0 : static_cast<int64_t>(max_val) - static_cast<int64_t>(min_val);

    if (all_nas || !use_int_table(range_span, n)) {
        return false;
    }

    // Table maps (key - min_val) -> int, NA keys get a side slot
    std::vector<int> table(range_span + 1, empty_value);
    int na_slot = empty_value;
    int* RESTRICT p_table = table.data();

    // na_slot is the only capture that must be by reference (mutable shared state).
    auto try_emplace = [&na_slot, p_table, min_val, empty_value](int key, int v) -> std::pair<int, bool> {
        int& slot = is_na(key) ? na_slot : p_table[static_cast<size_t>(key - min_val)];
        if (slot == empty_value) {
            slot = v;
            return {v, true};
        }
        return {slot, false};
    };

    auto find_or = [&na_slot, p_table, min_val, max_val, empty_value](int key, int not_found) -> int {
        if (is_na(key)) {
            return na_slot == empty_value ? not_found : na_slot;
        }
        if (key < min_val || key > max_val) {
            return not_found;
        }
        int slot = p_table[static_cast<size_t>(key - min_val)];
        return slot == empty_value ? not_found : slot;
    };

    body(try_emplace, find_or);
    return true;
}

}

}

#endif
