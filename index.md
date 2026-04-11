# cpp20

cpp20 is a high-performance header-only library providing a rich C++20
API for advanced R data manipulation. Leveraging C++20 Concepts, custom
R-based classes, templated functions and
Single-Instruction-Multiple-Data (SIMD) vectorisation, cpp20 enables
type-safety, performance, flexible templates and readable code.

For info on using cpp20 see [Getting started with
cpp20](https://nicchr.github.io/cpp20/vignettes/cpp20.Rmd)

### Design choices

**Templates**

cpp20 makes heavy use of templates for powerful object-oriented
programming. While this offers a flexible framework for writing generic
functions, it comes at the cost of slower compile times and larger
binary sizes.

Users can write and optionally register their own templates (to R).
There are two main limitations to be aware of - a C++ specific one and
an R specific one. The C++ limitation is that templates generally must
be written in header files if they are to be used across multiple
compilation units. The R limitation has to do with automatic template
argument deduction. There is a workaround that I will discuss in a later
section but it is not ideal.

**Scalar R types and custom methods**

cpp20 offers R-based C++ scalar types that are `NA` aware. To achieve
this multiple methods such as binary arithmetic operators have been
written to ensure `NA` is propagated correctly. While every attempt has
been made to make this as fast as possible, it adds some overhead and in
some cases can prevent effective vectorisation (via e.g. SIMD
instructions). If you find that this is slowing things down too much you
can work with the underlying C/C++ types using `unwrap_t<>` and
`unwrap()`.

**Automatic protection**

Like the excellent cpp11 package, cpp20 also handles automatic
protection for R objects.

Heavily inspired by cpp11’s double-linked protection list, we also use a
double-linked list system. Instead of a single pairlist, we use
double-linked chain of vector list chunks. We find that this generally
offers lower protection overhead. Furthermore, reference counting is
utilised to make copying `r_sexp` cheaper by avoiding re-inserting
copies into the protection list where possible.

The same caveats surrounding R longjmp errors that apply to cpp11
protection also applies to cpp20.

**R String views**

To avoid the overhead associated with automatic protection entirely, one
can use `r_str_view`, a non-owning class for R strings. `r_str_view` is
designed for read-only and short-lived contexts such as accessing and
manipulating the elements of a character vector. Using `r_str_view`
guarantees that no extra re-protections occur when the string object is
copied or moved due to the fact that it is simply a light wrapper around
`SEXP`. Similar to `std::string_view`, the major caveat is that you must
ensure the `r_str_view` object’s lifetime does not extend beyond the
lifetime of the R string (CHARSXP) it is pointing to. See the Annex of
the vignette.

**No copy-on-write or copy-on-modify**

Deep copies are almost never triggered when modifying vectors, a design
choice that contrasts cpp11’s copy-on-write approach. cpp20’s `r_vec<T>`
member `set()` always modifies in-place. It is up to the user to ensure
that a fresh vector is created before further manipulation or that it’s
safe to modify the existing vector.

**Differences between R and cpp20**

Any coercion that results in complete information loss is an error
(partial is allowed, e.g. double -\> int). This means that for example
`as<r_int>(r_str("a"))` will throw an error instead of returning an NA
like R does with `as.integer("a")`

The benefit of this is that when registering C++ functions to R, inputs
can be supplied flexibly without unexpected behaviour. Let’s say you
have a function foo that expects an `r_int` but you give it an `r_dbl`
without realising - this will implicitly coerce to `r_int` without
throwing an error. If it can’t be coerced to integer without complete
information loss (i.e non-parse-able string to integer), then an
informative error is thrown. If we allowed implicit coercion to NA then
we would need to be strict with inputs to balance things out.

**Vector indexing**

Most indexing is 0-based except when dealing with vectors of indices,
which are 1-indexed. 1-indexed indices are used in
[`subset()`](https://rdrr.io/r/base/subset.html),
[`find()`](https://rdrr.io/r/utils/apropos.html), and other functions
which accept or return a vector of indices.

**64-bit integers**

On the C++ side, 64-bit integers are fully supported, including vectors.
To return 64-bit integers to R we need the bit64 package to be loaded.
cpp20 delegates the handling of 64-bit integer vectors to bit64 by
marking them with the “integer64” class.

``` cpp
[[cpp20::register]]
r_int64 as_int64(r_int x){
    return as<r_int64>(x);
}
```

Please note that other 64-bit signed integer types like `int64_t`,
`R_xlen_t`, or cpp20’s identical `r_size_t` will convert to 64-bit
integer vectors when returned to R.

``` cpp
[[cpp20::register]]
r_size_t as_r_size_t(r_int x){
    return as<r_size_t>(x);
}
```

``` r
library(bit64)
as_int64(10L)
integer64
[1] 10
as_r_size_t(10L)
integer64
[1] 10
```

**Using R’s `NULL`**

The cpp20 version of R’s `R_NilValue` is `r_null` which is of type
`r_sexp`. In an attempt to avoid the use of additional meta-programming
tactics to deal with `r_null`, we allow vectors to be able to contain
`r_null` which makes programming with R attributes easier. This means
`r_vec<T>` objects can be `r_null`. To detect this, use the `is_null()`
member function.
