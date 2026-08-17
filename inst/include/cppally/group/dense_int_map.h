#ifndef CPPALLY_R_DENSE_INT_MAP_H
#define CPPALLY_R_DENSE_INT_MAP_H

#include <cppally/vector/r_vector.h>
#include <cppally/stats/stats.h>
#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

namespace cppally {

namespace internal {

// Single source of truth for when a dense int table beats a hash map:
// the table must be small in absolute terms (memory) and relative to n (build time).
inline constexpr uint64_t max_table_bytes = 80000000;
inline constexpr uint64_t min_table_bytes = 262144;
inline constexpr uint64_t table_bytes_per_row = 64;

inline bool table_fits_budget(uint64_t table_bytes, r_size_t n) {
    return table_bytes <= std::min<uint64_t>(max_table_bytes, std::max<uint64_t>(min_table_bytes, static_cast<uint64_t>(n) * table_bytes_per_row));
}

template <CppIntegerNumber Val>
inline bool use_int_table(uint64_t range_span, r_size_t n) {

    if (range_span > ( max_table_bytes / sizeof(Val) )) {
        return false;
    }

    return table_fits_budget(range_span * sizeof(Val), n);
}

// Presence tables store a bit per slot, so they earn ~32-64x more span than a value table for the
// same budget - the shift can't overflow so it needs no pre-filter
// inline bool use_int_set(uint64_t range_span, r_size_t n) {
//     return table_fits_budget(range_span >> 3, n);
// }

// The 'try the integer optimisation first' entry point.
// Runs body(try_emplace, find_or) against a dense int table and returns true,
// or returns false WITHOUT calling body when the keys don't suit a table
// (all-NA, or too wide a range) - the caller then runs its own fallback.
// Results should escape through body's captures, not its return value.
//
//   try_emplace(key, v) -> std::pair<Val, bool>
//       Inserts v under key if absent. Returns the value now stored under key,
//       and whether this call inserted it. Only call with values of `keys`.
//   find_or(key, not_found) -> Val
//       The value stored under key, or `not_found` if absent. Any key is fine.
//
// `Key` and `Val` are independent widths. `Key` is fixed by the data being indexed and is what
// NA detection runs on; `Val` is chosen by the caller and covers everything on the value side -
// what gets stored, the `empty_value` sentinel, and `find_or`'s `not_found`. Callers wanting
// 64-bit values over 32-bit keys (e.g. `match<r_int64>` on an int haystack) specify `Val` explicitly.
//
// NA is an ordinary key (it gets a side slot).
// The table needs one value to mark unoccupied slots, so `empty_value` must be a value the
// caller never stores (-1 when storing 0-indexed positions/ids, 0 when storing presence flags).
// Note `not_found` may legitimately be NA - it is returned, never stored, so it does not
// collide with `empty_value`

// Builds the table over [min_val, max_val] and runs body. Both bounds must be
// non-NA and the span already vetted as worth a table
template <CppIntegerNumber Val, CppIntegerNumber Key, typename F>
bool run_dense_int_map(Key min_val, Key max_val, Val empty_value, F&& body) {

    uint64_t range_span = static_cast<uint64_t>(max_val) - static_cast<uint64_t>(min_val);

    // Table maps (key - min_val) -> Val, NA keys get a side slot
    std::vector<Val> table(range_span + 1, empty_value);
    Val na_slot = empty_value;
    Val* RESTRICT p_table = table.data();

    // na_slot is the only capture that must be by reference (mutable shared state).
    auto try_emplace = [&na_slot, p_table, min_val, empty_value](Key key, Val v) -> std::pair<Val, bool> {
        Val& slot = is_na(key) ? na_slot : p_table[static_cast<size_t>(key - min_val)];
        if (slot == empty_value) {
            slot = v;
            return {v, true};
        }
        return {slot, false};
    };

    auto find_or = [&na_slot, p_table, min_val, max_val, empty_value](Key key, Val not_found) -> Val {
        if (is_na(key)) {
            return na_slot == empty_value ? not_found : na_slot;
        }
        if (key < min_val || key > max_val) {
            return not_found;
        }
        Val slot = p_table[static_cast<size_t>(key - min_val)];
        return slot == empty_value ? not_found : slot;
    };

    body(try_emplace, find_or);
    return true;
}

// No table for these keys, the caller runs its own fallback
// `Val` leads so that an explicit `try_dense_int_map<Val>(...)` binds the value width, not the key vector
template <CppIntegerNumber Val, typename T, typename F>
bool try_dense_int_map(const T&, Val, F&&) {
    return false;
}

template <CppIntegerNumber Val, RIntegerNumber T, typename F>
bool try_dense_int_map(const r_vec<T>& keys, Val empty_value, F&& body) {

    r_size_t n = keys.length();

    r_vec<T> rng = range(keys, /*na_rm=*/true);

    auto min_val = unwrap(rng.get(0));
    auto max_val = unwrap(rng.get(1));

    // If keys had only NAs, result would also be c(NA, NA)
    bool all_nas = is_na(min_val) && is_na(max_val);
    uint64_t range_span = all_nas ? 0 : static_cast<uint64_t>(max_val) - static_cast<uint64_t>(min_val);

    if (all_nas || !use_int_table<Val>(range_span, n)) {
        return false;
    }

    return run_dense_int_map<Val>(min_val, max_val, empty_value, std::forward<F>(body));
}

template <CppIntegerNumber Val, typename F>
bool try_dense_int_map(const r_vec<r_lgl>&, Val empty_value, F&& body) {
    return run_dense_int_map<Val>(0, 1, empty_value, std::forward<F>(body));
}

// Presence-only counterpart of the dense int table, for callers that only ask "have I seen this
// key before?" and never store a value against it. One bit per slot rather than one `Val`, so it
// is 32-64x smaller for the same span - which both admits far wider spans under the memory budget
// and keeps the table cache-resident where a value table would be missing to DRAM.
//
//   insert(key) -> bool
//       true when this call is the first sighting of `key`. NA is an ordinary key
//
// There is no `empty_value` to reserve: an unset bit means absent, so every key value is storable

// // Builds the bitset over [min_val, max_val] and runs body. Both bounds must be
// // non-NA and the span already vetted as worth a table
// template <CppIntegerNumber Key, typename F>
// bool run_dense_int_set(Key min_val, Key max_val, F&& body) {

//     uint64_t range_span = static_cast<uint64_t>(max_val) - static_cast<uint64_t>(min_val);

//     // Table maps (key - min_val) -> one bit, NA keys get a side flag.
//     // `std::vector<bool>` is a bit array, so this is 1 bit per slot without the manual packing
//     std::vector<uint8_t> table(range_span + 1, uint8_t(0));
//     bool na_seen = false;

//     auto insert = [&table, &na_seen, min_val](Key key) -> bool {

//         // Returning early also keeps `key - min_val` unevaluated for NA keys - NA is the
//         // minimum representable value, so the subtraction would overflow
//         if (is_na(key)) {
//             bool seen = na_seen;
//             na_seen = true;
//             return !seen;
//         }

//         uint64_t slot = static_cast<uint64_t>(key - min_val);
//         bool seen = table[slot];
//         table[slot] = true;

//         return !seen;
//     };

//     body(insert);
//     return true;
// }

// // No table for these keys, the caller runs its own fallback
// template <typename T, typename F>
// bool try_dense_int_set(const T&, F&&) {
//     return false;
// }

// template <RIntegerNumber T, typename F>
// bool try_dense_int_set(const r_vec<T>& keys, F&& body) {

//     r_size_t n = keys.length();

//     r_vec<T> rng = range(keys, /*na_rm=*/true);

//     auto min_val = unwrap(rng.get(0));
//     auto max_val = unwrap(rng.get(1));

//     // If keys had only NAs, result would also be c(NA, NA)
//     bool all_nas = is_na(min_val) && is_na(max_val);
//     uint64_t range_span = all_nas ? 0 : static_cast<uint64_t>(max_val) - static_cast<uint64_t>(min_val);

//     if (all_nas || !use_int_set(range_span, n)) {
//         return false;
//     }

//     return run_dense_int_set(min_val, max_val, std::forward<F>(body));
// }

// template <typename F>
// bool try_dense_int_set(const r_vec<r_lgl>&, F&& body) {
//     return run_dense_int_set(0, 1, std::forward<F>(body));
// }

}

}

#endif
