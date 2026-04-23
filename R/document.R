#' A wrapper around `devtools::document()` to support cppally package development
#'
#' @param pkg See `?devtools::document`
#' @param roclets See `?devtools::document`
#' @param quiet See `?devtools::document`
#'
#' @returns
#' Invisibly updates roxygen documentation, compiles C++ code and exports cppally
#' tagged functions to R.
#'
#' @export
document <- function (pkg = ".", roclets = NULL, quiet = FALSE){

  stop_unless_installed(c("rstudioapi", "withr", "roxygen2", "pkgload", "fs", "devtools"))

  pkg <- devtools::as.package(pkg)

  if (!isTRUE(quiet)) {
    cli::cli_inform(c(i = "Updating {.pkg {pkg$package}} documentation"))
  }

  if (rstudioapi::hasFun("documentSaveAll")) {
    rstudioapi::documentSaveAll()
  }
  if (pkg$package == "roxygen2") {
    load_all(pkg$path, quiet = quiet)
  }
  if (quiet){
    output <- fs::file_temp()
    withr::defer(fs::file_delete(output))
    withr::local_output_sink(output)
  }
  withr::local_envvar(devtools::r_env_vars())
  roxygen2::roxygenise(pkg$path, roclets, load_code = function(path) {
    load_all(path, quiet = quiet, helpers = FALSE, attach_testthat = FALSE)$env
  })
  pkgload::dev_topic_index_reset(pkg$package)
  invisible()
}
