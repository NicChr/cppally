# Adds the `CPPALLY_COPY_ON_MODIFY` flag to Makevars

Adds a flag to Makevars which enables copy-on-modify behaviour like in
R. This ensures you can never modify object B's data by modifying object
A (unless you do it manually via `r_vec::data()`).

The default behaviour is that `r_vec::set()` always modifies-in-place
with no checks to shared or referenced objects. This default behaviour
is generally much faster.

### Parallelisation

Adding this global flag effectively kills most parallelisation. To
achieve copy-on-modify we have to check whether the `r_sexp` object
being modified is referenced anywhere else at each and every call to
`set()`. This check (via `r_sexp::is_exclusive()`) is not thread-safe
and therefore most cppally parallelisation is disabled once
CPPALLY_COPY_ON_MODIFY is set.

## Usage

``` r
use_copy_on_modify()
```

## Value

Invisibly adds the `CPPALLY_COPY_ON_MODIFY` flag to Makevars.
