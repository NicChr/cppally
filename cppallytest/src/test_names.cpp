#include <cppally.hpp>
#include <string>

using namespace cppally;

// Two wrappers around the same SEXP see each other's name changes
[[cppally::register]]
r_vec<r_str_view> test_names_inplace_mutation() {
    r_vec<r_int> a(1);
    a.set_names(make_vec<r_str>("a"));
    r_vec<r_int> b = a;
    b.set_names(make_vec<r_str>("b"));
    return a.names();
}

// A pre-built hash must be invalidated by set_names
[[cppally::register]]
r_lgl test_names_stale_invalidation() {
    r_vec<r_int> a(2);
    a.set_names(make_vec<r_str>("x", "y"));
    (void) a.name_index("x");
    (void) a.name_index("x");           // force hash build (second access)

    a.set_names(make_vec<r_str>("p", "q"));

    r_int old_idx = a.name_index("x", /*abort_on_missing=*/false);
    r_int new_idx = a.name_index("p");
    return r_lgl(is_na(old_idx) && unwrap(new_idx) == 0);
}

// Routing names through the generic attr::set_attr path must also invalidate the cache
[[cppally::register]]
r_lgl test_names_set_attr_invalidation() {
    r_vec<r_int> a(2);
    a.set_names(make_vec<r_str>("x", "y"));
    (void) a.name_index("x");
    (void) a.name_index("x");

    attr::set_attr(a, symbol::names_sym, make_vec<r_str>("u", "v"));

    r_int old_idx = a.name_index("x", false);
    r_int new_idx = a.name_index("u");
    return r_lgl(is_na(old_idx) && unwrap(new_idx) == 0);
}

// Hash table grows past the initial reserved capacity. With 200 entries grow() must fire multiple times. Every name must remain findable.
[[cppally::register]]
r_lgl test_names_growing() {
    constexpr int N = 200;
    r_vec<r_str> names_vec(N);
    for (int i = 0; i < N; ++i) {
        names_vec.set(i, r_str(("name" + std::to_string(i)).c_str()));
    }
    r_vec<r_int> v(N);
    v.set_names(names_vec);

    (void) v.name_index("name0");
    (void) v.name_index("name0");      // build hash

    for (int i = 0; i < N; ++i) {
        r_int idx = v.name_index(r_str(("name" + std::to_string(i)).c_str()), false);
        if (is_na(idx) || unwrap(idx) != i) return r_lgl(false);
    }

    // A non-existent name must miss cleanly.
    r_int missing = v.name_index("nope", false);
    return r_lgl(is_na(missing));
}

// Sweeping. SWEEP_INTERVAL is 1024; cycling through far more than that must
//    not crash, leak unbounded, or break the registry. We can't observe the
//    sweep directly from here — the contract we assert is "still works after
//    all the churn"
[[cppally::register]]
r_lgl test_names_sweep() {
    for (int i = 0; i < 5000; ++i) {
        r_vec<r_int> v(1);
        v.set_names(make_vec<r_str>("x"));
        (void) v.name_index("x");
        (void) v.name_index("x");
    }
    r_vec<r_int> v(1);
    v.set_names(make_vec<r_str>("final"));
    r_int idx = v.name_index("final");
    return r_lgl(unwrap(idx) == 0);
}

// shallow_copy creates an independent SEXP
[[cppally::register]]
r_lgl test_names_shallow_copy_isolation() {
    r_vec<r_int> a(2);
    a.set_names(make_vec<r_str>("a1", "a2"));
    (void) a.name_index("a1");
    (void) a.name_index("a1");          // build hash on a

    r_vec<r_int> b = shallow_copy(a);   // independent SEXP, shared inner table
    b.set_names(make_vec<r_str>("b1", "b2"));

    r_int a_old   = a.name_index("a1", false);
    r_int b_new   = b.name_index("b1", false);
    r_int a_xover = a.name_index("b1", false);
    r_int b_xover = b.name_index("a1", false);

    return r_lgl(
        !is_na(a_old)   && unwrap(a_old) == 0 &&
        !is_na(b_new)   && unwrap(b_new) == 0 &&
        is_na(a_xover) &&
        is_na(b_xover)
    );
}

// Empty names vector - exercises the lazy_build n == 0 early-return
[[cppally::register]]
r_lgl test_names_empty() {
    r_vec<r_int> v = r_vec<r_int>();
    v.set_names(r_vec<r_str>());
    (void) v.name_index("nothing", false);   // linear scan path
    r_int idx = v.name_index("nothing", false); // hash path
    return r_lgl(is_na(idx));
}

// Building the hash then re-looking up every entry — exercises the
//    post-grow probe path repeatedly. Catches load-factor / probe bugs that
//    would manifest as wrong indices or "infinite" probe loops
[[cppally::register]]
r_lgl test_names_roundtrip_after_grow() {
    constexpr int N = 64;
    r_vec<r_str> names_vec(N);
    for (int i = 0; i < N; ++i) {
        names_vec.set(i, r_str(("k" + std::to_string(i)).c_str()));
    }
    r_vec<r_int> v(N);
    v.set_names(names_vec);

    (void) v.name_index("k0");
    (void) v.name_index("k0");          // build hash, growth-prone path

    // Hit every name twice — second pass exercises the grown table without
    // any further mutation.
    for (int pass = 0; pass < 2; ++pass) {
        for (int i = 0; i < N; ++i) {
            r_int idx = v.name_index(r_str(("k" + std::to_string(i)).c_str()), false);
            if (is_na(idx) || unwrap(idx) != i) return r_lgl(false);
        }
    }
    return r_lgl(true);
}

