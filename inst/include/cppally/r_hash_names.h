#ifndef CPPALLY_R_HASH_NAMES_H
#define CPPALLY_R_HASH_NAMES_H

#include <ankerl/unordered_dense.h>
#include <optional>
#include <memory>
#include <bit>
#include <cstdint>

namespace cppally {

// Three layers for name → index lookups on R named vectors:
//
//   sexp_index_table  raw open-addressing hash; slots store `index + 1`
//   names_map         lazily builds a table from a STRSXP, owns the SEXP
//   cache_registry    lets r_vec wrappers around the same SEXP share one
//                     names_map, and lets attribute mutations invalidate
//                     it by SEXP key
//
// Lazy in two stages: names_map captures the STRSXP only when ensured;
// the table is built only on the first find(). Most wrappers never look
// up by name, so both stages matter.

// Knuth multiplicative — upper bits of the product distribute well even
// when the input bits are correlated (e.g. aligned pointers).
inline std::size_t sexp_data_hash(SEXP p) noexcept {
    constexpr auto phi = 0x9E3779B97F4A7C15ull;
    return (reinterpret_cast<std::uintptr_t>(p) * phi);
}

namespace internal {

// Open-addressing hash from SEXP keys to indices into an external names
// array. Keys aren't stored — comparison goes back through names_ptr_, so
// the table only needs a single int[] of ~1.5x n ints (half ankerl's
// footprint, one fewer cache line touched per insert).
//
// names_ptr_ is unowned. The owner (names_map) keeps the SEXP alive via
// r_sexp; rebind_to_storage covers the edge case where the array is
// replaced but stored indices are still valid.
struct sexp_index_table {

    // Slots store `index + 1`, so the value-initialised 0 means "empty".
    // These helpers keep the encoding local to one place.
    static constexpr int EMPTY_SLOT = 0;
    static int to_slot(int idx)    noexcept { return idx + 1; }
    static int from_slot(int slot) noexcept { return slot - 1; }

    sexp_index_table() = default;

    sexp_index_table(const sexp_index_table&) = delete;
    sexp_index_table& operator=(const sexp_index_table&) = delete;
    sexp_index_table(sexp_index_table&&) noexcept = default;
    sexp_index_table& operator=(sexp_index_table&&) noexcept = default;

    // Reserve room for at least n entries with ~33% headroom so a single
    // post-build append (e.g. r_factors::append_level) cannot fill the table.
    void reserve(std::size_t n, const SEXP* names_ptr) {
        std::size_t target = n + (n >> 1) + 1;
        std::size_t cap = std::bit_ceil(std::max<std::size_t>(target, 16));
        slots_ = std::make_unique<int[]>(cap); // value-init: all EMPTY_SLOT
        capacity_ = cap;
        mask_ = cap - 1;
        shift_ = 64 - std::countr_zero(cap);
        size_ = 0;
        names_ptr_ = names_ptr;
    }

    // Re-anchor the table to a new backing array. Use when the underlying
    // names SEXP was replaced but stored indices are still valid — e.g.
    // r_factors::append_level builds a strictly extended levels vector
    // where every old index still points at the same CHARSXP.
    void rebind_to_storage(const SEXP* names_ptr) noexcept { names_ptr_ = names_ptr; }

    // Insert `idx` into the table. The key is read from names_ptr_[idx].
    // First-wins. Returns false only if the table is completely full.
    bool insert(int idx) noexcept {
        if (size_ >= capacity_) return false;
        SEXP key = names_ptr_[idx];
        std::size_t pos = hash_(key);
        while (slots_[pos] != EMPTY_SLOT) {
            if (names_ptr_[from_slot(slots_[pos])] == key) return true;
            pos = (pos + 1) & mask_;
        }
        slots_[pos] = to_slot(idx);
        ++size_;
        return true;
    }

    int find(SEXP key) const noexcept {
        if (capacity_ == 0) return -1;
        std::size_t pos = hash_(key);
        while (slots_[pos] != EMPTY_SLOT) {
            int idx = from_slot(slots_[pos]);
            if (names_ptr_[idx] == key) return idx;
            pos = (pos + 1) & mask_;
        }
        return -1;
    }

    std::size_t size() const noexcept { return size_; }
    bool empty() const noexcept { return size_ == 0; }

    private:

    std::unique_ptr<int[]> slots_;
    const SEXP* names_ptr_ = nullptr;
    std::size_t capacity_ = 0;
    std::size_t mask_ = 0;
    int shift_ = 64;
    std::size_t size_ = 0;

    // Knuth multiplicative — upper bits of the product distribute well even
    // when the input bits are correlated (e.g. aligned pointers).
    std::size_t hash_(SEXP p) const noexcept {
        return sexp_data_hash(p) >> shift_;
    }
};

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

        if (static_cast<SEXP>(*names) == R_NilValue) return;

        r_size_t n = Rf_xlength(*names);
        if (n > std::numeric_limits<int>::max()) [[unlikely]] {
            abort("Long vector name hashing is not supported");
        }

        const SEXP* RESTRICT p_names = STRING_PTR_RO(*names);
        map->reserve(static_cast<std::size_t>(n), p_names);
        int n_ = static_cast<int>(n);
        for (int i = 0; i < n_; ++i){
            map->insert(i);
        }
    }

    public:

    // Returns the 0-based index of `name` in the captured STRSXP, plus `offset`.
    // Factor levels pass offset=1 to convert to R's 1-based factor codes.
    template <RStringType T>
    r_int find(const T& name, int offset = 0) const {
        lazy_build();
        int idx = map->find(unwrap(name));
        if (idx < 0) {
            return na<r_int>();
        }
        return r_int(idx + offset);
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

    void sweep() noexcept {
        for (auto it = storage_.begin(); it != storage_.end(); ) {
            if (it->second.expired()) it = storage_.erase(it);
            else ++it;
        }
    }

    public:

    std::shared_ptr<names_map> get_or_create(SEXP s) {
        // Equivalent to (++counter_ % SWEEP_INTERVAL) == 0 — power-of-two interval lets us use &
        if ((++counter_ & (SWEEP_INTERVAL - 1)) == 0) sweep();

        auto [it, inserted] = storage_.try_emplace(s);
        if (!inserted) {
            if (auto sp = it->second.lock()) return sp;
        }
        auto sp = std::make_shared<names_map>();
        it->second = sp;
        return sp;
    }

    std::shared_ptr<names_map> try_lookup(SEXP s) noexcept {
        auto it = storage_.find(s);
        return it != storage_.end() ? it->second.lock() : nullptr;
    }

    // Bind `sp` to `s` directly — used when sharing an already-built cache
    // across sibling wrappers (see r_factors::remove).
    void store(SEXP s, std::shared_ptr<names_map> sp) {
        storage_[s] = std::move(sp);
    }
};

inline cache_registry& name_cache()   { static cache_registry r; return r; }
inline cache_registry& levels_cache() { static cache_registry r; return r; }

}

}

#endif
