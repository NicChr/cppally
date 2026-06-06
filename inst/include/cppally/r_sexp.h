#ifndef CPPALLY_R_SEXP_H
#define CPPALLY_R_SEXP_H

#include <cppally/r_setup.h>
#include <cppally/r_concepts.h>
#include <cppally/r_protect.h>

namespace cppally {

// General SEXP, automatic protection handled via cppally-managed vector list
//
// The SEXP protection mechanism here is inspired by cpp11::sexp
//   cpp11: https://github.com/r-lib/cpp11
//   Copyright (c) 2020 Posit Software, PBC
//   Licensed under the MIT License <https://opensource.org/licenses/MIT>
//

struct r_sexp {

  public:

  SEXP value = R_NilValue;
  using value_type = SEXP;

  private:

  // Refcounted protection token. nullptr means "view mode" (no protection).
  // Copy construction bumps `ctl_->refs` instead of allocating a new cons cell,
  // so passing r_sexp around by value is essentially free
  internal::refcount::protect_cell* ctl_ = nullptr;

  public:

  r_sexp() = default;
  explicit r_sexp(SEXP data) : value(data), ctl_(internal::refcount::insert(data)) {}

  // Copy = refcount bump (no R API involvement)
  r_sexp(const r_sexp& rhs) noexcept : value(rhs.value), ctl_(rhs.ctl_) {
    internal::refcount::incref(ctl_);
  }

  // Move = steal the token, leaving rhs empty
  r_sexp(r_sexp&& rhs) noexcept : value(rhs.value), ctl_(rhs.ctl_) {
    rhs.value = R_NilValue;
    rhs.ctl_ = nullptr;
  }

  r_sexp& operator=(const r_sexp& rhs) noexcept {
    if (this != &rhs) {
        internal::refcount::incref(rhs.ctl_); // bump new first, release old after
        internal::refcount::decref(ctl_);
        value = rhs.value;
        ctl_ = rhs.ctl_;
    }
    return *this;
  }

  r_sexp& operator=(r_sexp&& rhs) noexcept {
    if (this != &rhs) {
      internal::refcount::decref(ctl_);
      value = rhs.value;
      ctl_ = rhs.ctl_;
      rhs.value = R_NilValue;
      rhs.ctl_ = nullptr;
    }
    return *this;
  }

  ~r_sexp() { internal::refcount::decref(ctl_); }

  // Implicit conversion to SEXP
  operator SEXP() const noexcept { return value; }

  // convert SEXP -> r_sexp directly without extra protection
  explicit r_sexp(SEXP s, internal::view_tag) noexcept : value(s), ctl_(nullptr) {}

  bool is_null() const noexcept {
    return value == R_NilValue; 
  }

  bool is_altrep() const noexcept {
    return static_cast<bool>(ALTREP(value));
  }

  // Is this r_sexp the only owner? Checks cppally's refcount (how many objects share the same protect_cell)
  // and R's C refcount (via NOT_SHARED = REFCNT <= 1)
  // Both conditions must be true
  // Useful for conditional in-place manipulation (e.g. in vector math ops)
  bool is_exclusive() const noexcept {
    return ctl_ != nullptr && ctl_->refs == 1 && NOT_SHARED(value);
  }

  // Not recommended for general usage - currently needed for attribute manipulation
  void ensure_exclusive() {
    if (!is_exclusive()) [[unlikely]] {
      *this = r_sexp(safe[Rf_shallow_duplicate](value));
    }
  }
  void maybe_ensure_exclusive() {
    #ifdef CPPALLY_COPY_ON_MODIFY
    ensure_exclusive();
    #endif
  }

  r_size_t length() const {
    static bool warned = false;
    if (!warned) {
        warn("`r_sexp.length()` is deprecated, please use `cppally::length()`");
        warned = true;
    }
    return Rf_xlength(value);
}

  r_str address() const;
};

// R C NULL constant
inline const r_sexp r_null = r_sexp();
// // Lazy loaded version
// inline const r_sexp& r_null() { static const r_sexp s; return s; }

// Equal to `r_null`?
inline bool is_null(SEXP x) noexcept {
    return x == R_NilValue;
}

inline bool is_altrep(SEXP x) noexcept {
  return r_sexp(x, internal::view_tag{}).is_altrep();
}

namespace internal {
inline bool ptrs_identical(SEXP x, SEXP y) noexcept {
  return x == y;
}
}

}


#endif
