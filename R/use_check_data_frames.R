#' Adds the `CPPALLY_CHECK_DATA_FRAMES` flag to Makevars
#'
#' @description
#' Adds a flag to Makevars which enables stricter validation on data frames
#' at the point of `r_df` construction. This ensures that column lengths are
#' always valid, avoiding potential R crashes downstream.
#'
#' The default behaviour is NOT to validate column lengths, enabling faster
#' `r_df` creating from `SEXP`.
#'
#' @param quiet `[logical(1)]` - Should messages be suppressed?
#' Default is `FALSE`.
#'
#' @returns
#' Invisibly adds the `CPPALLY_CHECK_DATA_FRAMES` flag to Makevars.
#'
#' @export
use_check_data_frames <- function(quiet = FALSE){
  add_makevars_flag("PKG_CPPFLAGS", "-DCPPALLY_CHECK_DATA_FRAMES", quiet = quiet)
}
