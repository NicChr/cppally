#include <cppally_light.hpp>
#include <vector>
#include <algorithm>

using namespace cppally;

namespace vs = cppally::internal::vec_store;


static void check_true(bool ok, const char* what) {
    if (!ok) {
        abort("vec_store test: %s", what);
    }
}

static void check_eq(long long got, long long expected, const char* what) {
    if (got != expected) {
        abort("vec_store test: %s (expected %lld, got %lld)", what, expected, got);
    }
}

static int pool_chunks() {
    int n = 0;
    for (vs::chunk* c = vs::head_chunk(); c != nullptr; c = c->next) {
        ++n;
    }
    return n;
}

static long long pool_capacity() {
    long long n = 0;
    for (vs::chunk* c = vs::head_chunk(); c != nullptr; c = c->next) {
        n += c->capacity;
    }
    return n;
}

static long long pool_free() {
    return pool_capacity() - static_cast<long long>(vs::count());
}

// Structural invariants that must hold after every insert/release, whatever
// the pool's history
static void check_pool_invariants() {
    long long reserved_total = 0;

    for (vs::chunk* c = vs::head_chunk(); c != nullptr; c = c->next) {
        if (c->prev == nullptr) {
            check_true(vs::head_chunk() == c, "chunk with no prev must be the master head");
        } else {
            check_true(c->prev->next == c, "master chain back-links consistent");
        }

        check_true(c->capacity >= 1024 && c->capacity <= 16384, "chunk capacity within bounds");
        check_true(c->capacity <= vs::watermark_size(), "chunk capacity never exceeds the watermark");
        check_true(c->free_count >= 0 && c->free_count <= c->capacity, "free_count within bounds");

        if (c->reserved) {
            check_true(c->free_count == c->capacity, "reserved chunk must be empty");
            reserved_total += c->capacity;
        }
        if (c->free_count == c->capacity && !c->reserved) {
            check_true(c->prev == nullptr && c->next == nullptr,
                       "an empty unreserved chunk can only be the sole scratchpad");
        }

        // free_stack holds each free slot exactly once; free slots are cleared
        // to R_NilValue, occupied slots are not
        std::vector<bool> is_free(static_cast<size_t>(c->capacity), false);
        for (int i = 0; i < c->free_count; ++i) {
            int idx = c->free_stack[i];
            check_true(idx >= 0 && idx < c->capacity, "free_stack index in range");
            check_true(!is_free[static_cast<size_t>(idx)], "free_stack indices distinct");
            is_free[static_cast<size_t>(idx)] = true;
        }
        for (int i = 0; i < c->capacity; ++i) {
            SEXP slot = VECTOR_ELT(c->vec, i);
            if (is_free[static_cast<size_t>(i)]) {
                check_true(slot == R_NilValue, "free slot cleared to R_NilValue");
            } else {
                check_true(slot != R_NilValue, "occupied slot holds a real SEXP");
            }
        }

        bool on_free_list = false;
        for (vs::chunk* f = vs::free_list_head(); f != nullptr; f = f->free_next) {
            if (f == c) {
                on_free_list = true;
                break;
            }
        }
        check_true(on_free_list == (c->free_count > 0), "free-list membership iff free_count > 0");
    }

    check_eq(reserved_total, vs::reserved_slots(), "reserved_slots matches reserved chunk capacities");
    check_true(reserved_total <= 16384, "reserve within its slot budget");

    if (vs::free_list_head() != nullptr) {
        check_true(vs::free_list_head()->free_prev == nullptr, "free-list head has no back-link");
    }
    for (vs::chunk* f = vs::free_list_head(); f != nullptr; f = f->free_next) {
        check_true(f->free_count > 0, "only chunks with free slots sit on the free list");
        if (f->free_next != nullptr) {
            check_true(f->free_next->free_prev == f, "free-list back-links consistent");
        }
    }
}


// count() follows wrapper lifetimes exactly: one slot per owning wrapper,
// copies share via refcount, moves transfer, views and NULL take nothing
[[cppally::register]]
bool test_protect_count_tracking() {
    const long long baseline = vs::count();

    {
        r_sexp a(safe[Rf_allocVector](INTSXP, 1));
        check_eq(vs::count(), baseline + 1, "one wrapper takes one slot");

        r_sexp b = a;
        check_eq(vs::count(), baseline + 1, "copy shares the slot via refcount");

        r_sexp c(static_cast<SEXP>(a));
        check_eq(vs::count(), baseline + 2, "re-wrapping the raw SEXP takes a fresh slot");

        r_sexp d = std::move(b);
        check_eq(vs::count(), baseline + 2, "move transfers the slot");
    }
    check_eq(vs::count(), baseline, "all slots returned at scope exit");

    {
        r_sexp survivor;
        {
            r_sexp a(safe[Rf_allocVector](INTSXP, 1));
            survivor = a;
        }
        check_eq(vs::count(), baseline + 1, "slot outlives the original through the copy");
    }
    check_eq(vs::count(), baseline, "slot released when the last owner dies");

    {
        r_sexp null_wrapper(R_NilValue);
        check_eq(vs::count(), baseline, "NULL wrapper takes no slot");

        r_sexp owner(safe[Rf_allocVector](INTSXP, 1));
        r_sexp view(static_cast<SEXP>(owner), internal::view_tag{});
        check_eq(vs::count(), baseline + 1, "view takes no slot");
    }
    check_eq(vs::count(), baseline, "count restored");

    check_pool_invariants();
    return true;
}


