# Changelog

## cppally (development version)

- Fixed bug where exclusion of concepts header was affecting vignette
  creation on macos.

- Fixed incorrect NA handling of `r_date` and `r_psxct`.

- Improved type-safety regarding implicit coercion of RScalar types.
  They now can implicitly coerce **only** to their wrapped types,
  whereas before they could implicitly coerce to that as well as other C
  types.

- `r_lgl` can now implicitly coerce to `int` (and only `int`),
  consistent with the other RScalar types.

- New member `is_na` for RScalar types, in addition to the equivalent
  `is_na` free function.

- Fixed bug where checking exact equality (via `identical`) for C/C++
  types was not compiling due to template ordering issue.

- All arithmetic scalar operations have been restricted to types under
  the RMathType umbrella.

## cppally 1.0.0 (2026-07-02)

CRAN release: 2026-07-02

First major release. cppally’s public API is now considered stable.
While there may be structural changes to the `r_df` and `r_raw` classes
in the future, cppally’s vector and scalar classes are considered
stable.

### Breaking changes

- `r_sexp.length` has been deprecated, in favour of the free function
  `length`

- `length(r_df)` now returns the number of rows instead of the number of
  cols, marking a shift in how cppally treats data frames. They are now
  seen as row-wise vectors.

- Setting attributes on plain `SEXP` is now unsupported, e.g. via
  `cppally::attr::set_attr`. Use cppally types such as `r_vector`,
  `r_factors`, `r_df` and in some cases `r_sexp` for attribute
  manipulation.

- Various out-of-place or trivial to implement `r_vec` member functions
  have been removed.

- `visit_vector`, `visit_sexp` and `view_sexp` have been deprecated in
  favour of the more flexible constrained `r_sexp` visitors:
  `r_sexp_visit`, `r_sexp_view` and `r_sexp_mutate`. These allow
  concepts and custom constraints to be applied directly on the lambda’s
  template parameter, e.g. `r_sexp_visit(x, [&]<RVector T>(T vec){})` —
  here `x` is dispatched as its concrete vector type and aborts at
  runtime if the underlying type isn’t an `RVector`. `r_sexp_view` is
  the non-owning sibling: the wrapper handed to the lambda is a view (no
  extra protect), so it must not outlive `x`. `r_sexp_mutate` is for
  in-place mutation: it moves `x` into the typed wrapper (making it the
  sole owner), calls `f`, then writes the result back.

- `r_factors` elements are now treated as `r_str` in member functions
  like `get` and `set`

- `r_sexp_visit` now visits `r_null` as `r_vec<r_sexp>(r_null)`,
  essentially treating `NULL` as an empty list but without changing the
  underlying data.

For example, in the below pseudo-code, when x is `r_null` of type
`r_sexp`, `r_sexp_visit` will disambiguate it as
`r_vec<r_sexp>(r_null)`, preserving its data as R’s `NULL` but assigning
its type as `r_vec<r_sexp>` (list).

``` cpp
 r_sexp_visit(x, [&]<RVector T>(const T& vec) -> bool {
  return vec.is_null();
 });
```

This preservation behaviour is not new, in fact all `r_vec<T>` vectors
preserve `r_null` by design, allowing for efficient and easier attribute
manipulation with vectors that may or may not be `r_null`. What is new
is that previously `r_null` was not a visitable `r_sexp` object and now
it is.

### Data frames

- `r_df` is now fully integrated into cppally.

- New variadic function `make_df` to create in-line data frames.

- Various `r_df` members have been added to allow easier data frame
  manipulation.

### std::vector

- Partial support for `std::vector` coercion. The following
  `std::vector` coercion directions are supported:
  - `std::vector` -\> `std::vector`
  - `std::vector` -\> `cppally::r_vec`
  - `cppally::r_vec` -\> `std::vector`

Any coercion between `std::vector` and `cppally::r_vec` is possible so
long as the element coercions are supported by `cppally::as`

### Regular sequences

