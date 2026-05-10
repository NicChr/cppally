#ifndef CPPALLY_R_HASH_NAMES_H
#define CPPALLY_R_HASH_NAMES_H

#include <ankerl/unordered_dense.h>
#include <optional>
#include <memory>

namespace cppally {

namespace internal {

using r_names_map_t = ankerl::unordered_dense::map<SEXP, int>;

// Lazily-built hash over a STRSXP for O(1) name → index lookup.
// `names` is the protected STRSXP; nullopt = not yet captured from the parent.
// `map` is the hash table; nullopt = not yet built from `names`.
struct names_map {

    std::optional<r_sexp> names;
    mutable std::optional<r_names_map_t> map;

    names_map() = default;

    void invalidate() noexcept {
        names.reset();
        map.reset();
    }

    void reset_map() noexcept {
        map.reset();
    }

    private:

    void lazy_build() const {
        if (map.has_value()) return;
        
        map.emplace();
        
        if (static_cast<SEXP>(*names) == R_NilValue) return;
        
        r_size_t n = Rf_xlength(*names);
        if (n > std::numeric_limits<int>::max()) [[unlikely]] {
            abort("Long vector name hashing is not supported");
        }

        map->reserve(static_cast<std::size_t>(n));
        const SEXP* RESTRICT p_names = STRING_PTR_RO(*names);
        int n_ = static_cast<int>(n);
        for (int i = 0; i < n_; ++i){
            map->emplace(p_names[i], i);
        }
    }

    public:

    template <RStringType T>
    r_int find(const T& name, int offset = 0) const {
        lazy_build();
        auto it = map->find(unwrap(name));
        if (it == map->end()) {
            return na<r_int>();
        }
        return r_int(it->second + offset);
    }
};

// Per-attribute cache registries: keyed by parent SEXP*, so any two wrappers
// around the same SEXP converge on the same names_map. A mutation through any
// of them is visible to all.
//
// weak_ptr storage means the cache dies when the last wrapper holding it dies.
// Dead slots are reused on next lookup or swept periodically.
//
// NOT thread-safe — assumes wrapper construction outside OMP regions.

inline ankerl::unordered_dense::map<SEXP, std::weak_ptr<names_map>>& name_cache_storage() {
    static ankerl::unordered_dense::map<SEXP, std::weak_ptr<names_map>> s;
    return s;
}

inline ankerl::unordered_dense::map<SEXP, std::weak_ptr<names_map>>& levels_cache_storage() {
    static ankerl::unordered_dense::map<SEXP, std::weak_ptr<names_map>> s;
    return s;
}

inline std::size_t& cache_construction_counter() {
    static std::size_t counter = 0;
    return counter;
}

constexpr std::size_t CACHE_SWEEP_INTERVAL = 1024;

inline void sweep_cache_storage(ankerl::unordered_dense::map<SEXP, std::weak_ptr<names_map>>& storage) noexcept {
    for (auto it = storage.begin(); it != storage.end(); ) {
        if (it->second.expired()) {
            it = storage.erase(it);
        } else {
            ++it;
        }
    }
}

inline std::shared_ptr<names_map> get_or_create_cache(
    ankerl::unordered_dense::map<SEXP, std::weak_ptr<names_map>>& storage,
    SEXP s
) {
    // if ((++cache_construction_counter() % 1024) == 0) // Equivalent to this line
    if ((++cache_construction_counter() & (CACHE_SWEEP_INTERVAL - 1)) == 0) {
        sweep_cache_storage(storage);
    }
    auto it = storage.find(s);
    if (it != storage.end()) {
        if (auto sp = it->second.lock()) {
            return sp;
        }
    }
    auto sp = std::make_shared<names_map>();
    storage[s] = sp;
    return sp;
}
// Is there a live cache currently associated with this SEXP?
inline std::shared_ptr<names_map> try_lookup_cache(
    ankerl::unordered_dense::map<SEXP, std::weak_ptr<names_map>>& storage,
    SEXP s
) noexcept {
    auto it = storage.find(s);
    if (it != storage.end()) {
        return it->second.lock();
    }
    return nullptr;
}

inline std::shared_ptr<names_map> get_or_create_name_cache(SEXP s) {
    return get_or_create_cache(name_cache_storage(), s);
}

inline std::shared_ptr<names_map> get_or_create_levels_cache(SEXP s) {
    return get_or_create_cache(levels_cache_storage(), s);
}

inline std::shared_ptr<names_map> try_lookup_name_cache(SEXP s) noexcept {
    return try_lookup_cache(name_cache_storage(), s);
}

inline std::shared_ptr<names_map> try_lookup_levels_cache(SEXP s) noexcept {
    return try_lookup_cache(levels_cache_storage(), s);
}

}

}

#endif
