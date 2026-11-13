#ifndef CPPALLY_R_STR_MAP_H
#define CPPALLY_R_STR_MAP_H

#include <cppally/scalar/hash_scalars.h>
#include <cppally/scalar/relational_ops.h>
#include <ankerl/unordered_dense.h>
#include <cstdint>
#include <functional>
#include <utility>

// A wrapper hash map around ankerl::unordered_dense::map
// Contains custom members specifically for r_str/r_str_view
// where pointer (CHARSXP) hash/equality is used for duplicate values.
// The first time a new non-ASCII CHARSXP is seen, the text is used for hashing.
// ASCII strings are never marked, so each ASCII text has exactly one CHARSXP and therefore we can always use pointer hashing/equality for those.
// try_emplace() + find() - Slower map build, fast lookups later (good for smaller maps, many lookups, or knowing about text duplicates on insert)
// lazy_insert() + lazy_find() - Fast map build, text is indexed once finds need it (good for large maps, not many lookups)

namespace cppally {

namespace internal {

template <typename V>
class r_str_map {

    using charsxp_map = ankerl::unordered_dense::map<SEXP, V>;

    // String + text-based hash so that growing the map doesn't re-read text.
    struct text_key {
        r_str_view str;
        uint64_t text_hash;
    };
    struct text_key_hash {
        using is_avalanching = void;
        uint64_t operator()(const text_key& k) const noexcept {
            return k.text_hash;
        }
    };
    struct text_key_eq {
        bool operator()(const text_key& a, const text_key& b) const {
            return a.text_hash == b.text_hash && string::str_equal(a.str, b.str);
        }
    };

    charsxp_map charsxp_lookup;
    ankerl::unordered_dense::map<text_key, V, text_key_hash, text_key_eq> text_lookup;
    std::size_t n_unindexed = 0; // The last n_unindexed entries (from lazy_insert) aren't in `text_lookup` yet
    std::size_t n_key_checks = 0; // Finds since then that checked their own key instead

    // ASCII and NA always have unique CHARSXP, so no need to check text for those ever.
    static bool ascii_or_na(r_str_view key) {
        return key.is_na() || key.is_ascii();
    }

    static uint64_t text_hash(r_str_view key) {
        return r_hash_impl(key);
    }

public:

    using iterator = typename charsxp_map::iterator;

    void reserve(std::size_t n) {
        charsxp_lookup.reserve(n);
    }

    iterator end() noexcept {
        return charsxp_lookup.end();
    }

    // Pointer equality
    std::equal_to<SEXP> key_eq() const noexcept {
        return {};
    }

    [[gnu::always_inline]] std::pair<iterator, bool> try_emplace(SEXP key, V value) {
        auto result = charsxp_lookup.try_emplace(key, value);
        r_str_view str(key, no_checks_tag{});
        if (result.second && !ascii_or_na(str)) {
            // A new CHARSXP whose text was already seen counts as not inserted and refers to the existing value
            result.second = try_emplace_text(result.first, str, value);
        }
        return result;
    }

    [[gnu::always_inline]] iterator find(SEXP key) {
        auto it = charsxp_lookup.find(key);
        r_str_view str(key, no_checks_tag{});
        if (it != charsxp_lookup.end() || text_lookup.empty() || ascii_or_na(str)) {
            return it;
        }
        // A new CHARSXP whose text was already seen is added to `charsxp_lookup`, so repeats are a pointer hit
        return find_text(str);
    }

    // Pointer-only insert (first CHARSXP wins), text duplicates are resolved once lazy_find() needs them
    [[gnu::always_inline]] void lazy_insert(SEXP key, V value) {
        n_unindexed += charsxp_lookup.try_emplace(key, value).second;
    }

    [[gnu::always_inline]] iterator lazy_find(SEXP key) {
        if (n_unindexed != 0) [[unlikely]] {
            // Non-ASCII keys need the text index, and ASCII ones switch to it once checking keys has cost as much (rent-or-buy)
            if (!ascii_or_na(r_str_view(key, no_checks_tag{})) || ++n_key_checks >= n_unindexed) {
                index_text();
            }
        }
        return find(key);
    }

private:

    CPPALLY_NOINLINE bool try_emplace_text(iterator it, r_str_view key, V value) {
        auto [first, inserted] = text_lookup.try_emplace(text_key{key, text_hash(key)}, value);
        if (!inserted) {
            it->second = first->second;
        }
        return inserted;
    }

    CPPALLY_NOINLINE iterator find_text(r_str_view key) {
        auto first = text_lookup.find(text_key{key, text_hash(key)});
        if (first == text_lookup.end()) {
            return charsxp_lookup.end();
        }
        return charsxp_lookup.try_emplace(unwrap(key), first->second).first;
    }

    // Index the unindexed entries' text in insertion order, so the first of any text duplicates wins
    CPPALLY_NOINLINE void index_text() {
        auto it = charsxp_lookup.end() - static_cast<std::ptrdiff_t>(n_unindexed);
        for (; it != charsxp_lookup.end(); ++it) {
            r_str_view str(it->first, no_checks_tag{});
            if (!ascii_or_na(str)) {
                try_emplace_text(it, str, it->second);
            }
        }
        n_unindexed = 0;
        n_key_checks = 0;
    }
};

}

}

#endif
