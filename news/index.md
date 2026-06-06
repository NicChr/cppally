# Changelog

## cppally (development version)

#### Data frames

- `r_df` is now fully integrated into cppally

- New variadic function `make_df()` to create in-line data frames

- Various `r_df` members have been added to allow easier data frame
  manipulation

#### Breaking changes

- `r_sexp.length()` has been deprecated, in favour of the free function
  [`length()`](https://rdrr.io/r/base/length.html)

- `length(r_df)` now returns the number of rows instead of the number of
  cols, marking a shift in how cppally treats data frames. They are now
  seen as row-wise vectors

- Setting attributes on plain `SEXP` is now unsupported, e.g. via
  `cppally::attr::set_attr()`. Use cppally types such as `r_vector`,
  `r_factors`, `r_df` and in some cases `r_sexp` for attribute
  manipulation.

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
  like [`get()`](https://rdrr.io/r/base/get.html) and `set()`

- `r_sexp_visit()` now visit `r_null` as `r_vec<r_sexp>(r_null)`,
  essentially treating `NULL` as an empty list but without changing the
  underlying data

For example, in the below pseudo-code, when x is `r_null` of type
`r_sexp`, `r_sexp_visit()` will disambiguate it as
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

#### std::vector

- Partial support for `std::vector` coercion. The following
  `std::vector` coercion directions are supported:
  - `std::vector` -\> `std::vector`
  - `std::vector` -\> `cppally::r_vec`
  - `cppally::r_vec` -\> `std::vector`

Any coercion between `std::vector` and `cppally::r_vec` is possible so
long as the element coercions are supported by `cppally::as`

#### Improvements and New Features

- New alias of `r_vec`, `r_vector`

- For named vectors, lookup by name has been dramatically improved in
  C++ by introducing a hashing approach. On second lookup, a hash map of
  names is created and cached, making subsequent lookups much faster.
  This also applies to factor levels.

- cppally now supports copy-on-modify as an opt-in feature. This feature
  prevents accidentally overwriting data between shared objects, just
  like R. To opt-in, run
  [`cppally::use_copy_on_modify()`](https://nicchr.github.io/cppally/reference/use_copy_on_modify.md)
  or set the `copy_on_modify` to `TRUE` in
  [`cpp_source()`](https://nicchr.github.io/cppally/reference/cpp_source.md).
  The major downside of this feature is significantly slower element
  setting as every set must verify the object is not referenced by
  another object. This check is single-threaded and thus nearly all
  parallel cppally code is disabled as a safety precaution. If using
  copy-on-modify, it is recommended to avoid writing cppally registered
  R functions that rely on in-place modification.

- Named-vector subsetting is now supported

- New C++ functions `combine()` and `flatten()`. `combine()` is a
  variadic function that allows for combining multiple vectors into one,
  similar to [`base::c()`](https://rdrr.io/r/base/c.html) but always
  casts vectors to the common type among them. `flatten()` allows one to
  flatten a list of vectors into one vector of a specified type. Similar
  to `unlist(recursive = FALSE)` but it differs in that only the return
  type must be specified,
  e.g. `flatten<r_vector<r_int>>(make_vec<r_sexp>(1, 2, 3))`

- Many functions that were originally `r_vec`-only members are now free
  functions that also work on `r_sexp` as well as `RComposite` types,
  allowing for easier manipulation of lists.

- All C++ reference qualifiers (T&, T&&, const T&) are now supported for
  registered functions, including templated ones.

#### Bug fixes

- When registering C++ functions, cppally.hpp is now included in the
  generated C++ code. Not including it caused issues when trying to
  compile functions that constructed factors

## cppally 0.1.0

CRAN release: 2026-04-28

- Initial CRAN submission.
