## cppally 1.2.0

Bug fixes, improvements and API changes, listed in NEWS.md.

This release fixes macOS x86_64 CRAN package check result errors by supplying fallback functions 
for some features that are missing from older libc++, e.g. `std::cmp_less_equal`.

## Test environments

* Local: Windows 11, R 4.6.0
* win-builder (R-release)
* macos-latest (R-release)
* windows-latest (R-release)
* ubuntu-latest (R-release/devel)
* R-hub clang22 (R-devel)
* R-hub gcc-asan (R-devel)
* R-hub clang-ubsan (R-devel)
* R-hub valgrind (R-devel)

## R CMD check results

0 errors | 0 warnings | 0 notes

## Additional testing

cppally is header-only and hence has no compiled code of its own. Its C++ unit
tests live in a separate package, cppallytest, which is not part of this submission:
https://github.com/NicChr/cppally/tree/main/cppallytest

cppallytest passes R CMD check on:

* Local: Windows 11, R 4.6.0
* macos-latest (R-release)
* windows-latest (R-release)
* ubuntu-latest (R-devel)
* ubuntu-latest (R 4.5.0)
* R-hub clang22 (R-devel)
* R-hub gcc-asan (R-devel)
* R-hub clang-ubsan (R-devel)
* R-hub valgrind (R-devel)
* R-hub m1-san (R-devel)
* R-hub macos-arm64 (R-devel)
