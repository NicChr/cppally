#' Add OpenMP flags to Makevars
#'
#' @description
#' Adds the correct flags to Makevars, enabling SIMD vectorisation and
#' multi-threading via OpenMP.
#'
#' @param quiet `[logical(1)]` - Should messages be suppressed?
#' Default is `FALSE`.
#'
#' @returns
#' Invisibly adds the '$(SHLIB_OPENMP_CXXFLAGS)' value to the
#' Makevars variables 'PKG_LIBS' and 'PKG_CXXFLAGS', enabling multi-threading
#' and SIMD vectorisation via OpenMP.
#'
#' @export
use_openmp <- function(quiet = FALSE) {
  add_makevars_flag("PKG_CXXFLAGS", "$(SHLIB_OPENMP_CXXFLAGS)", quiet = quiet)
  add_makevars_flag("PKG_LIBS", "$(SHLIB_OPENMP_CXXFLAGS)", quiet = quiet)
}
