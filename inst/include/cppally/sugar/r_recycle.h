#ifndef CPPALLY_R_RECYCLE_H
#define CPPALLY_R_RECYCLE_H

#include <cppally/r_visit.h>
#include <cppally/sugar/r_list_helpers.h>
#include <cppally/sugar/r_make_vec.h>
#include <initializer_list>

namespace cppally {

// Recycle helper functions

namespace internal {

// In-place recycle
inline void recycle_impl(r_vec<r_sexp>& x, r_size_t common_size) {
  x.apply([common_size](const r_sexp& v) { return rep_len(v, common_size); });
}
  
}

inline r_size_t list_common_length(const r_vec<r_sexp>& x){

  return x.reduce([](r_size_t acc, const r_sexp& curr) {
    
    r_size_t n = length(curr);

    return n == 0 ? done(r_size_t{0}) : keep(std::max(acc, n));
  }, /*init=*/ r_size_t{0});

}

template <RComposite T>
inline r_size_t common_length(std::initializer_list<T> args){
  r_size_t n_vectors = static_cast<r_size_t>(args.size());
  r_vec<r_sexp> vectors(n_vectors);
  for (r_size_t i = 0; i < n_vectors; ++i){
    vectors.set(i, args.begin()[i]);
  }
  return list_common_length(vectors);
}

inline r_vec<r_sexp> list_recycle(const r_vec<r_sexp>& x) {
  r_vec<r_sexp> out = x.remove(r_null);
  out.ensure_exclusive();
  internal::recycle_impl(out, list_common_length(out));
  return out;
}

template <RComposite T>
inline r_vec<r_sexp> recycle(std::initializer_list<T> args){
  r_size_t n_vectors = static_cast<r_size_t>(args.size());
  r_vec<r_sexp> vectors(n_vectors);
  for (r_size_t i = 0; i < n_vectors; ++i){
    vectors.set(i, args.begin()[i]);
  }
  return list_recycle(vectors);
}

// Variadic recycle function
template <typename... Args>
inline r_vec<r_sexp> recycle(Args&&... args) {
  r_vec<r_sexp> vectors = make_vec<r_sexp>(std::forward<Args>(args)...);
  return list_recycle(vectors);
}

}

#endif
