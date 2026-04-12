# Compile C++20 code

cpp11-style helpers to compile cpp20 code outside of a cpp20-linked
package `cpp_source()` compiles and loads a single C++ for use in R,
either from an expression or a cpp file. `cpp_eval()` evaluates a single
C++ expressions and returns the result. `void` return is not supported
in `cpp_eval()`.

## Usage

``` r
cpp_source(
  file,
  code = NULL,
  env = parent.frame(),
  clean = TRUE,
  quiet = TRUE,
  cxx_std = Sys.getenv("CXX_STD", "CXX20"),
  dir = tempfile()
)

cpp_eval(
  code,
  env = parent.frame(),
  clean = TRUE,
  quiet = TRUE,
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

- cxx_std:

  C++ standard to use. Should be \>= C++20.

- dir:

  Directory to store the source files. The default is a temporary
  directory via [`tempfile()`](https://rdrr.io/r/base/tempfile.html)
  which is removed when `clean = TRUE`.

## Value

`cpp_source()` invisibly registers the `[[cpp20::register]]` tagged
functions to R.  
`cpp_eval()` returns the result of the evaluated C++ expression.

## Examples

``` r
library(cpp20)

# Expression returning the integer 0
cpp_eval("r_int(0)")
#> [1] 0

cpp_source(code = '
  #include <cpp20.hpp>
  using namespace cpp20;

  [[cpp20::register]]
  r_vec<r_str> unique_strs(r_vec<r_str> x){
    return unique(x);
  }
')
x <- sample(letters, 10^3, TRUE)
unique_strs(x)
#>  [1] "m" "w" "l" "e" "f" "o" "d" "i" "x" "b" "k" "u" "p" "c" "y" "v" "g" "j" "q"
#> [20] "t" "a" "s" "n" "r" "h" "z"
```
