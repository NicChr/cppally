#pragma once

#include <csetjmp>
#include <exception>
#include <cpp20/R.hpp>

namespace cpp20 {

// A minimal unwind exception
class unwind_exception : public std::exception {
public:
    SEXP token;
    explicit unwind_exception(SEXP token_) : token(token_) {}
};

// The core unwind_protect (no tuple/gcc4.8 hacks)
template <typename Fun>
auto unwind_protect(Fun&& code) -> decltype(code()) {
    static SEXP token = [] {
        SEXP res = R_MakeUnwindCont();
        R_PreserveObject(res);
        return res;
    }();

    std::jmp_buf jmpbuf;
    if (setjmp(jmpbuf)) {
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
                if (jump == TRUE) longjmp(*static_cast<std::jmp_buf*>(jmpbuf_ptr), 1);
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
                if (jump == TRUE) longjmp(*static_cast<std::jmp_buf*>(jmpbuf_ptr), 1);
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

constexpr protect safe = {};

inline void check_user_interrupt() { safe[R_CheckUserInterrupt](); }

template <typename... Args>
inline void abort [[noreturn]] (const char* msg, Args&&... args) {
    unwind_protect([&] { Rf_errorcall(R_NilValue, msg, std::forward<Args>(args)...); });
    throw std::exception(); // satisfy compiler [[noreturn]]
}
template <typename... Args>
inline void warn(const char* msg, Args&&... args) {
    safe[Rf_warningcall](R_NilValue, msg, std::forward<Args>(args)...);
}

template <typename... Args>
inline void print(const char* msg, Args&&... args) {
    Rprintf(msg, std::forward<Args>(args)...);
}

namespace detail {
namespace store {

inline SEXP init() {
    SEXP out = Rf_cons(R_NilValue, Rf_cons(R_NilValue, R_NilValue));
    R_PreserveObject(out);
    return out;
}

inline SEXP get() {
    // Note the `static` local variable in the inline extern function here! Guarantees we
    // have 1 unique preserve list across all compilation units in the package.
    static SEXP out = init();
    return out;
}

inline R_xlen_t count() {
    const R_xlen_t head = 1;
    const R_xlen_t tail = 1;
    SEXP list = get();
    return Rf_xlength(list) - head - tail;
}

inline SEXP insert(SEXP x) {
    if (x == R_NilValue) {
        return R_NilValue;
    }

    Rf_protect(x);

    SEXP list = get();
    SEXP head = list;
    SEXP next = CDR(list);

    SEXP cell = Rf_protect(Rf_cons(head, next));
    SET_TAG(cell, x);

    SETCDR(head, cell);
    SETCAR(next, cell);

    Rf_unprotect(2);
    return cell;
}

inline void release(SEXP cell) {
    if (cell == R_NilValue) {
        return;
    }
    SEXP lhs = CAR(cell);
    SEXP rhs = CDR(cell);
    SETCDR(lhs, rhs);
    SETCAR(rhs, lhs);
}

inline void print() {
    SEXP list = get();
    for (SEXP cell = list; cell != R_NilValue; cell = CDR(cell)) {
        REprintf("%p CAR: %p CDR: %p TAG: %p\n", reinterpret_cast<void*>(cell),
                    reinterpret_cast<void*>(CAR(cell)), reinterpret_cast<void*>(CDR(cell)),
                    reinterpret_cast<void*>(TAG(cell)));
    }
    REprintf("---\n");
}

} // namespace store
} // namespace detail

} // namespace cpp20
