#ifndef CPPALLY_R_HASH_NAMES_H
#define CPPALLY_R_HASH_NAMES_H

#include <cppally/r_setup.h>
#include <ankerl/unordered_dense.h>
#include <optional>
#include <memory>

namespace cppally {

// Two layers for name → index lookups on R named vectors:
//
//   names_map         lazily builds a hash table from a STRSXP, owns the SEXP
//   cache_registry    lets r_vec wrappers around the same SEXP share one
//                     names_map, and lets attribute mutations invalidate
//                     it by SEXP key
//
// Lazy in two stages: names_map captures the STRSXP only when ensured;
// the table is built only on the first find(). Most wrappers never look
// up by name, so both stages matter.

namespace internal {

// CHARSXP pointer → 0-based index into a STRSXP
using sexp_index_table = ankerl::unordered_dense::map<SEXP, int>;

// Lazily-built hash over a STRSXP for O(1) name → index lookup.
// `names` is the protected STRSXP; nullopt = not yet captured from the parent.
// `map` is the hash table; null = not yet built from `names`.
// `map` is a shared_ptr so sibling r_vec wrappers around the same SEXP
// can share a single built table without copying.
struct names_map {

    std::optional<r_sexp> names;
    mutable std::shared_ptr<sexp_index_table> map;

    names_map() = default;

    void invalidate() noexcept {
        names.reset();
        map.reset();
    }

    private:

    void lazy_build() const {
        if (map) return;

        map = std::make_shared<sexp_index_table>();

        if (!names.has_value()) return;
        SEXP nms = *names;
        if (nms == R_NilValue) return;

        r_size_t n = Rf_xlength(nms);
        if (n == 0) return;
        if (n > std::numeric_limits<int>::max()) [[unlikely]] {
            abort("Long vector name hashing is not supported");
        }

        const SEXP* RESTRICT p_names = STRING_PTR_RO(nms);
        map->reserve(static_cast<std::size_t>(n));
        int n_ = static_cast<int>(n);
        for (int i = 0; i < n_; ++i){
            // try_emplace = first-wins on duplicate names
            map->try_emplace(p_names[i], i);
        }
    }

    public:

    // Returns the 0-based index of `name` in the captured STRSXP, plus `offset`.
    // Factor levels pass offset=1 to convert to R's 1-based factor codes.
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

// Per-attribute cache registry: keyed by parent SEXP*, so any two wrappers
// around the same SEXP converge on the same names_map. A mutation through
// any of them is visible to all.
//
// weak_ptr storage means the cache is removed when the last wrapper holding it
// goes out of scope. Dead slots are reused on next lookup or swept periodically.
//
// NOT thread-safe — assumes wrapper construction outside OMP regions.
struct cache_registry {

    private:

    static constexpr std::size_t SWEEP_INTERVAL = 1024;

    ankerl::unordered_dense::map<SEXP, std::weak_ptr<names_map>> storage_;
    std::size_t counter_ = 0;

    void maybe_sweep() noexcept {
        // Equivalent to (++counter_ % SWEEP_INTERVAL) == 0 — power-of-two interval lets us use &
        if ((++counter_ & (SWEEP_INTERVAL - 1)) == 0){
            for (auto it = storage_.begin(); it != storage_.end(); ) {
                if (it->second.expired()) it = storage_.erase(it);
                else ++it;
            }
        }
    }

    public:

    std::shared_ptr<names_map> get_or_create(SEXP s) {
        maybe_sweep();
        auto [it, inserted] = storage_.try_emplace(s);
        if (!inserted) {
            if (auto sp = it->second.lock()) return sp;
        }
        auto sp = std::make_shared<names_map>();
        it->second = sp;
        return sp;
    }

    std::shared_ptr<names_map> try_lookup(SEXP s) noexcept {
        maybe_sweep();
        auto it = storage_.find(s);
        return it != storage_.end() ? it->second.lock() : nullptr;
    }

    // Bind `sp` to `s` directly — used when sharing an already-built cache
    // across sibling wrappers
    void store(SEXP s, std::shared_ptr<names_map> sp) {
        storage_.insert_or_assign(s, std::move(sp));
    }
};

inline cache_registry& name_cache()   { static cache_registry r; return r; }
inline cache_registry& levels_cache() { static cache_registry r; return r; }

}

}

#endif
