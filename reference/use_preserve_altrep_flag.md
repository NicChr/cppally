# Adds the CPPALLY_PRESERVE_ALTREP flag to Makevars

Adds a flag to Makevars which enables lazy materialisation of ALTREP
vectors.

## Usage

``` r
use_preserve_altrep_flag(quiet = FALSE)
```

## Arguments

- quiet:

  `[logical(1)]` - Should messages be suppressed? Default is `FALSE`.

## Value

Invisibly adds the CPPALLY_PRESERVE_ALTREP flag to Makevars.
