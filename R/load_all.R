#' A wrapper around `devtools::load_all()` specifically for cppally
#'
#' @param path Path to package.
#' @param debug Should package be built without optimisations?
#' Default is `FALSE` which builds with optimisations.
#' @param compile See `?pkgload::load_all`.
#' @param cppally_header Which header should be
#' included with the registered C++ code? The default is the full library
#' "cppally.hpp". Choose "cppally_light.hpp" for the lighter header, which may
#' provide quicker compile times, at the cost of less features.
#' @param ... Further arguments passed on to `pkgload::load_all()`
#'
#' @returns
#' Invisibly registers cppally tagged functions and compiles C++ code.
#'
#' @export
load_all <- function (path = ".", debug = FALSE, cppally_header = c("cppally.hpp", "cppally_light.hpp"), compile = NA, ...){
  stop_unless_installed(c("pkgload", "pkgbuild", "rstudioapi"))
  if (inherits(path, "package")) {
    path <- path$path
  }
  if (rstudioapi::hasFun("documentSaveAll")) {
    rstudioapi::documentSaveAll()
  }
  pkgload::load_all(path = path, debug = debug, compile = FALSE, quiet = TRUE, helpers = FALSE)
  if (isTRUE(compile)){
    pkgbuild::clean_dll(path = path)
  }
  cpp_register(path = path, cppally_header = cppally_header)
  if (is.na(compile)){
    pkgload::load_all(path = path, debug = debug, ...)
  } else {
    pkgload::load_all(path = path, debug = debug, compile = compile, ...)
  }
}
