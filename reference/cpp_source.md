# Compile C++20 code

cpp11-style helpers to compile cppally code outside of a cppally-linked
package context.

`cpp_source()` compiles and loads a single C++ file for use in R, either
from an expression or a cpp file. This may include multiple C++
functions.

`cpp_eval()` evaluates a single C++ expression and returns the result.
For example `cpp_eval('get_threads()')` will run the C++ function
`cppally::get_threads()` and return the number of OMP threads currently
set for use. For expressions that do not return a result, the call is
evaluated and `NULL` is returned invisibly.

## Usage

``` r
cpp_source(
  file = NULL,
  code = NULL,
  env = parent.frame(),
  clean = TRUE,
  quiet = TRUE,
  debug = FALSE,
  preserve_altrep = FALSE,
  check_factors = FALSE,
  check_data_frames = FALSE,
  copy_on_modify = FALSE,
  openmp = TRUE,
  cxx_std = Sys.getenv("CXX_STD", "CXX20"),
  dir = tempfile(),
  ...
)

cpp_eval(
  code,
  env = curr_env(),
  clean = TRUE,
  quiet = TRUE,
  debug = FALSE,
  preserve_altrep = FALSE,
  check_factors = FALSE,
  check_data_frames = FALSE,
  copy_on_modify = FALSE,
  openmp = TRUE,
  simplify = TRUE,
  cxx_std = Sys.getenv("CXX_STD", "CXX20"),
  cppally_header = c("cppally.hpp", "cppally_light.hpp"),
  ...
)
```

## Arguments

- file:

  C++ file.

- code:

  For `cpp_source()` - If `file` is `NULL` then a string of C++ code to
  compile. This can include the contents of a cpp file which can contain
  multiple `[[cppally::register]]` tagged functions. For `cpp_eval` -
  This can be a character vector of single-line expressions.

- env:

  Environment where R functions should be defined.

- clean:

  Should files be cleaned up after sourcing? Default is `TRUE`.

- quiet:

  Should compiler output be suppressed? Default is `TRUE`.

- debug:

  Should C++ code be compiled in a debug build? Default is `FALSE`.

- preserve_altrep:

  Should ALTREP vectors be preserved by avoiding materialisation where
  possible? Default is `FALSE`.

- check_factors:

  Should factor levels be validated when using `r_factors` objects?
  Default is `FALSE`. When `TRUE`, factor levels are checked once on
  `r_factors` construction to ensure they are valid, reducing the chance
  of R crashing when passing factors with invalid levels.

- check_data_frames:

  Should data frames be validated when constructing `r_df` objects from
  `SEXP`? Default is `FALSE`.

- copy_on_modify:

  Should copy-on-modify be used everywhere? Default is `FALSE`.

- openmp:

  Should code be compiled with OpenMP flags? Default is `TRUE`.

- cxx_std:

  C++ standard to use. Should be \>= C++20.