- New function `seq` which behaves similarly to
  [`base::seq`](https://rdrr.io/r/base/seq.html).

- New function `sequence` which is similar to
  [`base::sequence`](https://rdrr.io/r/base/sequence.html) but accepts
  only scalar inputs.

### Named vector hash lookups

For named vectors, lookup by name has been dramatically improved in C++
by introducing a hashing approach. It works in the following way: the
first time a lookup is requested, a linear scan is done to find the
named value. The second time triggers the hash map of name-value pairs
to be built and cached with the vector. That second lookup is completed
using the cached hash map and all subsequent lookups also use the hash
map. The rationale for hashing on second lookup is covered in the
‘Automatic Names Hashing’ vignette.

A similar hashing approach is also used for `r_factors`, making
conversions of strings to and from factor codes fast and analytically
viable.

### Copy-on-modify

cppally now supports copy-on-modify as an opt-in feature. This feature
prevents accidentally overwriting data between shared objects, just like
R. To opt-in, run
[`cppally::use_copy_on_modify`](https://nicchr.github.io/cppally/reference/use_copy_on_modify.md)
or set the `copy_on_modify` to `TRUE` in `cpp_source`.

The major downside of this feature is significantly slower element
setting as every set must verify the object is not referenced by another
object. This check is single-threaded and thus nearly all parallel
cppally code is disabled as a safety precaution. If using
copy-on-modify, it is recommended to avoid writing cppally registered R
functions that rely on in-place modification.

### pmap

Inspired by
[`purrr::pmap`](https://purrr.tidyverse.org/reference/pmap.html) and
[`base::mapply`](https://rdrr.io/r/base/mapply.html), `cppally::pmap` is
a C++ variadic function that supports applying custom C++ lambda
functions across corresponding elements of multiple vectors.

With `pmap` it is trivial to calculate parallel statistics like max,
min, etc. Example of C++ version of
[`base::pmax`](https://rdrr.io/r/base/Extremes.html) applied to two
vectors.

``` cpp
template <RVector T, RVector U>
requires requires(typename T::data_type a, typename U::data_type b) { max(a, b); }
[[cppally::register]]
auto cpp_pmax2(T x, U y){
  return pmap([](auto a, auto b){ return max(a, b); }, x, y);
}
```

### reduce

A left-fold reduction functional that successively applies a binary
function along the elements of the vector (from left-to-right).

Example: maximum value across vector of doubles

``` cpp
[[cppally::register]]
r_dbl cpp_max(r_vec<r_dbl> x){
  return x.reduce([](auto acc, auto curr){ return max(acc, curr); });
}
```

### Other changes

- New alias of `r_vec`, `r_vector`.

- Named-vector subsetting is now supported.

- New C++ functions `combine` and `flatten`. `combine` is a variadic
  function that allows for combining multiple vectors into one, similar
  to [`base::c`](https://rdrr.io/r/base/c.html) but always casts vectors
  to the common type among them. `flatten` allows one to flatten a list
  of vectors into one vector of a specified type, similar to
  `unlist(recursive = FALSE)`.

- Many functions that were originally `r_vec`-only members are now free
  functions that also work on `r_sexp` as well as `RComposite` types,
  allowing for easier manipulation of lists.

- All C++ reference qualifiers (T&, T&&, const T&) are now supported for
  registered functions, including templated ones.

- New concept `RVectorisable` which encompasses types that are OMP
  friendly.

- New infix operator `IS_IN`, identical to R’s `%in%`.

- New C++ function `coalesce()`.

- `r_psxct.datetime_str()` always appends “UTC” at the end to avoid
  time-zone ambiguity.

### Bug fixes

- When registering C++ functions, cppally.hpp is now included in the
  generated C++ code. Not including it caused issues when trying to
  compile functions that constructed factors.

- Zero-length `r_vec` vectors can now be constructed unambiguously via
  `r_vec<T>(0)`.

- Math operations involving mixed types that included `r_dbl` are now
  correct when involving `NA` values.

## cppally 0.1.0

CRAN release: 2026-04-28

- Initial CRAN submission.
