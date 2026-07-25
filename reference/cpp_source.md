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
  cxx_std = Sys.getenv("CXX_STD", "CXX20"),
  cppally_header = c("cppally.hpp", "cppally_light.hpp")
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

## Examples
