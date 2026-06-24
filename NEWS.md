# cppally (development version)

### Data frames

- `r_df` is now fully integrated into cppally

- New variadic function `make_df()` to create in-line data frames

- Various `r_df` members have been added to allow easier data frame manipulation

### Breaking changes

- `r_sexp.length()` has been deprecated, in favour of the free function `length()`

- `length(r_df)` now returns the number of rows instead of the number of cols, marking a shift in how cppally treats data frames. They are now seen as row-wise vectors

- Setting attributes on plain `SEXP` is now unsupported, e.g. via `cppally::attr::set_attr()`. Use cppally types such as `r_vector`, `r_factors`, `r_df` and in some cases `r_sexp` for attribute manipulation.

- `visit_vector`, `visit_sexp` and `view_sexp` have been deprecated in favour of the more flexible constrained `r_sexp` visitors: `r_sexp_visit`, `r_sexp_view` and `r_sexp_mutate`. These allow concepts and custom constraints to be applied directly on the lambda's template parameter, e.g. `r_sexp_visit(x, [&]<RVector T>(T vec){})` — here `x` is dispatched as its concrete vector type and aborts at runtime if the underlying type isn't an `RVector`. `r_sexp_view` is the non-owning sibling: the wrapper handed to the lambda is a view (no extra protect), so it must not outlive `x`. `r_sexp_mutate` is for in-place mutation: it moves `x` into the typed wrapper (making it the sole owner), calls `f`, then writes the result back.

- `r_factors` elements are now treated as `r_str` in member functions like `get()` and `set()`

- `r_sexp_visit()` now visit `r_null` as `r_vec<r_sexp>(r_null)`, essentially treating `NULL` as an empty list but without changing the underlying data

For example, in the below pseudo-code, when x is `r_null` of type `r_sexp`, `r_sexp_visit()` will disambiguate it as `r_vec<r_sexp>(r_null)`, preserving its data as R's `NULL` but assigning its type as `r_vec<r_sexp>` (list).

``` cpp
 r_sexp_visit(x, [&]<RVector T>(const T& vec) -> bool {
  return vec.is_null();
 });
```

This preservation behaviour is not new, in fact all `r_vec<T>` vectors preserve `r_null` by design, allowing for efficient and easier attribute manipulation with vectors that may or may not be `r_null`. What is new is that previously `r_null` was not a visitable `r_sexp` object and now it is.

### std::vector

- Partial support for `std::vector` coercion. The following `std::vector` coercion directions are supported:
  - `std::vector` -\> `std::vector`
  - `std::vector` -\> `cppally::r_vec`
  - `cppally::r_vec` -\> `std::vector`

Any coercion between `std::vector` and `cppally::r_vec` is possible so long as the element coercions are supported by `cppally::as`

### Improvements and New Features

#### Named vector hash lookups

For named vectors, lookup by name has been dramatically improved in C++ by introducing a hashing approach. It works in the following way: the first time a lookup is requested, a linear scan is done to find the named value. The second time triggers the hash map of name-value pairs to be built and cached with the vector. That second lookup is completed using the cached hash map and all subsequent lookups also use the hash map. The rationale for hashing on second lookup is covered in the 'Automatic Names Hashing' vignette.

A similar hashing approach is also used for `r_factors`, making conversions of strings to and from factor codes fast and analytically viable.

#### Copy-on-modify

cppally now supports copy-on-modify as an opt-in feature. This feature prevents accidentally overwriting data between shared objects, just like R. To opt-in, run `cppally::use_copy_on_modify()` or set the `copy_on_modify` to `TRUE` in `cpp_source()`.

The major downside of this feature is significantly slower element setting as every set must verify the object is not referenced by another object. This check is single-threaded and thus nearly all parallel cppally code is disabled as a safety precaution. If using copy-on-modify, it is recommended to avoid writing cppally registered R functions that rely on in-place modification.

#### pmap

