#ifndef CPPALLY_R_VALUE_MAP_H
#define CPPALLY_R_VALUE_MAP_H

#include <cppally/r_vec.h>
#include <cppally/sugar/r_stats.h>
#include <cppally/sugar/r_hash.h>
#include <ankerl/unordered_dense.h>
#include <algorithm>
#include <cstdint>
#include <type_traits>
#include <utility>
#include <vector>

namespace cppally {

namespace internal {

// Single source of truth for choosing a dense value table over a hash map:
// span must be small in absolute terms (memory) and relative to n (build time)
inline bool use_int_table(int64_t range_span, r_size_t n) {
    return range_span <= std::min<int64_t>(20000000, std::max<int64_t>(65536, static_cast<int64_t>(n) * 4));
}

// Runs body(try_emplace, find_or) against a key -> V lookup backend, choosing
// a dense table when keys are small-range ints and a hash map otherwise.
//
//   try_emplace(key, v) -> std::pair<V, bool>
//       Inserts v under key if absent. Returns the value now stored under key,
//       and whether this call inserted it. Only call with values of `keys`.
//   find_or(key, not_found) -> V
//       The value stored under key, or `not_found` if absent. Any key is fine.
//
// NA is an ordinary key in both backends.
//
// The dense table is a plain array of V, so it needs one V to mark unoccupied
// slots: `empty_value` must be a value the caller never stores (-1 when storing
// 0-indexed positions/ids, 0 when storing presence flags). The hash backend
// tracks occupancy itself and ignores it.
template <typename V, RVal T, typename F>
auto with_value_map(const r_vec<T>& keys, V empty_value, F&& body) {
    
    static_assert(std::is_integral_v<V>, "with_value_map() requires an integral payload type V");

    r_size_t n = keys.length();

    // The V cap keeps wide payloads (e.g. match<r_int64> positions) hash-only,
    // avoiding a whole family of dense-table instantiations
    if constexpr (is<T, r_int> && sizeof(V) <= 4) {

        r_vec<T> rng = range(keys, /*na_rm=*/true);

        int min_val = unwrap(rng.get(0));
        int max_val = unwrap(rng.get(1));

        // If keys had only NAs, result would also be c(NA, NA)
        bool all_nas = is_na(min_val) && is_na(max_val);
        int64_t range_span = all_nas ? 0 : static_cast<int64_t>(max_val) - static_cast<int64_t>(min_val);

        if (!all_nas && use_int_table(range_span, n)) {

            // Table maps (key - min_val) -> V, NA keys get a side slot
            std::vector<V> table(range_span + 1, empty_value);
            V na_slot = empty_value;
            V* p_table = table.data();

            auto try_emplace = [&](int key, V v) -> std::pair<V, bool> {
                // Safe subtraction because in-range keys were validated by range()
                V& slot = is_na(key) ? na_slot : p_table[static_cast<size_t>(key - min_val)];
                if (slot == empty_value) {
                    slot = v;
                    return {v, true};
                }
                return {slot, false};
            };

            auto find_or = [&](int key, V not_found) -> V {
                if (is_na(key)) {
                    return na_slot == empty_value ? not_found : na_slot;
                }
                if (key < min_val || key > max_val) {
                    return not_found;
                }
                V slot = p_table[static_cast<size_t>(key - min_val)];
                return slot == empty_value ? not_found : slot;
            };

            return body(try_emplace, find_or);
        }
    }

    ankerl::unordered_dense::map<unwrap_t<T>, V, r_hash<T>, r_hash_eq<T>> lookup;
    lookup.reserve(get_hash_map_reserve_size<T>(n));

    auto try_emplace = [&](unwrap_t<T> key, V v) -> std::pair<V, bool> {
        auto [it, inserted] = lookup.try_emplace(key, v);
        return {it->second, inserted};
    };

    auto find_or = [&](unwrap_t<T> key, V not_found) -> V {
        auto it = lookup.find(key);
        return it == lookup.end() ? not_found : it->second;
    };

    return body(try_emplace, find_or);
}

}

}

#endif
