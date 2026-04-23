
<!-- badges: start -->

[![R-CMD-check](https://github.com/NicChr/cpp20/actions/workflows/R-CMD-check.yaml/badge.svg)](https://github.com/NicChr/cpp20/actions/workflows/R-CMD-check.yaml)
<!-- badges: end -->

## Acknowledgements

I first want to thank the authors and contributors of the fantastic
[cpp11](https://cpp11.r-lib.org/) R package, without which I would not
have been inspired to write this package. I’d also like to thank the
authors and contributors of [Rcpp](https://github.com/RcppCore/Rcpp) for
developing this ecosystem that has laid much of the groundwork for C++
and R integration.

# cppally

cppally is a high-performance header-only library providing a rich C++20
API for advanced R data manipulation. Leveraging C++20 Concepts, custom
R-based classes, templated functions and
Single-Instruction-Multiple-Data (SIMD) vectorisation, cppally enables
type-safety, performance, flexible templates and readable code.

For info on using cppally see [Getting started with
cppally](https://nicchr.github.io/cpp20/articles/cpp20.html)

## Design choices

### Templates

cppally makes heavy use of templates for powerful object-oriented
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
cppally](https://nicchr.github.io/cpp20/articles/cpp20.html)

### Scalar R types and custom methods

cppally offers R-based C++ scalar types that are `NA` aware. To achieve
this multiple methods such as binary arithmetic operators have been
written to ensure `NA` is propagated correctly. While every attempt has
been made to make this as fast as possible, it adds some overhead and in
some cases can prevent effective vectorisation (via e.g. SIMD
instructions). If you find that this is slowing things down too much you
can work with the underlying C/C++ types using `unwrap_t<>` and
`unwrap()`.

### Automatic protection

Like the excellent cpp11 package, cppally also handles automatic
protection for R objects. For more info see [Automatic
Protection](https://nicchr.github.io/cpp20/articles/protection.html)

### Interopability with the R C API

Using cppally and the R C API is generally discouraged.

If R throws an error via `Rf_error()` a ‘longjmp’ will occur, meaning
C++ destructors won’t run and memory that should have been released will
not be released. If you really want to use the R C API and cppally, you
must make sure that either the code is exception safe (unlikely), or
that R C API functions are called via cppally’s `safe[]`.

### views

To avoid the overhead associated with automatic protection entirely, one
can use view types like e.g. `r_str_view`, a non-owning class for R
strings. For more info on views see [Automatic
Protection](https://nicchr.github.io/cpp20/articles/protection.html)

### No copy-on-write or copy-on-modify

Deep copies are almost never triggered when modifying vectors, a design
choice that contrasts cpp11’s copy-on-write approach. cppally’s
`r_vec<T>` member `set()` always modifies in-place. It is up to the user
to ensure that a fresh vector is created before further manipulation or
that it’s safe to modify the existing vector.

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
cppally delegates the handling of 64-bit integer vectors to bit64 by
marking them with the “integer64” class.

``` cpp
[[cpp::register]]
r_int64 as_int64(r_int x){
    return as<r_int64>(x);
}
```

Please note that other 64-bit signed integer types like `int64_t`,
`R_xlen_t`, or cppally’s identical `r_size_t` will convert to 64-bit
integer vectors when returned to R.

``` cpp
[[cpp::register]]
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

The cppally version of R’s `R_NilValue` is `r_null` which is of type
`r_sexp`. In an attempt to avoid the use of additional meta-programming
tactics to deal with `r_null`, we allow vectors to be able to contain
`r_null` which makes programming with R attributes easier. This means
`r_vec<T>` objects can be `r_null`. To detect this, use the `is_null()`
member function.

## Useful Makevars flags

Because cppally is a template-heavy library, binary sizes can sometimes
get large. This is primarily an issue on windows which will throw a
compiler error if a single .o file gets too big. In this case you may
want to consider adding the following flag to Makevars.win

    PKG_CXXFLAGS = -Wa,-mbig-obj

To benefit from OMP SIMD vectorisation and parallelisation, it is
recommended to add these flags to Makevars

    PKG_CXXFLAGS = $(SHLIB_OPENMP_CXXFLAGS)
    PKG_LIBS = $(SHLIB_OPENMP_CXXFLAGS)

And these flags to Makevars.win (including the windows specific binary
size flags)

    PKG_CXXFLAGS = $(SHLIB_OPENMP_CXXFLAGS) -Wa,-mbig-obj
    PKG_LIBS = $(SHLIB_OPENMP_CXXFLAGS)

## C++20 and RStudio

At the moment C++20 is not fully supported via RStudio, so I would
recommend using vscode with the C/C++ for Visual Studio Code extension.
Positron may also be an option but since I haven’t used it, I can’t
speak to its capabilities.

While I personally use vscode for C++ code and RStudio for R code and
package development, you can also use vscode (or Positron) for both
these things, but again, I haven’t personally used vscode for writing R
code so I can’t say much about it.

To get vscode’s intellisense to work correctly, you will likely need to
set some parameters in c_cpp_properties.json.

My json file looks like this:

``` json
{
  "configurations": [
    {
      "name": "Win32",
      "includePath": [
        "${workspaceFolder}/**",
        "${workspaceFolder}/src",
        "${workspaceFolder}/inst/include",
        "C:/Program Files/R/R-4.*/include",
        "${env:LOCALAPPDATA}/R/win-library/4.*/cpp11/include",
        "${env:LOCALAPPDATA}/R/win-library/4.*/Rcpp/include",
        "${env:LOCALAPPDATA}/R/win-library/4.*/cppally/include"
      ],
      "defines": [
        "_DEBUG",
        "UNICODE",
        "_UNICODE",
        "STRICT_R_HEADERS"
      ],
      "compilerPath": "C:\\rtools45\\x86_64-w64-mingw32.static.posix\\bin\\g++.exe",
      "cppStandard": "gnu++20",
      "intelliSenseMode": "gcc-x64"
    }
  ],
  "version": 4
}
```

As your R installation path may differ, you can find the exact path with

``` r
normalizePath(Sys.getenv("R_HOME"), winslash = "/")
```

Your R libraries can be found with

``` r
.libPaths()
```

The compiler bundled with RTools is likely found here

``` r
cxx <- system2(file.path(R.home("bin"), "R"), c("CMD", "config", "CXX20"), stdout = TRUE)
cxx_bin <- trimws(strsplit(cxx, " ")[[1]][1])
normalizePath(Sys.which(cxx_bin))
```
