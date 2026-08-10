#' Adds the `CPPALLY_CHECK_FACTORS` flag to Makevars
#'
#' @description
#' Adds a flag to Makevars which enables stricter validation on factor levels
#' at the point of `r_factors` construction. This avoids creating `r_factors`
#' objects with invalid levels and avoiding potential R crashes.
#'
#' The default behaviour is NOT to validate factor levels, which is naturally
#' faster when calling C++ functions that take `r_factors` inputs.
#'
#' @param quiet `[logical(1)]` - Should messages be suppressed?
#' Default is `FALSE`.
#'
#' @returns
#' Invisibly adds the `CPPALLY_CHECK_FACTORS` flag to Makevars.
#'
#' @export
use_check_factors <- function(quiet = FALSE){
  add_makevars_flag("PKG_CPPFLAGS", "-DCPPALLY_CHECK_FACTORS", quiet = quiet)
}
