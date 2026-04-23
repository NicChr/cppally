#' Adds the CPPALLY_PRESERVE_ALTREP flag to Makevars
#'
#' @description
#' Adds a flag to Makevars which enables lazy materialisation of
#' ALTREP vectors.
#'
#' @returns
#' Invisibly adds the CPPALLY_PRESERVE_ALTREP flag to Makevars.
#'
#' @export
use_preserve_altrep_flag <- function(){
  add_makevars_flag("PKG_CPPFLAGS", "-DCPPALLY_PRESERVE_ALTREP")
  cli::cli_bullets(c("v" = "Added CPPALLY_PRESERVE_ALTREP flag."))
}
