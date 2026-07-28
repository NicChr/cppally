#include <cppally_light.hpp>
#include <random>   // test-only: for the uniform_random_bit_generator check
#include <cppally/random/random_stream.h>
#include <type_traits>

using namespace cppally;

// ---------------------------------------------------------------------------
// Compile-time contracts. These cost nothing and fail loudly if the API drifts
// ---------------------------------------------------------------------------

static_assert(std::uniform_random_bit_generator<random_stream>,
              "random_stream must satisfy uniform_random_bit_generator so it can "
              "be passed to <random> distributions and std::shuffle");

static_assert(!std::is_copy_constructible_v<random_stream>);
static_assert(!std::is_copy_assignable_v<random_stream>);
static_assert(std::is_move_constructible_v<random_stream>);
static_assert(std::is_move_assignable_v<random_stream>);

// Guards our fork of Xoshiro-cpp: upstream defaults the seed argument, so
// `Xoshiro256PlusPlus g;` compiles there and hands every such object the same
// stream. Fires if the bundled copy is ever re-synced from upstream
static_assert(!std::is_default_constructible_v<random_stream::engine_type>,
              "the engine must never be constructible without a seed");

// ---------------------------------------------------------------------------
// Known-answer support: raw draws as hex strings, so 64-bit values survive the
// trip through R exactly. Compare against the reference implementation
// ---------------------------------------------------------------------------

[[cppally::register]]
r_vec<r_str> test_rng_raw_hex(uint64_t seed, r_size_t n) {
    random_stream rs(seed);
    r_vec<r_str> out(n);
    char buf[17];
    for (r_size_t i = 0; i < n; ++i) {
        std::snprintf(buf, sizeof(buf), "%016llx", static_cast<unsigned long long>(rs()));
        out.set(i, r_str(static_cast<const char*>(buf)));
    }
    return out;
}

// ---------------------------------------------------------------------------
// Lemire. A small range never reaches the rejection loop - for range 7 the
// threshold is 2, so retries happen with probability 2/2^64. Only a range just
// above 2^63 exercises it (threshold ~2^63, so ~half of draws are rejected).
// Returns 16 bucket counts over [0, 2^63 + 1), and aborts if any draw escapes
// ---------------------------------------------------------------------------

[[cppally::register]]
r_vec<r_int> test_rng_lemire_huge(uint64_t seed, r_size_t n) {
    random_stream rs(seed);

    // 2^63, written as one past the largest signed 64-bit value so there are no
    // magic digits to mistype. The range is one more than that: a power of two
    // would never reject, and this is the width that rejects most often
    constexpr uint64_t two_pow_63 = static_cast<uint64_t>(std::numeric_limits<int64_t>::max()) + 1;
    constexpr uint64_t range = two_pow_63 + 1;

    constexpr int n_buckets = 16;
    constexpr uint64_t bucket_width = two_pow_63 / n_buckets;

    r_vec<r_int> out(n_buckets, r_int(0));
    int* p_out = out.data();

    for (r_size_t i = 0; i < n; ++i) {
        uint64_t v = rs.index(uint64_t(0), range - 1);
        if (v >= range) {
            abort("index() returned a value outside [0, range)");
        }

        uint64_t bucket = v / bucket_width;
        if (bucket >= n_buckets) {
            // v was exactly 2^63 - the one value that lands past the last
            // bucket. Fold it in rather than writing off the end
            bucket = n_buckets - 1;
        }
        p_out[bucket]++;
    }
    return out;
}

// Counts for a small range, so the ordinary (no-rejection) path is checked too
[[cppally::register]]
r_vec<r_int> test_rng_bounded_small(uint64_t seed, uint64_t range, r_size_t n) {
    random_stream rs(seed);

    r_vec<r_int> out(static_cast<r_size_t>(range), r_int(0));
    int* p_out = out.data();

    for (r_size_t i = 0; i < n; ++i) {
        uint64_t v = rs.index(uint64_t(0), range - 1);
        if (v >= range) [[unlikely]] {
            abort("index() returned a value outside [0, range)");
        }
        p_out[v]++;
    }
    return out;
}

// ---------------------------------------------------------------------------
// index() and unif()
// ---------------------------------------------------------------------------

[[cppally::register]]
r_vec<r_int> test_rng_index(uint64_t seed, int a, int b, r_size_t n) {
    random_stream rs(seed);
    r_vec<r_int> out(n);
    for (r_size_t i = 0; i < n; ++i) {
        out.set(i, r_int(static_cast<int>(rs.index(a, b))));
    }
    return out;
}

// The full-width case: range wraps to 0 and must take the whole-64-bit branch
[[cppally::register]]
bool test_rng_index_extremes(uint64_t seed) {
    random_stream rs(seed);
    r_size_t lo = std::numeric_limits<r_size_t>::min();
    r_size_t hi = std::numeric_limits<r_size_t>::max();
    for (int i = 0; i < 1000; ++i) {
        rs.index(lo, hi); // must not crash, hang, or divide by zero
    }
    return true;
}

[[cppally::register]]
r_vec<r_dbl> test_rng_unif(uint64_t seed, r_dbl a, r_dbl b, r_size_t n) {
    random_stream rs(seed);
    r_vec<r_dbl> out(n);
    for (r_size_t i = 0; i < n; ++i) {
        out.set(i, rs.unif(a, b));
    }
    return out;
}

// ---------------------------------------------------------------------------
// R integration: the default constructor seeds from R's stream
// ---------------------------------------------------------------------------

[[cppally::register]]
r_vec<r_dbl> test_rng_from_r(r_size_t n) {
    random_stream rs;
    r_vec<r_dbl> out(n);
    for (r_size_t i = 0; i < n; ++i) {
        out.set(i, rs.unif());
    }
    return out;
}

// The seed the default constructor drew, so tests can prove set.seed() reached it
[[cppally::register]]
r_str test_rng_seed_from_r() {
    random_stream rs;
    char buf[17];
    std::snprintf(buf, sizeof(buf), "%016llx", static_cast<unsigned long long>(rs.seed()));
    return r_str(static_cast<const char*>(buf));
}

// Errors after consuming from R's stream. rng_guard must still run PutRNGstate
// while the exception unwinds, so .Random.seed has to change despite the error
[[cppally::register]]
r_sexp test_rng_error_inside_with_rng() {
    return draw_from_r([]() -> r_sexp {
        internal::draw_seed();
        abort("deliberate error inside draw_from_r");
        return r_null;
    });
}
