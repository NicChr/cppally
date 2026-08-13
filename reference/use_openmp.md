# Add OpenMP flags to Makevars

Adds the correct flags to Makevars, enabling SIMD vectorisation and
multi-threading via OpenMP.

## Usage

``` r
use_openmp(quiet = FALSE)
```

## Arguments

- quiet:

  `[logical(1)]` - Should messages be suppressed? Default is `FALSE`.

## Value

Invisibly adds the '\$(SHLIB_OPENMP_CXXFLAGS)' value to the Makevars
variables 'PKG_LIBS' and 'PKG_CXXFLAGS', enabling multi-threading and
SIMD vectorisation via OpenMP.
