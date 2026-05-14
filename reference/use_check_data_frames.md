# Adds the `CPPALLY_CHECK_DATA_FRAMES` flag to Makevars

Adds a flag to Makevars which enables stricter validation on data frames
at the point of `r_df` construction. This ensures that column lengths
are always valid, avoiding potential R crashes downstream.

The default behaviour is NOT to validate column lengths, enabling faster
`r_df` creating from `SEXP`.

## Usage

``` r
use_check_data_frames()
```

## Value

Invisibly adds the `CPPALLY_CHECK_DATA_FRAMES` flag to Makevars.
