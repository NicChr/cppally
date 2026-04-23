#' A wrapper around `devtools::load_all()` specifically for cppally
#'
#' @param path Path to package.
#' @param debug Should package be built without optimisations?
#' Default is `FALSE` which builds with optimisations.
#' @param ... Further arguments passed on to `pkgload::load_all()`
#'
#' @returns
#' Invisibly registers cppally tagged functions and compiles C++ code.
#'
#' @export
load_all <- function (path = ".", debug = FALSE, ...){
  stop_unless_installed(c("pkgload", "rstudioapi"))
  if (inherits(path, "package")) {
    path <- path$path
  }
  if (rstudioapi::hasFun("documentSaveAll")) {
    rstudioapi::documentSaveAll()
  }
  pkgload::load_all(path = path, debug = debug, compile = FALSE, quiet = TRUE, helpers = FALSE)
  cpp_register(path = path)
  pkgload::load_all(path = path, debug = debug, ...)
}
