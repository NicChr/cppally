# Restrict the R/C++ types a template function will dispatch on

Use this when:

- You want to speed up compilation time.

- Your code only ever will accept a subset of types.

For example, let's say you are writing a C++ matrix algebra library
using cppally and you are only interested in working with integers and
doubles. Because the only types your library accepts are based on these
two types, you could restrict all template accepted types to `r_int` and
`r_dbl` (and equivalently `r_vector<r_int>` and `r_vector<r_dbl>`).
Doing this will dramatically improve development time by reducing
compile times and also reducing the size of your package dll.

Because cppally is a header-only library, templates you call from your
registered templated functions, including cppally's own, are
instantiated once per surviving candidate rather than once per default
candidate.

## Usage

``` r
use_template_dispatch_candidates(
  scalar_types = cppally_scalar_types,
  scalars = TRUE,
  vectors = TRUE,
  factors = TRUE,
  data_frames = TRUE,
  r_sexp = TRUE,
  quiet = FALSE
)
```

## Arguments

- scalar_types:

  `[character(n)]` - Template candidate scalar types to allow
  package-wide. Vector candidates are derived from this, e.g. `r_dbl`
  allows both scalar `r_dbl` and vector `r_vector<r_dbl>`. The default
  includes all cppally scalar types.

- scalars:

  `[logical(1)]` - Should bare scalar candidates be included? Default is
  `TRUE`. Scalars share a type mapping with their vector counterpart.
  Setting this to `FALSE` prevents scalars from being valid template
  arguments, but the vector equivalents will still be allowed.

- vectors:

  `[logical(1)]` - Should `r_vector<>` candidates be included? Default
  is `TRUE`.

- factors:

  `[logical(1)]` - Should `r_factors` be a candidate? Default is `TRUE`.

- data_frames:

  `[logical(1)]` - Should `r_df` be a candidate? Default is `TRUE`.

- r_sexp:

  `[logical(1)]` - Should `r_sexp` be a candidate? Default is `TRUE`.
  `r_sexp` is not a scalar type, so it has its own switch rather than
  belonging to `scalar_types`.

- quiet:

  `[logical(1)]` - Should messages be suppressed? Default is `FALSE`.

## Value

Invisibly adds dispatch candidate flags to Makevars.

## Details

Adds flags to Makevars restricting the set of R types the dispatcher
considers when calling a registered templated function.

The dispatch table holds `N^k` entries for `k` template parameters, so
the saving grows sharply with the number of template parameters on a
function.

Calling it with no arguments restores the defaults.

### This changes behaviour, not just build time

A type left out of the candidate set is no longer accepted at the R
boundary. Dropping `r_date` does not make a `Date` argument slower, it
makes it an error. The rejection is explicit and happens before
dispatch, so an excluded type is never quietly claimed by the catch-all
and reinterpreted as something else. The same source therefore behaves
differently depending on these flags, so narrow the set only to types
the package genuinely intends to support.

This does not affect the `r_sexp` visitor functions defined in
`r_visit.h`. Those dispatch over their own type list and continue to
handle every R type regardless of what is set here.
