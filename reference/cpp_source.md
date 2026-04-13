# Compile C++20 code

cpp11-style helpers to compile cpp20 code outside of a cpp20-linked
package context.

`cpp_source()` compiles and loads a single C++ file for use in R, either
from an expression or a cpp file. This may include multiple C++
functions.

`cpp_eval()` evaluates a single C++ expression and returns the result.
For example `cpp_eval('get_threads()')` will run the C++ function
`cpp20::get_threads()` and return the number of OMP threads currently
set for use. `void` return is not supported in `cpp_eval()`.

## Usage

``` r
cpp_source(
  file,
  code = NULL,
  env = parent.frame(),
  clean = TRUE,
  quiet = TRUE,
  debug = FALSE,
  cxx_std = Sys.getenv("CXX_STD", "CXX20"),
  dir = tempfile()
)

cpp_eval(
  code,
  env = parent.frame(),
  clean = TRUE,
  quiet = TRUE,
  debug = FALSE,
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

- cxx_std:

  C++ standard to use. Should be \>= C++20.

- dir:

  Directory to store the source files. The default is a temporary
  directory via [`tempfile()`](https://rdrr.io/r/base/tempfile.html)
  which is removed when `clean = TRUE`.

## Value

`cpp_source()` invisibly compiles the C++ code and registers the
`[[cpp20::register]]` tagged functions to R.  
`cpp_eval()` returns the result of the evaluated C++ expression.

## Examples

``` r
library(cpp20)

cpp_source(code = '
  #include <cpp20_light.hpp>
  using namespace cpp20;

  // We included cpp20_light.hpp so
  // example runs faster and does not trigger R CMD check note
  // Include cpp20.hpp for all features in usual development

  [[cpp20::register]]
  r_dbl add(r_dbl x, r_dbl y){
    return x + y;
  }
', debug = TRUE)
add(1, 2)
#> [1] 3
add(2, NA)
#> [1] NA
```
