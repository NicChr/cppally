#' Deprecated.
#'
#' @description#
#' Deprecated, do not use.
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
