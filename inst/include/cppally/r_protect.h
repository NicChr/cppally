#ifndef CPPALLY_R_PROTECT_H
#define CPPALLY_R_PROTECT_H

#include <cppally/r_setup.h>
#include <type_traits>
#include <utility>
#include <csetjmp>
#include <cstdint> // For fixed-width int types
#include <exception>


namespace cppally {


namespace internal {

// A minimal unwind exception
class unwind_exception : public std::exception {
public:
    SEXP token;
    explicit unwind_exception(SEXP token_) : token(token_) {}
};

// One shared unwind continuation for the whole shared library.
inline SEXP unwind_token() {
    static SEXP token = [] {
        SEXP res = R_MakeUnwindCont();
        R_PreserveObject(res);
        return res;
    }();
    return token;
}

// The core unwind_protect (no tuple/gcc4.8 hacks)
template <typename Fun>
auto unwind_protect(Fun&& code) -> decltype(code()) {
    SEXP token = unwind_token();

    std::jmp_buf jmpbuf;
    if (setjmp(jmpbuf)) [[unlikely]] {
        throw unwind_exception(token);
    }

    // Capture the return type (SEXP or void)
    using ReturnType = decltype(code());

    static_assert(std::is_same_v<ReturnType, SEXP> || std::is_same_v<ReturnType, void>,
        "unwind_protect only supports returning SEXP or void");
    
    if constexpr (std::is_same_v<ReturnType, SEXP>) {
        SEXP res = R_UnwindProtect(
            [](void* data) -> SEXP {
                return (*static_cast<std::decay_t<Fun>*>(data))();
            },
            &code,
            [](void* jmpbuf_ptr, Rboolean jump) {
                if (jump == TRUE) [[unlikely]] longjmp(*static_cast<std::jmp_buf*>(jmpbuf_ptr), 1);
            },
            &jmpbuf, token);
        SETCAR(token, R_NilValue);
        return res;
    } else {
        R_UnwindProtect(
            [](void* data) -> SEXP {
                (*static_cast<std::decay_t<Fun>*>(data))();
                return R_NilValue;
            },
            &code,
            [](void* jmpbuf_ptr, Rboolean jump) {
                if (jump == TRUE) [[unlikely]] longjmp(*static_cast<std::jmp_buf*>(jmpbuf_ptr), 1);
            },
            &jmpbuf, token);
        SETCAR(token, R_NilValue);
    }
}

// The `safe` syntax wrapper (C++20 lambda style, no complex structs)
struct protect {
    template <typename F>
    constexpr auto operator[](F* raw_func) const {
        return [raw_func](auto&&... args) -> decltype(raw_func(std::forward<decltype(args)>(args)...)) {
            return unwind_protect([&] {
                return raw_func(std::forward<decltype(args)>(args)...);
            });
        };
    }
};


// Forwarding non-trivially-copyable C++ objects through a C vararg function
// (e.g. Rf_errorcall / Rf_warningcall) is undefined behaviour. These helpers
// are printf-style; only POD types and `const char*` are valid. The static
// assert catches accidental `std::string` etc at compile time -- use .c_str().
template <typename... Args>
inline constexpr bool all_vararg_safe_v =
    (std::is_trivially_copyable_v<std::decay_t<Args>> && ...);

}

constexpr internal::protect safe = {};

inline void check_user_interrupt() { safe[R_CheckUserInterrupt](); }
template <typename... Args>
[[noreturn]] inline void abort (const char* msg, Args&&... args) {
    static_assert(internal::all_vararg_safe_v<Args...>,
        "abort() forwards args into a C vararg function; all args must be "
        "trivially copyable (use .c_str() for std::string)");
    internal::unwind_protect([&] { Rf_errorcall(R_NilValue, msg, std::forward<Args>(args)...); });
    throw std::exception(); // satisfy compiler [[noreturn]]
}

[[noreturn]] inline void abort(const char* msg) {
    internal::unwind_protect([&] { Rf_errorcall(R_NilValue, "%s", msg); });
    throw std::exception();
}

template <typename... Args>
inline void warn(const char* msg, Args&&... args) {
    static_assert(internal::all_vararg_safe_v<Args...>,
        "warn() forwards args into a C vararg function; all args must be "
        "trivially copyable (use .c_str() for std::string)");
    safe[Rf_warningcall](R_NilValue, msg, std::forward<Args>(args)...);
}

inline void warn(const char* msg) {
    safe[Rf_warningcall](R_NilValue, "%s", msg);
}

template <typename... Args>
inline void print(const char* msg, Args&&... args) {
    static_assert(internal::all_vararg_safe_v<Args...>,
        "print() forwards args into a C vararg function; all args must be "
        "trivially copyable (use .c_str() for std::string)");
    Rprintf(msg, std::forward<Args>(args)...);
}

inline void print(const char* msg) {
    Rprintf("%s", msg);
}


namespace internal {
namespace vec_store {

// ---------------------------------------------------------------------------
// Chunked VECSXP-backed protection pool
//
// The pool is a singly-linked chain of "chunks". Each chunk is one VECSXP
// (independently R_PreserveObject'd) plus a small C++ control block:
//
//   chunk { vec=VECSXP[capacity], capacity, free_count, free_stack[capacity],
//           next, free_next }
//
// Slot lifetime within a chunk is tracked by a per-chunk LIFO freelist of
// integer slot indices (`free_stack`). Insert pops the next free slot and
// writes the protected SEXP via SET_VECTOR_ELT (write-barrier safe). Release
// clears the slot back to R_NilValue and pushes the index onto free_stack.
//
// Two intrusive lists are threaded through `chunk`, both doubly linked so
// every link/unlink is O(1):
//   * `next`/`prev`           -- master allocation chain (every chunk ever
//                                created). Used for diagnostics (count/print)
//                                and for O(1) unlink in destroy_chunk.
//   * `free_next`/`free_prev` -- the "has free slots" list. A chunk is on
//                                this list iff its free_count > 0. Insert
//                                and release are O(1) on every path, hot or
//                                cold -- there is no chain walk anywhere.
//                                When a chunk fills, it is unlinked from the
//                                free list; when a previously-full chunk has
//                                a slot released, it is pushed back on.
//
// New chunks double in capacity (256, 512, 1024, ...) up to `max_chunk_size`.
// Crucially, growing the pool means *appending* a new chunk -- existing
// chunks are never copied or moved, so all outstanding `slot_ref` tokens
// remain valid forever
//
// Memory behaviour:
//   * The pool tracks the actual current working set, not the historical
//     peak. When a chunk becomes entirely empty (free_count == capacity)
//     it is R_ReleaseObject'd and freed in `release()`, with one exception:
//     we always keep at least one chunk alive as a scratchpad to avoid
//     allocate-then-free thrashing at burst boundaries.
//   * This matters because R's GC marks every slot of a VECSXP up to its
//     length, not just the populated ones -- a 16384-slot chunk holding
//     three live objects still costs ~16384 pointer reads per GC cycle.
//     Releasing empty chunks keeps GC scan cost proportional to actual
//     working-set size.
//   * `next_size` (chunk capacity for the next allocation) is watermark-
//     only: it never shrinks. Once it reaches max_chunk_size all subsequent
//     chunks are allocated at that size. The reasoning: if we needed a big
//     chunk once, we'll likely need one again, and over-allocating beats
//     repeated doubling on every burst.
//   * `protect_cell` C++ control blocks are recycled via a small freelist
//     and never released back to the heap. Bounded by peak concurrent
//     refcount-distinct r_sexp count, which is small in practice.
//
// Threading:
//   * Not thread-safe. All state (head_chunk, free_list_head, the per-chunk
//     free_stack and free_count) is shared process-global with no locking.
//     This matches R itself, which is single-threaded -- the R API is not
//     reentrant from parallel threads. **Do not construct r_sexp / r_str /
//     r_vec from inside an OpenMP parallel region.** Read-only access via
//     view_tag or r_str_view is safe because it never touches the pool.
//
// Trade-offs vs the previous cons-cell linked list:
//   + 1 R-side allocation per *chunk* instead of 1 per cell -- far fewer
//     calls into R's allocator and far fewer GC roots.
//   + Linear memory layout: chunk slots are contiguous, so the hot path
//     touches one cache line for many consecutive inserts/releases.
//   + Slightly larger token (chunk* + int instead of a single SEXP), but
//     refcount means most copies don't allocate a token at all.
//   - Per-chunk freelist introduces a tiny C++-side allocation (`new int[]`),
//     amortised across the entire chunk.
// ---------------------------------------------------------------------------

struct chunk {
    SEXP   vec;         // VECSXP, R_PreserveObject'd
    int    capacity;
    int    free_count;  // slots currently free in this chunk
    int*   free_stack;  // [capacity] indices of free slots (LIFO)
    chunk* next;        // master allocation chain (forward link)
    chunk* prev;        // master allocation chain (back link, for O(1) unlink)
    chunk* free_next;   // "has free slots" list, forward; nullptr if full
    chunk* free_prev;   // "has free slots" list, back; nullptr if full or head
};

// Token returned by insert; opaque to callers other than `release`.
struct slot_ref {
    chunk* c;     // owning chunk (nullptr means "no slot / R_NilValue")
    int    slot;  // index within c->vec
};

// Singletons (one per shared library via `static` in inline functions).
//   head_chunk     -- master chain of every chunk ever allocated
//   free_list_head -- head of the intrusive "has free slots" list
inline chunk*& head_chunk()     { static chunk* head = nullptr; return head; }
inline chunk*& free_list_head() { static chunk* head = nullptr; return head; }

// Allocate a new chunk and push it onto both the master chain and the
// free list. Capacity doubles each time, capped at `max_chunk_size`. Caller
// is responsible for protecting any SEXPs that must outlive this allocation.
inline chunk* add_chunk() {
    // Start at 1024 slots: skips two doublings of warmup so the first
    // ~thousand protections in any .Call don't trigger any growth, while
    // staying small enough that GC scan cost stays trivial when the chunk
    // is sparsely populated)
    constexpr int first_chunk_size = 1024;
    constexpr int max_chunk_size   = 16384;
    static int next_size = first_chunk_size;

    int cap = next_size;
    SEXP v = safe[Rf_allocVector](VECSXP, cap);
    R_PreserveObject(v);

    chunk* c = new chunk{};
    c->vec        = v;
    c->capacity   = cap;
    c->free_count = cap;
    c->free_stack = new int[cap];
    // Fill the freelist so slot 0 is popped first (LIFO).
    for (int i = 0; i < cap; ++i) {
        c->free_stack[i] = cap - 1 - i;
    }

    // Link into master chain (doubly-linked for O(1) unlink in release).
    c->prev = nullptr;
    c->next = head_chunk();
    if (c->next != nullptr) {
        c->next->prev = c;
    }
    head_chunk() = c;

    // Link into "has free slots" list (it's empty, so it definitely has free).
    c->free_prev = nullptr;
    c->free_next = free_list_head();
    if (c->free_next != nullptr) {
        c->free_next->free_prev = c;
    }
    free_list_head() = c;

    if (next_size < max_chunk_size) {
        next_size *= 2;
    }
    return c;
}

// Permanently free a chunk: unlink from both lists, release the VECSXP back
// to R, and delete the C++ control block. Caller must guarantee `c` has no
// live slots, that it is on the free list (free_count > 0), and that it is
// not the only chunk in existence.
inline void destroy_chunk(chunk* c) noexcept {
    // Unlink from master chain (doubly linked).
    if (c->prev != nullptr) {
        c->prev->next = c->next;
    } else {
        head_chunk() = c->next;
    }
    if (c->next != nullptr) {
        c->next->prev = c->prev;
    }

    // Unlink from free list (also doubly linked -- O(1)).
    if (c->free_prev != nullptr) {
        c->free_prev->free_next = c->free_next;
    } else {
        free_list_head() = c->free_next;
    }
    if (c->free_next != nullptr) {
        c->free_next->free_prev = c->free_prev;
    }

    R_ReleaseObject(c->vec);
    delete[] c->free_stack;
    delete c;
}

inline slot_ref insert(SEXP x) {
    if (x == R_NilValue) {
        return {nullptr, -1};
    }

    chunk* c = free_list_head();
    if (c == nullptr) [[unlikely]] {
        // Every chunk is full (or none exist). Allocate a new one.
        // Allocation may GC, so PROTECT x. This cold path runs only when
        // the pool needs a new chunk — effectively never after warmup.
        Rf_protect(x);
        c = add_chunk();
        Rf_unprotect(1);
    }

    int slot = c->free_stack[--c->free_count];
    SET_VECTOR_ELT(c->vec, slot, x);

    // If this chunk just became full, unlink it from the free list.
    if (c->free_count == 0) [[unlikely]] {
        free_list_head() = c->free_next;
        if (c->free_next != nullptr) {
            c->free_next->free_prev = nullptr;
        }
        c->free_next = nullptr;
        c->free_prev = nullptr;
    }

    return {c, slot};
}

inline void release(slot_ref ref) noexcept {
    if (ref.c == nullptr) {
        return;
    }
    chunk* c = ref.c;
    SET_VECTOR_ELT(c->vec, ref.slot, R_NilValue);

    bool was_full = (c->free_count == 0);
    c->free_stack[c->free_count++] = ref.slot;

    // If the chunk was full, it wasn't on the free list -- put it back.
    if (was_full) [[unlikely]] {
        c->free_prev = nullptr;
        c->free_next = free_list_head();
        if (c->free_next != nullptr) {
            c->free_next->free_prev = c;
        }
        free_list_head() = c;
    }

    // If the chunk is now entirely empty, free it back to R -- but always
    // keep at least one chunk alive as a scratchpad. Without the guard, a
    // workload that protects exactly one object at a time and releases it
    // immediately would allocate-then-free a chunk on every cycle.
    if (c->free_count == c->capacity && head_chunk() != nullptr &&
        head_chunk()->next != nullptr) [[unlikely]] {
        destroy_chunk(c);
    }
}

inline r_size_t count() {
    r_size_t n = 0;
    for (chunk* c = head_chunk(); c != nullptr; c = c->next) {
        n += (c->capacity - c->free_count);
    }
    return n;
}

inline void print() {
    REprintf("vec_store:\n");
    int idx = 0;
    for (chunk* c = head_chunk(); c != nullptr; c = c->next, ++idx) {
        REprintf("  chunk %d: vec=%p capacity=%d in_use=%d\n",
                 idx, reinterpret_cast<void*>(c->vec),
                 c->capacity, c->capacity - c->free_count);
    }
    REprintf("---\n");
}

} // namespace vec_store

// ---------------------------------------------------------------------------
// Refcounted protection tokens.
//
// A `protect_cell` is a tiny C++ control block that owns one slot in the
// chunked VECSXP pool above. Multiple r_sexp wrappers can share the same
// protect_cell via refcounting -- copy construction just bumps `refs`
// instead of taking a new pool slot.
//
// `protect_cell` instances themselves are recycled from a C++-side freelist,
// so in the steady state there is zero heap allocation on either the R side
// (slots come from an existing chunk) or the C++ side.
// ---------------------------------------------------------------------------

namespace refcount {

struct protect_cell {
    vec_store::slot_ref token; // slot in the chunked VECSXP pool
    std::uint32_t       refs;  // how many r_sexps point at this control block
    protect_cell*       next;  // freelist link (only valid while on freelist)
};

// C++-side freelist of recycled protect_cell instances. Not thread-safe,
// but R itself is single-threaded.
inline protect_cell*& free_list() {
    static protect_cell* head = nullptr;
    return head;
}

inline protect_cell* alloc_cell() {
    protect_cell*& head = free_list();
    if (head != nullptr) {
        protect_cell* p = head;
        head = p->next;
        return p;
    }
    return new protect_cell{};
}

inline void free_cell(protect_cell* p) {
    protect_cell*& head = free_list();
    p->next = head;
    head = p;
}

// Create a new protection token for `x`. Returns nullptr for R_NilValue.
//
// Order matters: do the R-side work *first*. vec_store::insert may throw an
// unwind_exception from a failed Rf_allocVector inside add_chunk. If we
// allocated the C++ control block first, that throw would leak it -- the
// cell is neither attached to anything nor returned to the freelist.
inline protect_cell* insert(SEXP x) {
    if (x == R_NilValue) {
        return nullptr;
    }
    auto token = vec_store::insert(x); // may throw -- nothing to clean up yet
    protect_cell* p = alloc_cell();
    p->token = token;
    p->refs  = 1;
    p->next  = nullptr;
    return p;
}

// Bump the refcount. Safe to call with nullptr.
inline void incref(protect_cell* p) noexcept {
    if (p != nullptr) {
        ++p->refs;
    }
}

// Drop a reference. When the count hits zero, release the pool slot and
// recycle the control block.
inline void decref(protect_cell* p) noexcept {
    if (p != nullptr && --p->refs == 0) {
        vec_store::release(p->token);
        free_cell(p);
    }
}

} // namespace refcount
} // namespace internal

} // namespace cppally

#endif 
