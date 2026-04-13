
<!-- badges: start -->

[![R-CMD-check](https://github.com/NicChr/cpp20/actions/workflows/R-CMD-check.yaml/badge.svg)](https://github.com/NicChr/cpp20/actions/workflows/R-CMD-check.yaml)
<!-- badges: end -->

I first want to thank cpp11, its authors and contributors for the
inspiration and for producing the excellent cpp11 package, without which
I would not have written this package.

# cpp20

cpp20 is a high-performance header-only library providing a rich C++20
API for advanced R data manipulation. Leveraging C++20 Concepts, custom
R-based classes, templated functions and
Single-Instruction-Multiple-Data (SIMD) vectorisation, cpp20 enables
type-safety, performance, flexible templates and readable code.

For info on using cpp20 see [Getting started with
cpp20](https://nicchr.github.io/cpp20/articles/cpp20.html)

## Design choices

### Templates

cpp20 makes heavy use of templates for powerful object-oriented
programming. While this offers a flexible framework for writing generic
functions, it comes at the cost of slower compile times and larger
binary sizes.

Users can write and optionally register their own templates (to R).
There are two main limitations to be aware of - a C++ specific one and
an R specific one. The C++ limitation is that templates generally must
be written in header files if they are to be used across multiple
compilation units. The R limitation has to do with automatic template
argument deduction. There is a workaround discussed in the main vignette
[Getting started with
cpp20](https://nicchr.github.io/cpp20/articles/cpp20.html)

### Scalar R types and custom methods

cpp20 offers R-based C++ scalar types that are `NA` aware. To achieve
this multiple methods such as binary arithmetic operators have been
written to ensure `NA` is propagated correctly. While every attempt has
been made to make this as fast as possible, it adds some overhead and in
some cases can prevent effective vectorisation (via e.g. SIMD
instructions). If you find that this is slowing things down too much you
can work with the underlying C/C++ types using `unwrap_t<>` and
`unwrap()`.

### Automatic protection

Like the excellent cpp11 package, cpp20 also handles automatic
protection for R objects. For more info see [Automatic
Protection](https://nicchr.github.io/cpp20/articles/protection.html)

### Interopability with the R C API

Using cpp20 and the R C API is generally discouraged.

If R throws an error via `Rf_error()` a ‘longjmp’ will occur, meaning
C++ destructors won’t run and memory that should have been released will
not be released. If you really want to use the R C API and cpp20, you
must make sure that either the code is exception safe (unlikely), or
that R C API functions are called via cpp20’s `safe[]`. \### views

To avoid the overhead associated with automatic protection entirely, one
can use view types like e.g. `r_str_view`, a non-owning class for R
strings. Vector member function `view()` also returns non-owning
view-only vector elements. For more info on views see [Automatic
Protection](https://nicchr.github.io/cpp20/articles/protection.html)

### No copy-on-write or copy-on-modify

Deep copies are almost never triggered when modifying vectors, a design
choice that contrasts cpp11’s copy-on-write approach. cpp20’s `r_vec<T>`
member `set()` always modifies in-place. It is up to the user to ensure
that a fresh vector is created before further manipulation or that it’s
safe to modify the existing vector.

### Lossy coercion

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

### Vector indexing

All indexing is 0-based including subsetting vectors.

### 64-bit integers

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

### Using R’s `NULL`

The cpp20 version of R’s `R_NilValue` is `r_null` which is of type
`r_sexp`. In an attempt to avoid the use of additional meta-programming
tactics to deal with `r_null`, we allow vectors to be able to contain
`r_null` which makes programming with R attributes easier. This means
`r_vec<T>` objects can be `r_null`. To detect this, use the `is_null()`
member function.

## Useful Makevars flags

Because cpp20 is a template-heavy library, binary sizes can sometimes
get large. This is primarily an issue on windows which will throw a
compiler error if a single .o file gets too big. In this case you may
want to consider adding the following flag to Makevars.win

    PKG_CXXFLAGS = -Wa,-mbig-obj

To benefit from OMP SIMD vectorisation and parallelisation, it is
advised to add these flags to Makevars and Makevars.win

    PKG_CXXFLAGS = $(SHLIB_OPENMP_CXXFLAGS)
    PKG_LIBS = $(SHLIB_OPENMP_CXXFLAGS)

All together they would be

    PKG_CXXFLAGS = $(SHLIB_OPENMP_CXXFLAGS) -Wa,-mbig-obj
    PKG_LIBS = $(SHLIB_OPENMP_CXXFLAGS)
