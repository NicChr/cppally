#ifndef CPPALLY_R_HASH_MAP_H
#define CPPALLY_R_HASH_MAP_H

#include <cppally/hash/hash.h>
#include <cppally/hash/r_str_map.h>
#include <ankerl/unordered_dense.h>
#include <type_traits>

namespace cppally {

namespace internal {

// Unordered dense hash map, with strings keyed by text
template <typename T, typename V>
using r_hash_map = std::conditional_t<
    RStringType<T>,
    r_str_map<V>,
    ankerl::unordered_dense::map<unwrap_t<T>, V, r_hash_fn<T>, r_hash_eq<T>>
>;

}

}

#endif
