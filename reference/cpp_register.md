# Generates wrappers for registered C++ functions

Register C++ functions to be callable from R. C++ functions decorated
with `[[cppally::register]]` will be registered (including template
functions).

## Usage

``` r
cpp_register(
  path = ".",
  quiet = !is_interactive(),
  extension = c(".cpp", ".cc"),
  cppally_header = c("cppally.hpp", "cppally_light.hpp")
)
```

## Arguments

- path:

  Path to package root directory.

- quiet:

  If `TRUE` suppresses output from this function.

- extension:

  The file extension to use for the generated src/cppally file. Options
  are either '.cpp' (the default) or '.cc'.

- cppally_header:

  Which header should be included with the registered C++ code? The
  default is the full library "cppally.hpp". Choose "cppally_light.hpp"
  for the lighter header, which may provide quicker compile times, at
  the cost of less features.

## Value

The paths to the generated R and C++ source files.

## See also

[cpp_source](https://nicchr.github.io/cppally/reference/cpp_source.md)