- dir:

  Directory to store the source files. The default is a temporary
  directory via [`tempfile()`](https://rdrr.io/r/base/tempfile.html)
  which is removed when `clean = TRUE`.

- ...:

  Further arguments passed to
  [`use_template_dispatch_candidates()`](https://nicchr.github.io/cppally/reference/use_template_dispatch_candidates.md).

- simplify:

  Applies to `cpp_eval`. A list of results is returned unless
  `length(code) == 1` and `simplify = TRUE`.

- cppally_header:

  Which header should be included with the registered C++ code? The
  default is the full library "cppally.hpp". Choose "cppally_light.hpp"
  for the lighter header, which may provide quicker compile times, at
  the cost of less features.

## Value

`cpp_source()` invisibly compiles the C++ code and registers the
`[[cppally::register]]` tagged functions to R.  
`cpp_eval()` returns the results of the evaluated C++ expressions.

## See also

[cpp_register](https://nicchr.github.io/cppally/reference/cpp_register.md)
[use_template_dispatch_candidates](https://nicchr.github.io/cppally/reference/use_template_dispatch_candidates.md)

## Examples

``` r

library(cppally)
library(bit64)
#> ********************************************************
#> R-core is collecting use cases for 64-bit integers as they explore native support for these vectors.
#> 
#> See https://stat.ethz.ch/pipermail/r-devel/2026-July/084631.html and reach out to Luke Tierney (luke-tierney@uiowa.edu).
#> ********************************************************
#> 
#> Attaching package: ‘bit64’
#> The following object is masked from ‘package:utils’:
#> 
#>     hashtab
#> The following objects are masked from ‘package:base’:
#> 
#>     %in%, :, array, as.factor, as.ordered, colSums, factor, intersect,
#>     is.double, is.element, match, matrix, order, rank, rowSums,
#>     setdiff, setequal, table, union

cpp_eval <- function(...){
  # We don't need the full cppally header for these examples
  cppally::cpp_eval(..., cppally_header = "cppally_light.hpp")
}

# \donttest{
cpp_eval('print("hello world!")')
#> hello world!

# Default values of all cppally scalars
cpp_eval(c(
  'r_lgl()',
  'r_int()',
  'r_dbl()',
  'r_int64()',
  'r_str()',
  'r_raw()',
  'r_cplx()',
  'r_date()',
  'r_psxct()'
))
#> $res1
#> [1] FALSE
#> 
#> $res2
#> [1] 0
#> 
#> $res3
#> [1] 0
#> 
#> $res4
#> integer64
#> [1] 0
#> 
#> $res5
#> [1] ""
#> 
#> $res6
#> [1] 00
#> 
#> $res7
#> [1] 0+0i
#> 
#> $res8
#> [1] "1970-01-01"
#> 
#> $res9
#> [1] "1970-01-01 UTC"
#> 

cpp_source(code = '
  #include <cppally.hpp>
  using namespace cppally;

  [[cppally::register]]
  r_dbl add(r_dbl x, r_dbl y){
    return x + y;
  }
', debug = TRUE)
add(1, 2)
#> [1] 3
add(2, NA)
#> [1] NA

### ALTREP

# cppally also supports lazy ALTREP materialisation as an opt-in feature.
# To opt-in, set `preserve_altrep = TRUE`

cpp_source(
  code = '
  #include <cppally_light.hpp>
  using namespace cppally;

  [[cppally::register]]
  r_int last_altrep_unaware(r_vec<r_int> x){
    r_int out;
    r_size_t n = x.length();

    if (n > 0){
      out = x.get(n - 1);
    }
    return out;
  }
', debug = TRUE
)

cpp_source(
  code = '
  #include <cppally_light.hpp>
  using namespace cppally;

  [[cppally::register]]
  r_int last_altrep_aware(r_vec<r_int> x){
    r_int out;
    r_size_t n = x.length();

    if (n > 0){
      out = x.get(n - 1);
    }
    return out;
  }
', debug = TRUE,
  preserve_altrep = TRUE
)

library(bench)
mark(last_altrep_aware(1:10^5)) # No materialisation
#> # A tibble: 1 × 13
#>   expression      min median `itr/sec` mem_alloc `gc/sec` n_itr  n_gc total_time
#>   <bch:expr>   <bch:> <bch:>     <dbl> <bch:byt>    <dbl> <int> <dbl>   <bch:tm>
#> 1 last_altrep… 3.95µs  4.7µs   193436.    3.18KB        0 10000     0     51.7ms
#> # ℹ 4 more variables: result <list>, memory <list>, time <list>, gc <list>
mark(last_altrep_unaware(1:10^5)) # Materialises full vector
#> # A tibble: 1 × 13
#>   expression      min median `itr/sec` mem_alloc `gc/sec` n_itr  n_gc total_time
#>   <bch:expr>   <bch:> <bch:>     <dbl> <bch:byt>    <dbl> <int> <dbl>   <bch:tm>
#> 1 last_altrep… 36.9µs 38.2µs    22570.     391KB     182.  3844    31      170ms
#> # ℹ 4 more variables: result <list>, memory <list>, time <list>, gc <list>

### Copy-on-modify

# cppally supports copy-on-modify as an opt-in feature
# It is disabled by default because it incurs a major performance penalty
# and has been deemed not worth it even for the safety benefits
# That being said, if you prefer absolute safety over speed then you can
# enable it globally via `cppally::use_copy_on_modify()` or
# via the arg `copy_on_modify` if  using `cpp_source()`

cpp_source(
  code = '
  #include <cppally_light.hpp>
  using namespace cppally;

  [[cppally::register]]
  r_vec<r_int> reverse(r_vec<r_int> x){
    x.rev(); // in-place reverse
    return x;
  }
', copy_on_modify = TRUE
)

x <- c(1L, 2L, 3L)
reverse(x)
#> [1] 3 2 1
x # x was preserved and not updated by reference (as expected)
#> [1] 1 2 3

x <- sample.int(10^5)
mark(reverse(x)) # Memory allocated, therefore x was copied before reversing
#> # A tibble: 1 × 13
#>   expression      min median `itr/sec` mem_alloc `gc/sec` n_itr  n_gc total_time
#>   <bch:expr> <bch:tm> <bch:>     <dbl> <bch:byt>    <dbl> <int> <dbl>   <bch:tm>
#> 1 reverse(x)    229µs  248µs     3867.     391KB     30.8  1759    14      455ms
#> # ℹ 4 more variables: result <list>, memory <list>, time <list>, gc <list>

# The cppally preferred approach is to allocate a fresh vector or copy the
# existing vector
cpp_source(
  code = '
  #include <cppally_light.hpp>
  using namespace cppally;

  [[cppally::register]]
  r_vec<r_int> cppally_reverse(r_vec<r_int> x){
    r_vec<r_int> out = x.copy();
    out.rev();
    return out;
  }
', copy_on_modify = FALSE
)

mark(
  r_reverse = rev(x),
  cppally_copy_on_modify_reverse = reverse(x),
  cppally_no_copy_on_modify_reverse = cppally_reverse(x)
)
#> # A tibble: 3 × 13
#>   expression      min median `itr/sec` mem_alloc `gc/sec` n_itr  n_gc total_time
#>   <bch:expr>  <bch:t> <bch:>     <dbl> <bch:byt>    <dbl> <int> <dbl>   <bch:tm>
#> 1 r_reverse     205µs  223µs     4228.     781KB     68.1  1180    19      279ms
#> 2 cppally_co… 242.5µs  248µs     3970.     391KB     30.7  1810    14      456ms
#> 3 cppally_no…  56.3µs  203µs     6271.     391KB     47.1  2529    19      403ms
#> # ℹ 4 more variables: result <list>, memory <list>, time <list>, gc <list>

### Speeding up template-heavy compilation

# When writing C++ code that only ever uses a small subset of cppally types,
# we can restrict cppally template dispatch to only ever consider these types,
# potentially speeding up compilation times. Only do this if you are certain
# that these are the only viable types your code will ever reasonably accept.
# If you are unsure, then do not use this feature and instead use
# C++ concepts and template constraints.

# Example: restrict template dispatch on `r_int` and `r_dbl`
# `cppally::unique()` is templated on RComposite, which accepts all R vectors,
# factors, and data frames. Therefore writing our own template which depends
# on `unique()` being available for any generic type, might get expensive if say we
# are only interested in integers and doubles.

mark(

  unrestricted = cpp_source(
    code = '
    #include <cppally.hpp>
    using namespace cppally;

    template <RVector T>
    requires (requires (T obj){ unique(obj); })
    [[cppally::register]]
    T sorted_unique(T x){
      return unique(x, /*sort = */ true );
    }
  ',
    debug = TRUE
  ),
  restricted = cpp_source(
    code = '
    #include <cppally.hpp>
    using namespace cppally;

    template <RVector T>
    requires (requires (T obj){ unique(obj); })
    [[cppally::register]]
    T sorted_unique2(T x){
      return unique(x, /*sort = */ true );
    }
  ',
    debug = TRUE,
    scalar_types = c("r_int", "r_dbl"),
    r_sexp = FALSE,
    data_frames = FALSE,
    factors = FALSE
  ),
  check = FALSE,
  memory = FALSE,
  iterations = 1
)
#> # A tibble: 2 × 13
#>   expression      min median `itr/sec` mem_alloc `gc/sec` n_itr  n_gc total_time
#>   <bch:expr>   <bch:> <bch:>     <dbl> <bch:byt>    <dbl> <int> <dbl>   <bch:tm>
#> 1 unrestricted  8.71s  8.71s     0.115        NA        0     1     0      8.71s
#> 2 restricted    5.64s  5.64s     0.177        NA        0     1     0      5.64s
#> # ℹ 4 more variables: result <list>, memory <list>, time <list>, gc <list>

sorted_unique(c(1, 1, 2, 2, 3, 3))
#> [1] 1 2 3
sorted_unique2(c(1, 1, 2, 2, 3, 3))
#> [1] 1 2 3

sorted_unique(c("A", "A", "B", "B", "C", "C"))
#> [1] "A" "B" "C"

# Expected to fail
try(sorted_unique2(c("A", "A", "B", "B", "C", "C")))
#> Error : Argument 1 is of R type character, which this package excludes from its dispatch candidates. Restore it with `use_template_dispatch_candidates()`

# In production code and regular usage,
# you would call `use_template_dispatch_candidates(c("r_int", "r_dbl"))`
# which will add the relevant flag to your Makevars file(s).
# }
rm(cpp_eval)
```