Inspired by `purrr::pmap` and `base::mapply`, `cppally::pmap` is a C++ variadic function that supports applying custom C++ lambda functions across corresponding elements of multiple vectors.

With `pmap()` it is trivial to calculate parallel statistics like max, min, etc. Example of C++ version of `base::pmax()` applied to two vectors

``` cpp
template <RVector T, RVector U>
requires requires(typename T::data_type a, typename U::data_type b) { max(a, b); }
[[cppally::register]]
auto cpp_pmax2(T x, U y){
  return pmap([](auto a, auto b){ return max(a, b); }, x, y);
}
```

While `pmap()` is a powerful iterator, as with all variadic functions, the number of inputs must be known at compile-time, therefore we can't write an exact `base::pmax()` equivalent using `pmap()` because the number of vectors is only known at runtime.

`list_pmap()` doesn't have this limitation and can iterate over n vectors where n is known at runtime. This comes with other trade-offs which are detailed below.

|   | pmap | list_pmap |
|------------------------------|---------------------|---------------------|
| Inputs | Function in arg-1, variadic-supplied vectors thereafter | List of vectors in arg-1, function in arg-2 |
| Return type deduced at compile-time | Yes | No - defaults to `r_sexp` but can also be specified |
| SIMD-enabled loops | Yes | No |
| Parallel loops (\>1 thread) | Yes | No |
| Speed | Very fast | Fast with some overhead |
| Lambda requirements | Scalar inputs expressed explicitly | Container whose size is known at runtime like `std::span` (recommended) or `std::vector` |
| Accepted R objects | Vectors only | Vectors only |

The biggest and perhaps most surprising limitation of `list_pmap` is that it coerces all its vectors to a common type. Visiting all `r_sexp` objects simultaneously (via `r_sexp_view`) would incur a combinatorial explosion of instantiation size, roughly 15\^k for k vectors, therefore this is practically impossible for any reasonably-sized list.

Example of C++ version of `base::pmax`

``` cpp
[[cppally::register]]
SEXP list_pmax(r_vec<r_sexp> vectors){
  return list_pmap<r_vec<r_dbl>>(vectors, []<RMathType elem>(std::span<elem> r) {
    elem m = r[0];
    for (size_t i = 1; i < r.size(); ++i){
      m = max(m, r[i]);
    }
    return m;
  });
}
```

``` r
cpp_pmax <- function(...){
  list_pmax(list(...))
}
```

``` r
cpp_pmax2(c(0, 2, 4), c(1, 2, 3)) # Parallel max across 2 vectors
1 2 4
cpp_pmax(c(0, 2, 4), c(1, 2, 3), c(-5, 0, 5)) # Parallel max across k vectors
1 2 5
```

For performance reasons, always use `pmap` if you know the number of vectors up front, otherwise use `list_pmap`.

#### Other improvements

- New alias of `r_vec`, `r_vector`

- Named-vector subsetting is now supported

- New C++ functions `combine()` and `flatten()`. `combine()` is a variadic function that allows for combining multiple vectors into one, similar to `base::c()` but always casts vectors to the common type among them. `flatten()` allows one to flatten a list of vectors into one vector of a specified type. Similar to `unlist(recursive = FALSE)` but it differs in that only the return type must be specified, e.g. `flatten<r_vector<r_int>>(make_vec<r_sexp>(1, 2, 3))`

- Many functions that were originally `r_vec`-only members are now free functions that also work on `r_sexp` as well as `RComposite` types, allowing for easier manipulation of lists.

- All C++ reference qualifiers (T&, T&&, const T&) are now supported for registered functions, including templated ones.

- New concept `RVectorisable` which encompasses types that are OMP friendly.

### Bug fixes

- When registering C++ functions, cppally.hpp is now included in the generated C++ code. Not including it caused issues when trying to compile functions that constructed factors

- Zero-length `r_vec` vectors can now be constructed unambiguously via `r_vec<T>(0)`.

# cppally 0.1.0

- Initial CRAN submission.
