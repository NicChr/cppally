# A wrapper around `devtools::load_all()` specifically for cppally

A wrapper around
[`devtools::load_all()`](https://devtools.r-lib.org/reference/load_all.html)
specifically for cppally

## Usage

``` r
load_all(
  path = ".",
  debug = FALSE,
  cppally_header = c("cppally.hpp", "cppally_light.hpp"),
  ...
)
```

## Arguments

- path:

  Path to package.

- debug:

  Should package be built without optimisations? Default is `FALSE`
  which builds with optimisations.

- cppally_header:

  Which header should be included with the registered C++ code? The
  default is the full library "cppally.hpp". Choose "cppally_light.hpp"
  for the lighter header, which may provide quicker compile times, at
  the cost of less features.

- ...:

  Further arguments passed on to
  [`pkgload::load_all()`](https://pkgload.r-lib.org/reference/load_all.html)

## Value

Invisibly registers cppally tagged functions and compiles C++ code.
