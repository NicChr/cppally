#ifndef CPPALLY_R_LIST_PMAP_H
#define CPPALLY_R_LIST_PMAP_H

#include <cppally/r_visit.h>    // common_ptype, r_sexp_view
#include <cppally/r_coerce.h>
#include <cppally/sugar/r_sexp_methods.h>
#include <span>
#include <vector>

namespace cppally {

// Like cppally::pmap but works on a list of vectors instead of variadic inputs
// IMPORTANT: the inputs are coerced to a common type; the result's type is the
// caller-supplied `ret_t` (fn's return is coerced into it via r_vec::set).
// The core issue is that the vectors must be visited (via `r_sexp_view`)
// which incurs a combinatorial instantiation cost of 15^k instantiations for k vectors
// After 6 vectors compile-time becomes practically impossible (15^6 = 11390625 instantiations)
// To get around this C++ limitation, we find the common type and coerce all vectors to that type before applying the function
template <RVector ret_t = r_vec<r_sexp>, typename F>
ret_t list_pmap(const r_vec<r_sexp>& vectors, F fn) {
  r_size_t k = vectors.length();
  if (k == 0) return ret_t();

  return r_sexp_view(common_ptype(vectors), [&]<RVector T>(const T&) -> ret_t {
    using elem_t = typename T::data_type;

    // fn is handed a mutable std::span<elem_t>, so callers never need to write const. The
    // lambda is instantiated for EVERY RVector T, so only build the body for element types
    // fn accepts; the rest abort at runtime (never reached, since the common type is fixed).
    if constexpr (std::invocable<F, std::span<elem_t>>) {
      // Coerce each list element to the common type, recording lengths.
      std::vector<T>        coerced;  coerced.reserve(k);
      std::vector<r_size_t> lens;     lens.reserve(k);
      r_size_t n = length(vectors.view(0));
      bool any_zero = false;
      bool recycle = false;
      for (r_size_t j = 0; j < k; ++j) {
        coerced.push_back(as<T>(vectors.view(j)));
        r_size_t len = length(coerced.back());
        lens.push_back(len);
        recycle |= len != n;
        n = std::max(n, len);
        any_zero |= (len == 0);
      }
      if (any_zero){
        return ret_t();
      }

      ret_t out(n);
      std::vector<elem_t> row(k);

      if (recycle){
        std::vector<r_size_t> idx(k, 0);                   // per-vector recycle counters
        for (r_size_t i = 0; i < n; ++i) {
          for (r_size_t j = 0; j < k; ++j) row[j] = coerced[j].view(idx[j]);
          out.set(i, fn(std::span<elem_t>(row.data(), k)));
          for (r_size_t j = 0; j < k; ++j) recycle_index(idx[j], lens[j]);
        }
      } else {
        // All vectors are length n - index directly, no recycle counters needed.
        for (r_size_t i = 0; i < n; ++i) {
          for (r_size_t j = 0; j < k; ++j) row[j] = coerced[j].view(i);
          out.set(i, fn(std::span<elem_t>(row.data(), k)));
        }
      }

      return out;
    } else {
      abort("list_pmap: fn does not accept vectors of the list's common type (%s)",
            internal::type_str<T>());
    }
  });
}

}

#endif
