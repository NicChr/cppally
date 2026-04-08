#ifndef CPP20_R_SEXP_H
#define CPP20_R_SEXP_H

#include <cpp20/r_setup.h>
#include <cpp20/r_concepts.h>
#include <cpp20/r_protect.h>

namespace cpp20 {

namespace internal {
// Helper struct to allow for overloading SEXP-based constructors without re-protecting them
struct view_tag {};
}

// General SEXP, automatic protection handled via cpp20-managed vector list
//
// ----- All credits go to cpp11 authors/maintainers for inspiration from `cpp11::sexp` -----
//

struct r_sexp {

  public:

  SEXP value = R_NilValue;
  using value_type = SEXP;

  private:

  // Refcounted protection token. nullptr means "view mode" (no protection).
  // Copy construction bumps `ctl_->refs` instead of allocating a new cons cell,
  // so passing r_sexp around by value is essentially free
  detail::refcount::protect_cell* ctl_ = nullptr;

  public:

  r_sexp() = default;
  explicit r_sexp(SEXP data) : value(data), ctl_(detail::refcount::insert(data)) {}

  // Copy = refcount bump (no R API involvement)
  r_sexp(const r_sexp& rhs) noexcept : value(rhs.value), ctl_(rhs.ctl_) {
    detail::refcount::incref(ctl_);
  }

  // Move = steal the token, leaving rhs empty
  r_sexp(r_sexp&& rhs) noexcept : value(rhs.value), ctl_(rhs.ctl_) {
    rhs.value = R_NilValue;
    rhs.ctl_ = nullptr;
  }

  r_sexp& operator=(const r_sexp& rhs) noexcept {
    if (this != &rhs) {
        detail::refcount::incref(rhs.ctl_); // bump new first, release old after
        detail::refcount::decref(ctl_);
        value = rhs.value;
        ctl_ = rhs.ctl_;
    }
    return *this;
  }

  r_sexp& operator=(r_sexp&& rhs) noexcept {
    if (this != &rhs) {
      detail::refcount::decref(ctl_);
      value = rhs.value;
      ctl_ = rhs.ctl_;
      rhs.value = R_NilValue;
      rhs.ctl_ = nullptr;
    }
    return *this;
  }

  ~r_sexp() { detail::refcount::decref(ctl_); }

  // Implicit conversion to SEXP
  operator SEXP() const noexcept { return value; }
  constexpr SEXP data() const noexcept { return value; }

  // Optimized constructor
  // convert SEXP -> r_sexp directly without extra protection
  explicit r_sexp(SEXP s, internal::view_tag) noexcept : value(s), ctl_(nullptr) {}

  r_size_t length() const noexcept {
    return Rf_xlength(value);
  }

  r_size_t size() const noexcept {
    return length();
  }

  bool is_null() const noexcept { return value == R_NilValue; }
  
  r_str address() const;
};

// R C NULL constant
inline const r_sexp r_null = r_sexp();

}


#endif
