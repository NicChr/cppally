#ifndef CPPALLY_R_HASH_NAMES_H
#define CPPALLY_R_HASH_NAMES_H

#include <cppally/r_sexp.h>
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

// Knuth multiplicative hash
inline std::uint64_t sexp_data_hash(SEXP p) noexcept {
    constexpr std::uint64_t phi = 0x9E3779B97F4A7C15ull;
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(p)) * phi;
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

    // Linear-probe step with wrap-around. `mask` must be `capacity - 1`
    // for a power-of-two capacity, so the AND wraps `pos` back to 0
    // after the last slot.
    static std::size_t next_probe(std::size_t pos, std::size_t mask) noexcept {
        return (pos + 1) & mask;
    }

    sexp_index_table() = default;

    sexp_index_table(const sexp_index_table&) = delete;
    sexp_index_table& operator=(const sexp_index_table&) = delete;
    sexp_index_table(sexp_index_table&&) noexcept = default;
    sexp_index_table& operator=(sexp_index_table&&) noexcept = default;

    std::unique_ptr<int[]> slots_;
    const SEXP* names_ptr_ = nullptr;
    std::size_t capacity_ = 0;
    std::size_t mask_ = 0;
    // Any value < 64 keeps hash_() well-defined before reserve/grow runs.
    // It's overwritten the moment the table is sized.
    int shift_ = 63;
    std::size_t size_ = 0;

    std::size_t size() const noexcept { return size_; }
    bool empty() const noexcept { return size_ == 0; }
    
    std::size_t hash_(SEXP p) const noexcept {
        return static_cast<std::size_t>(sexp_data_hash(p) >> shift_);
    }

    void set_capacity(std::size_t cap) noexcept {
        capacity_ = cap;
        mask_ = cap - 1;
        shift_ = 64 - std::countr_zero(cap);
    }

    // Reserve room for at least n entries with ~33% headroom. Subsequent
    // appends past this threshold are handled by grow().
    void reserve(std::size_t n, const SEXP* names_ptr) {
        std::size_t target = 2 * (n + 1);
        std::size_t cap = std::bit_ceil(std::max<std::size_t>(target, 16));
        slots_ = std::make_unique<int[]>(cap); // value-init: all EMPTY_SLOT
        set_capacity(cap);
        size_ = 0;
        names_ptr_ = names_ptr;
    }

    // Re-anchor the table to a new backing array. Use when the underlying
    // names SEXP was replaced but stored indices are still valid — e.g.
    // r_factors::append_level builds a strictly extended levels vector
    // where every old index still points at the same CHARSXP.
    void rebind_to_storage(const SEXP* names_ptr) noexcept { names_ptr_ = names_ptr; }

    // Walk the probe chain until we either find `key` or hit an empty slot.
    // The returned position is either the match (slot non-empty, holds `key`)
    // or the first empty slot in the chain. Caller checks which by reading
    // slots_[pos]. Load factor ≤ 0.5 (enforced by insert) guarantees an
    // empty slot exists, so this always terminates.
    std::size_t probe_for(SEXP key) const noexcept {
        std::size_t pos = hash_(key);
        while (slots_[pos] != EMPTY_SLOT) {
            if (names_ptr_[from_slot(slots_[pos])] == key) break;
            pos = next_probe(pos, mask_);
        }
        return pos;
    }

    // Insert `idx` into the table. The key is read from names_ptr_[idx].
    // First-wins on duplicate keys. Grows the table if load factor would
    // exceed 0.5, which bounds probe length and guarantees find() always
    // terminates.
    void insert(int idx) {
        if ((size_ + 1) * 2 > capacity_){
            grow();
        }
        std::size_t pos = probe_for(names_ptr_[idx]);
        if (slots_[pos] != EMPTY_SLOT) return;  // duplicate
        slots_[pos] = to_slot(idx);
        ++size_;
    }

    int find(SEXP key) const noexcept {
        if (empty()) return -1;
        std::size_t pos = probe_for(key);
        return slots_[pos] == EMPTY_SLOT ? -1 : from_slot(slots_[pos]);
    }

    private:

    // Double capacity and rehash. Entries are unique by construction, so the
    // inner loop only probes for an empty slot — no duplicate check needed.
    void grow() {
        std::size_t new_cap = capacity_ ? capacity_ * 2 : 16;
        auto new_slots = std::make_unique<int[]>(new_cap);
        std::size_t new_mask = new_cap - 1;
        int new_shift = 64 - std::countr_zero(new_cap);

        for (std::size_t i = 0; i < capacity_; ++i) {
            int slot = slots_[i];
            if (slot == EMPTY_SLOT) continue;
            SEXP key = names_ptr_[from_slot(slot)];
            std::size_t pos = static_cast<std::size_t>(sexp_data_hash(key) >> new_shift);
            while (new_slots[pos] != EMPTY_SLOT) {
                pos = next_probe(pos, new_mask);
            }
            new_slots[pos] = slot;
        }
 
        slots_ = std::move(new_slots);
        set_capacity(new_cap);
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

        if (!names.has_value()) return;
        SEXP nms = *names;
        if (nms == R_NilValue) return;

        r_size_t n = Rf_xlength(nms);
        if (n == 0) return;
        if (n > std::numeric_limits<int>::max()) [[unlikely]] {
            abort("Long vector name hashing is not supported");
        }

        const SEXP* p_names = safe[STRING_PTR_RO](nms);
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
