# Changelog

## cppally (development version)

#### Data frames

- `r_df` is now fully integrated into cppally

- New variadic function `make_df()` to create in-line data frames

#### Breaking changes

- `r_sexp.length()` has been deprecated, in favour of the free function
  [`length()`](https://rdrr.io/r/base/length.html)

- `length(r_df)` now returns the number of rows instead of the number of
  cols, marking a shift in how cppally treats data frames. They are now
  seen as row-wise vectors

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

- When registering C++ functions, cppally.hpp is now included in the
  generated C++ code. Not including it caused issues when trying to
  compile functions that constructed factors

- Named-vector subsetting is now supported

## cppally 0.1.0

CRAN release: 2026-04-28

- Initial CRAN submission.
