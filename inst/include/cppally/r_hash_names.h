#ifndef CPPALLY_R_HASH_NAMES_H
#define CPPALLY_R_HASH_NAMES_H

#include <cppally/r_vec.h>
#include <ankerl/unordered_dense.h>
#include <optional>

namespace cppally {

namespace internal {

using r_names_map_t = ankerl::unordered_dense::map<SEXP, int>;

// Wraps a vector of unique names with a lazily-built hash map for O(1) lookup.
// Assumes no duplicate names. Indices stored are 0-based; callers using 1-based
// schemes (e.g. factor codes) should adjust at the call site.
struct hashed_names {

    r_vec<r_str_view> names;
    mutable std::optional<r_names_map_t> map;

    hashed_names() = default;
    explicit hashed_names(r_vec<r_str_view> names_) : names(std::move(names_)) {}

    void reset_map() noexcept {
        map.reset(); 
    }

    private:

    void lazy_build() const {
        if (map.has_value()) return;
        int n = names.length();
        map.emplace();
        map->reserve(static_cast<std::size_t>(n));
        for (int i = 0; i < n; ++i){
            map->emplace(names.data()[i], i);
        }
    }

    public:

    template <RStringType T>
    r_int find(const T& name) const {
        lazy_build();
        auto it = map->find(unwrap(name));
        if (it == map->end()) {
            return na<r_int>();
        }
        return r_int(it->second);
    }

};

}

}

#endif
