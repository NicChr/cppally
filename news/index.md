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

- `r_factors` elements are now treated as `r_str` in member functions
  like [`get()`](https://rdrr.io/r/base/get.html) and `set()`

- `visit_vector()` and `visit_sexp()` now visit `r_null` as
  `r_vec<r_sexp>(r_null)`, essentially treating `NULL` as an empty list
  but without changing the underlying data

For example, in the below pseudo-code, when x is `r_null` of type
`r_sexp`, `visit_vector()` will disambiguate it as
`r_vec<r_sexp>(r_null)`, preserving its data as R’s `NULL` but assigning
its type as `r_vec<r_sexp>` (list).

``` cpp
 visit_vector(x, [&](const auto& vec) -> bool {
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

#### Improvements

- New alias of `r_vec`, `r_vector`

- For named vectors, lookup by name has been dramatically improved in
  C++ by introducing a hashing approach. On second lookup, a hash map of
  names is created and cached, making subsequent lookups much faster.
  This also applies to factor levels.

- Named-vector subsetting is now supported

- New C++ functions `combine()` and `flatten()`. `combine()` is a
  variadic function that allows for combining multiple vectors into one,
  similar to [`base::c()`](https://rdrr.io/r/base/c.html) but always
  casts vectors to the common type among them. `flatten()` allows one to
  flatten a list of vectors into one vector of a specified type. Similar
  to `unlist(recursive = FALSE)` but it differs in that only the return
  type must be specified,
  e.g. `flatten<r_vector<r_int>>(make_vec<r_sexp>(1, 2, 3))`

- Many functions that were originally `r_vec` only members are now free
  functions that also work on `r_sexp` as well as `RComposite` types,
  allowing for easier manipulation of lists.

#### Bug fixes

- When registering C++ functions, cppally.hpp is now included in the
  generated C++ code. Not including it caused issues when trying to
  compile functions that constructed factors

## cppally 0.1.0

CRAN release: 2026-04-28

- Initial CRAN submission.
