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
  cxx_std = Sys.getenv("CXX_STD", "CXX20"),
  dir = tempfile()
)

cpp_eval(
  code,
  env = parent.frame(),
  clean = TRUE,
  quiet = TRUE,
  debug = FALSE,
  preserve_altrep = FALSE,
  cxx_std = Sys.getenv("CXX_STD", "CXX20")
)
```

## Arguments

- file:

  C++ file.

- code:

  If `file` is `NULL` then a string of C++ code to compile.

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

- cxx_std:

  C++ standard to use. Should be \>= C++20.

- dir:

  Directory to store the source files. The default is a temporary
  directory via [`tempfile()`](https://rdrr.io/r/base/tempfile.html)
  which is removed when `clean = TRUE`.

## Value

`cpp_source()` invisibly compiles the C++ code and registers the
`[[cppally::register]]` tagged functions to R.  
`cpp_eval()` returns the result of the evaluated C++ expression.

## Examples

``` r
library(cppally)

# \donttest{
cpp_eval('print("hello world!")')
#> hello world!
cpp_eval('r_int(0)')
#> [1] 0
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
#>   <bch:expr>    <bch> <bch:>     <dbl> <bch:byt>    <dbl> <int> <dbl>   <bch:tm>
#> 1 last_altrep_… 1.3µs 1.41µs   557848.        0B     55.8  9999     1     17.9ms
#> # ℹ 4 more variables: result <list>, memory <list>, time <list>, gc <list>
mark(last_altrep_unaware(1:10^5)) # Materialises full vector
#> # A tibble: 1 × 13
#>   expression      min median `itr/sec` mem_alloc `gc/sec` n_itr  n_gc total_time
#>   <bch:expr>   <bch:> <bch:>     <dbl> <bch:byt>    <dbl> <int> <dbl>   <bch:tm>
#> 1 last_altrep… 43.6µs 47.5µs    19428.     391KB     156.  3979    32      205ms
#> # ℹ 4 more variables: result <list>, memory <list>, time <list>, gc <list>

# }
```