// Rapid create/destroy of a single object must ping-pong on one slot with no
// chunk allocation or destruction (the sole-chunk scratchpad rule)
[[cppally::register]]
bool test_protect_slot_reuse() {
    r_sexp keeper(safe[Rf_allocVector](INTSXP, 1));

    const long long baseline   = vs::count();
    const int       chunks     = pool_chunks();
    const long long capacity   = pool_capacity();

    for (int i = 0; i < 100000; ++i) {
        r_sexp a(static_cast<SEXP>(keeper));
        if (vs::count() != baseline + 1) {
            abort("vec_store test: ping-pong count drifted at iteration %d", i);
        }
    }

    check_eq(vs::count(), baseline, "count restored after ping-pong");
    check_eq(pool_chunks(), chunks, "no chunk churn during ping-pong");
    check_eq(pool_capacity(), capacity, "no capacity churn during ping-pong");
    check_pool_invariants();
    return true;
}


// Filling every free slot allocates nothing; one insert beyond that adds
// exactly one chunk of double the previous size (capped at 16384)
[[cppally::register]]
bool test_protect_chunk_growth() {
    r_sexp keeper(safe[Rf_allocVector](INTSXP, 1));

    const long long baseline     = vs::count();
    const int       chunks       = pool_chunks();
    const int       expected_cap = std::min(vs::watermark_size() * 2, 16384);
    const long long free_slots   = pool_free();

    std::vector<r_sexp> objs;
    objs.reserve(static_cast<size_t>(free_slots) + 1);
    for (long long i = 0; i < free_slots; ++i) {
        objs.emplace_back(static_cast<SEXP>(keeper));
    }

    check_eq(pool_chunks(), chunks, "filling existing free slots allocates nothing");
    check_true(vs::free_list_head() == nullptr, "free list empty once every slot is taken");
    check_eq(vs::count(), baseline + free_slots, "count tracks the fill exactly");

    objs.emplace_back(static_cast<SEXP>(keeper));
    check_eq(pool_chunks(), chunks + 1, "one insert past capacity adds exactly one chunk");
    check_eq(vs::head_chunk()->capacity, expected_cap, "new chunk doubles the last size, capped");
    check_eq(vs::watermark_size(), expected_cap, "watermark moves to the new capacity");
    check_pool_invariants();

    objs.clear();
    check_eq(vs::count(), baseline, "count restored after releasing the burst");
    check_pool_invariants();
    return true;
}


// A burst big enough to drive the watermark to the cap, then full release:
// the pool must collapse to exactly one reserved 16384-slot chunk, which the
// next insert reuses without any R-side allocation
[[cppally::register]]
bool test_protect_burst_reserve() {
    const long long entry = vs::count();
    // The exact end-state assertions need an otherwise-idle pool; with live
    // foreign slots we still run the burst and the structural invariants
    const bool clean = (entry == 0);

    {
        r_sexp keeper(safe[Rf_allocVector](INTSXP, 1));

        // Past all current free slots, 16000 extra guarantees the allocation
        // sequence reaches a 16384 chunk (worst case adds 2048+4096+8192 = 14336
        // smaller slots first)
        const long long n = pool_free() + 16000;

        std::vector<r_sexp> objs;
        objs.reserve(static_cast<size_t>(n));
        for (long long i = 0; i < n; ++i) {
            objs.emplace_back(static_cast<SEXP>(keeper));
        }

        check_eq(vs::count(), entry + 1 + n, "count tracks the whole burst");
        check_eq(vs::watermark_size(), 16384, "burst drives the watermark to the cap");
        check_eq(vs::head_chunk()->capacity, 16384, "newest chunk is cap-sized");
        check_true(pool_chunks() >= 2, "burst spans multiple chunks");
        check_pool_invariants();

        objs.clear();
        check_eq(vs::count(), entry + 1, "count restored up to the keeper");
        check_pool_invariants();
    }

    check_eq(vs::count(), entry, "count fully restored");
    check_pool_invariants();

    if (clean) {
        check_eq(pool_chunks(), 1, "quiet pool retains exactly one chunk");
        check_eq(vs::head_chunk()->capacity, 16384, "retained chunk is watermark-sized");
        check_true(vs::head_chunk()->reserved, "retained chunk sits in the reserve");
        check_eq(vs::reserved_slots(), 16384, "reserve accounting matches");

        {
            r_sexp probe(safe[Rf_allocVector](INTSXP, 1));
            check_eq(pool_chunks(), 1, "probe reuses the reserved chunk, no allocation");
            check_true(!vs::head_chunk()->reserved, "chunk leaves the reserve when used");
            check_eq(vs::reserved_slots(), 0, "reserve accounting drops on reuse");
        }

        check_eq(pool_chunks(), 1, "sole chunk kept as scratchpad after the probe dies");
        check_eq(vs::reserved_slots(), 0, "sole scratchpad is not re-reserved");
        check_pool_invariants();
    }

    return true;
}
