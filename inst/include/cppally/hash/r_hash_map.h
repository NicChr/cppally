#ifndef CPPALLY_R_HASH_MAP_H
#define CPPALLY_R_HASH_MAP_H

#include <cppally/hash/hash.h>
#include <ankerl/unordered_dense.h>
#include <functional>
#include <type_traits>
#include <utility>

// A wrapper hash map around ankerl::unordered_dense::map
// Contains custom members specifically for r_str/r_str_view
// where pointer (CHARSXP) hash/equality is used for duplicate values.
// The first time a new non-ASCII CHARSXP is seen, the text is used for hashing.
// ASCII strings are never marked, so each ASCII text has exactly one CHARSXP and therefore we can always use pointer hashing/equality for those.

namespace cppally {

namespace internal {

template <typename V>
class r_str_map {

    using charsxp_map = ankerl::unordered_dense::map<SEXP, V>;

    // String + text-based hash so that growing the map doesn't re-read text.
    struct text_key {
        SEXP str;
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
            return a.text_hash == b.text_hash && r_hash_eq<r_str_view>{}(a.str, b.str);
        }
    };

    charsxp_map charsxp_lookup;
    ankerl::unordered_dense::map<text_key, V, text_key_hash, text_key_eq> text_lookup;

    // ASCII and NA always have unique CHARSXP, so no need to check text for those ever.
    static bool ascii_or_na(SEXP key) {
        r_str_view s = r_str_view(key, no_checks_tag{});
        return s.is_na() || s.is_ascii();
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
        if (result.second && !ascii_or_na(key)) {
            // A new CHARSXP whose text was already seen counts as not inserted and refers to the existing value
            result.second = try_emplace_text(result.first, key, value);
        }
        return result;
    }

    [[gnu::always_inline]] iterator find(SEXP key) {
        auto it = charsxp_lookup.find(key);
        if (it != charsxp_lookup.end() || text_lookup.empty() || ascii_or_na(key)) {
            return it;
        }
        // A new CHARSXP whose text was already seen is added to `charsxp_lookup`, so repeats are a pointer hit
        return find_text(key);
    }

private:

    CPPALLY_NOINLINE bool try_emplace_text(iterator it, SEXP key, V value) {
        auto [first, inserted] = text_lookup.try_emplace(text_key{key, r_hash_fn<r_str_view>{}(key)}, value);
        if (!inserted) {
            it->second = first->second;
        }
        return inserted;
    }

    CPPALLY_NOINLINE iterator find_text(SEXP key) {
        auto first = text_lookup.find(text_key{key, r_hash_fn<r_str_view>{}(key)});
        if (first == text_lookup.end()) {
            return charsxp_lookup.end();
        }
        return charsxp_lookup.try_emplace(key, first->second).first;
    }
};

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
