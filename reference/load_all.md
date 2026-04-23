# A wrapper around `devtools::load_all()` specifically for cpp20

A wrapper around
[`devtools::load_all()`](https://devtools.r-lib.org/reference/load_all.html)
specifically for cpp20

## Usage

``` r
load_all(path = ".", debug = FALSE, ...)
```

## Arguments

- path:

  Path to package.

- debug:

  Should package be built without optimisations? Default is `FALSE`
  which builds with optimisations.

- ...:

  Further arguments passed on to
  [`pkgload::load_all()`](https://pkgload.r-lib.org/reference/load_all.html)

## Value

Invisibly registers cpp20 tagged functions and compiles C++ code.
