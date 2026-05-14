# Adds the `CPPALLY_CHECK_FACTORS` flag to Makevars

Adds a flag to Makevars which enables stricter validation on factor
levels at the point of `r_factors` construction. This avoids creating
`r_factors` objects with invalid levels and avoiding potential R
crashes.

The default behaviour is NOT to validate factor levels, which is
naturally faster when calling C++ functions that take `r_factors`
inputs.

## Usage

``` r
use_check_factors()
```

## Value

Invisibly adds the `CPPALLY_CHECK_FACTORS` flag to Makevars.
