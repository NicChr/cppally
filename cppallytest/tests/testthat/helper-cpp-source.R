# Shared skip guard for tests that compile throwaway units via cpp_source().
# cpp_source() (through its cpp_decorations() dependency) needs a compiler
# toolchain plus these packages; skip cleanly rather than error when any is
# unavailable. testthat auto-loads helper-*.R before running the test files.
skip_if_cannot_cpp_source <- function(){
  testthat::skip_on_cran()
  for (pkg in c("brio", "callr", "cli", "decor", "desc",
                "glue", "purrr", "readr", "stringr", "vctrs")){
    testthat::skip_if_not_installed(pkg)
  }
}
