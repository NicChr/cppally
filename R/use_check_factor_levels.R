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
#' @returns
#' Invisibly adds the `CPPALLY_CHECK_FACTORS` flag to Makevars.
#'
#' @export
use_check_factors <- function(){
  add_makevars_flag("PKG_CPPFLAGS", "-DCPPALLY_CHECK_FACTORS")
  cli::cli_bullets(c("v" = "Added CPPALLY_CHECK_FACTORS flag."))
}
