# Compile C++20 code

cpp11-style helpers to compile cppally code outside of a cppally-linked
package context.

`cpp_source()` compiles and loads a single C++ file for use in R, either
from an expression or a cpp file. This may include multiple C++
functions.

`cpp_eval()` evaluates a single C++ expression and returns the result.
For example `cpp_eval('get_threads()')` will run the C++ function
`cppally::get_threads()` and return the number of OMP threads currently
set for use. For expressions no return result, the call is evaluated and
returns `NULL` invisibly.

## Usage

``` r
cpp_source(
  file,
  code = NULL,
  env = parent.frame(),
  clean = TRUE,
  quiet = TRUE,
  debug = FALSE,
  preserve_altrep = FALSE,
  check_factors = FALSE,
  check_data_frames = FALSE,
  copy_on_modify = FALSE,
  cxx_std = Sys.getenv("CXX_STD", "CXX20"),
  dir = tempfile()
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
  simplify = TRUE,
  cxx_std = Sys.getenv("CXX_STD", "CXX20")
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

- cxx_std:

  C++ standard to use. Should be \>= C++20.

- dir:

  Directory to store the source files. The default is a temporary
  directory via [`tempfile()`](https://rdrr.io/r/base/tempfile.html)
  which is removed when `clean = TRUE`.

- simplify:

  Applies to `cpp_eval`. A list of results is returned unless
  `length(code) == 1` and `simplify = TRUE`.

## Value

`cpp_source()` invisibly compiles the C++ code and registers the
`[[cppally::register]]` tagged functions to R.  
`cpp_eval()` returns the results of the evaluated C++ expressions.

## See also

[cpp_register](https://nicchr.github.io/cppally/reference/cpp_register.md)

## Examples

``` r

library(cppally)
library(bit64)
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

### ALTREP ###

# cppally also supports lazy ALTREP materialisation as an opt-in feature.
# To opt-in, set `preserve_altrep = TRUE`

cpp_source(
  code = '
  #include <cppally.hpp>
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
  #include <cppally.hpp>
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
#> 1 last_altrep… 2.98µs  4.4µs   222343.    3.18KB        0 10000     0       45ms
#> # ℹ 4 more variables: result <list>, memory <list>, time <list>, gc <list>
mark(last_altrep_unaware(1:10^5)) # Materialises full vector
#> # A tibble: 1 × 13
#>   expression      min median `itr/sec` mem_alloc `gc/sec` n_itr  n_gc total_time
#>   <bch:expr>   <bch:> <bch:>     <dbl> <bch:byt>    <dbl> <int> <dbl>   <bch:tm>
#> 1 last_altrep… 31.2µs 31.9µs    26057.     391KB     207.  4159    33      160ms
#> # ℹ 4 more variables: result <list>, memory <list>, time <list>, gc <list>

### Copy-on-modify ###

# cppally supports copy-on-modify as an opt-in feature
# It is disabled by default because it incurs a major performance penalty
# and has been deemed not worth it even for the safety benefits
# That being said, if you prefer absolute safety over speed then you can
# enable it globally via `cppally::use_copy_on_modify()` or
# via the arg `copy_on_modify` if  using `cpp_source()`

cpp_source(
  code = '
  #include <cppally.hpp>
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
#> 1 reverse(x)    342µs  372µs     2600.     391KB     21.4  1215    10      467ms
#> # ℹ 4 more variables: result <list>, memory <list>, time <list>, gc <list>

# The cppally preferred approach is to allocate a fresh vector or copy the
# existing vector
cpp_source(
  code = '
  #include <cppally.hpp>
  using namespace cppally;

  [[cppally::register]]
  r_vec<r_int> cppally_reverse(r_vec<r_int> x){
    r_vec<r_int> out = shallow_copy(x);
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
#>   expression     min  median `itr/sec` mem_alloc `gc/sec` n_itr  n_gc total_time
#>   <bch:expr> <bch:t> <bch:t>     <dbl> <bch:byt>    <dbl> <int> <dbl>   <bch:tm>
#> 1 r_reverse  177.6µs 186.3µs     5141.     781KB     83.8  1473    24      287ms
#> 2 cppally_c… 341.4µs 371.8µs     2666.     391KB     19.1  1257     9      471ms
#> 3 cppally_n…  58.2µs  62.4µs     9211.     391KB     73.6  3628    29      394ms
#> # ℹ 4 more variables: result <list>, memory <list>, time <list>, gc <list>
# }
```
