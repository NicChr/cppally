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

// General SEXP, reserved for everything except CHARSXP and SYMSXP
// Wrapper around cpp11::sexp to benefit from automatic protection (cpp11-managed linked list)
//
//
// ----- All credits go to cpp11 authors/maintainers for `cpp11::sexp` -----
//
//

struct r_sexp {

  public:

  SEXP value = R_NilValue;
  using value_type = SEXP;

  private:

  SEXP preserve_token_ = R_NilValue;

  public: 

  r_sexp() = default;
  r_sexp(SEXP data) : value(data), preserve_token_(detail::store::insert(value)) {}

  // We maintain our own new `preserve_token_`
  r_sexp(const r_sexp& rhs) : value(rhs.value), preserve_token_(detail::store::insert(rhs.value)) {}

  // We take ownership over the `rhs.preserve_token_`.
  // Importantly we clear it in the `rhs` so it can't release the object upon destruction.
  r_sexp(r_sexp&& rhs) noexcept : value(rhs.value), preserve_token_(rhs.preserve_token_) {
    rhs.value = R_NilValue;
    rhs.preserve_token_ = R_NilValue;
  }

  r_sexp& operator=(const r_sexp& rhs) noexcept {

    if (this != &rhs) {
      detail::store::release(preserve_token_);
  
      value = rhs.value;
      preserve_token_ = detail::store::insert(value);
    }
      

    return *this;
  }

  r_sexp& operator=(r_sexp&& rhs) noexcept {
    if (this != &rhs) {
      detail::store::release(preserve_token_);
      value = rhs.value;
      
      // Steal the token, do not create a new one
      preserve_token_ = rhs.preserve_token_;
      
      rhs.value = R_NilValue;
      rhs.preserve_token_ = R_NilValue;
    }
    return *this;
  }

  ~r_sexp() { detail::store::release(preserve_token_); }

  // Implicit conversion to SEXP
  constexpr operator SEXP() const noexcept { return value; }
  constexpr SEXP data() const noexcept { return value; }

  // Optimized constructor
  // convert SEXP -> r_sexp directly without extra protection
  explicit r_sexp(SEXP s, internal::view_tag) : value(s) {}

  r_size_t length() const noexcept {
    return Rf_xlength(value);
  }

  r_size_t size() const noexcept {
    return length();
  }

  bool is_null() const { return value == R_NilValue; }
  
  r_str address() const;
};

}


#endif
