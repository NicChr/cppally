#ifndef CPPALLY_R_HASH_NAMES_H
#define CPPALLY_R_HASH_NAMES_H

#include <cppally/r_sexp/r_sexp.h>
#include <cppally/vector/vector_utils.h>
#include <cppally/hash/r_str_map.h>
#include <ankerl/unordered_dense.h>
#include <R_ext/Visibility.h>
#include <limits>
#include <optional>
#include <utility>
#include <cstdint>

namespace cppally {

// Three layers for name → index lookups on R named vectors:
//
//   r_str_map         text-keyed hash from each name to its first index
//   names_map         lazily builds a table from a STRSXP, owns the SEXP
//   cache_registry    lets r_vec wrappers around the same SEXP share one
//                     names_map, and lets attribute mutations invalidate
//                     it by SEXP key
//
// Lazy in two stages: names_map captures the STRSXP only when ensured;
// the table is built only on the first find(). Most wrappers never look
// up by name, so both stages matter.

namespace internal {

struct cache_registry;

// Lazily built names map which contains the names STRSXP and its associated hash map, allowing O(1) name lookup.
// `names_map` can be shared by multiple wrappers (siblings) around 1 SEXP.
// Siblings can reach the names and map via a registry which is keyed on the SEXP.
// `nullopt` for `names` means either not yet captured or invalidated.
// `nullopt` for `map` means not yet built.
struct names_map {

    std::optional<r_sexp> names;
    mutable std::optional<r_str_map<int>> map;

    SEXP owner = nullptr; // names_map owner
    cache_registry* registry = nullptr; // Pointer to cache registry of names_map owners
    bool accessed = false; // flag to keep track of first access (important since we cache on 2nd access)

    names_map(SEXP owning_sexp, cache_registry* owning_registry) noexcept : owner(owning_sexp), registry(owning_registry) {}
    names_map(const names_map&) = delete;
    names_map& operator=(const names_map&) = delete;

    // Remove registry entry on destruction
    ~names_map();

    void invalidate() noexcept {
        names.reset();
        map.reset();
        accessed = false;
    }

    private:

    std::uint32_t refs = 0; // refcount

    void lazy_build() const {
        
        if (map) return;

        r_str_map<int> built;

        SEXP nms = names.has_value() ? static_cast<SEXP>(*names) : R_NilValue;
        r_size_t n = nms == R_NilValue ? 0 : Rf_xlength(nms);

        if (n > std::numeric_limits<int>::max()) [[unlikely]] {
            abort("Long vector name hashing is not supported");
        }

        if (n > 0) {
            const SEXP* p_names = safe[vector_ptr_ro<r_str>](nms);
            built.reserve(static_cast<std::size_t>(n));
            int n_ = static_cast<int>(n);
            for (int i = 0; i < n_; ++i){
                built.lazy_insert(p_names[i], i);
            }
        }

        map = std::move(built);
    }

    public:

    // Returns the 0-based index of `name` in the captured STRSXP, plus `offset`.
    // Factor levels pass offset=1 to convert to R's 1-based factor codes.
    r_int find(r_str_view name, int offset = 0) const {
        lazy_build();
        auto it = map->lazy_find(unwrap(name));
        if (it == map->end()) {
            return na<r_int>();
        }
        return r_int(it->second + offset);
    }

    // Non-atomic intrusive handle
    struct ptr {

        ptr() noexcept = default;
        explicit ptr(names_map* p) noexcept : p_(p) { acquire(); }
        ptr(const ptr& other) noexcept : p_(other.p_) { acquire(); }
        ptr(ptr&& other) noexcept : p_(std::exchange(other.p_, nullptr)) {}

        ptr& operator=(ptr other) noexcept {
            std::swap(p_, other.p_);
            return *this;
        }

        ~ptr() { release(p_); }

        void reset() noexcept { release(std::exchange(p_, nullptr)); }

        names_map* operator->() const noexcept { return p_; }
        explicit operator bool() const noexcept { return p_ != nullptr; }

        private:

        names_map* p_ = nullptr;

        void acquire() noexcept {
            if (p_) {
                ++p_->refs;
            }
        }

        static void release(names_map* p) noexcept {
            if (p && --p->refs == 0) {
                destroy(p);
            }
        }
        
        CPPALLY_NOINLINE static void destroy(names_map* p) noexcept {
            delete p;
        }
    };
};

// Per-attribute cache registry: keyed by parent SEXP*, so any two wrappers
// around the same SEXP converge on the same names_map. A mutation through
// any of them is visible to all.
//
// NOT thread-safe — assumes wrapper construction outside OMP regions.
struct cache_registry {

    private:

    friend struct names_map;
    
    ankerl::unordered_dense::map<SEXP, names_map*> storage_;

    void erase(SEXP s) noexcept {
        storage_.erase(s);
    }

    public:

    // Find or insert in a single probe
    names_map::ptr get_or_create(SEXP s) {
        names_map*& entry = storage_[s];
        // nullptr only if new
        if (!entry) {
            entry = new names_map(s, this);
        }
        return names_map::ptr(entry);
    }

    // Non-owning, for immediate use only
    names_map* try_lookup(SEXP s) noexcept {
        if (storage_.empty()){
            return nullptr;
        }
        auto it = storage_.find(s);
        return it != storage_.end() ? it->second : nullptr;
    }
};

inline names_map::~names_map() {
    registry->erase(owner);
}

inline attribute_hidden cache_registry& name_cache()   { static cache_registry& r = *new cache_registry; return r; }
inline attribute_hidden cache_registry& levels_cache() { static cache_registry& r = *new cache_registry; return r; }

}

}

#endif
