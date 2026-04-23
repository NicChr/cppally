add_makevars_flag <- function(var, value) {

  proj_path <- utils::getFromNamespace("proj_path", "usethis")
  makevars_path1 <- proj_path("src", "Makevars")
  makevars_path2 <- proj_path("src", "Makevars.win")

  if (file.exists(makevars_path1)) {
    lines1 <- brio::read_lines(makevars_path1)
  } else {
    lines1 <- character()
  }

  if (file.exists(makevars_path2)) {
    lines2 <- brio::read_lines(makevars_path2)
  } else {
    lines2 <- character()
  }


  pattern <- paste0("^\\s*", var, "\\s*[+:]?=")
  idx1 <- grep(pattern, lines1)
  idx2 <- grep(pattern, lines2)

  if (length(idx1) > 0) {
    if (!grepl(value, lines1[idx1[1]], fixed = TRUE)) {
      lines1[idx1[1]] <- paste(lines1[idx1[1]], value)
    }
  } else {
    lines1 <- c(lines1, paste(var, "=", value))
  }

  if (length(idx2) > 0) {
    if (!grepl(value, lines2[idx2[1]], fixed = TRUE)) {
      lines2[idx2[1]] <- paste(lines2[idx2[1]], value)
    }
  } else {
    lines2 <- c(lines2, paste(var, "=", value))
  }

  brio::write_lines(lines1, makevars_path1)
  brio::write_lines(lines2, makevars_path2)
}

#' Helper for developing packages with cppally
#'
#' @description
#' usethis style helper to add the necessary setup to a new package to help
#' users get started with writing C++ code.
#'
#' @returns
#' Invisibly sets up the necessary conditions for
#' developing a package with cppally.
#'
#' @export
use_cppally <- function(){
  stop_unless_installed(c("rlang", "usethis", "desc", "purrr", "brio", "cli", "rstudioapi"))
  proj_path <- utils::getFromNamespace("proj_path", "usethis")
  utils::getFromNamespace("check_is_package", "usethis")("use_cppally()")
  rlang::check_installed("cppally")
  utils::getFromNamespace("check_uses_roxygen", "usethis")("use_cppally()")
  utils::getFromNamespace("check_has_package_doc", "usethis")("use_cppally()")
  suppressMessages(utils::getFromNamespace("use_src", "usethis")())
  suppressMessages(utils::getFromNamespace("use_dependency", "usethis")("cppally", "LinkingTo"))
  cli::cli_bullets(c("v" = "Added cppally to LinkingTo field in DESCRIPTION."))
  desc <- desc::desc()
  cli::cli_bullets(c("v" = "Added C++20 to SystemRequirements field in DESCRIPTION."))
  desc$set(SystemRequirements = "C++20")
  desc$write()

  ns_path <- proj_path("NAMESPACE")
  pkg_name <- utils::getFromNamespace("project_name", "usethis")()
  ns_entry <- paste0("useDynLib(", pkg_name, ", .registration = TRUE)")
  if (file.exists(ns_path)) {
    ns_lines <- brio::read_lines(ns_path)
  } else {
    ns_lines <- character()
  }
  if (!any(grepl(paste0("useDynLib(", pkg_name), ns_lines, fixed = TRUE))) {
    brio::write_lines(c(ns_lines, ns_entry), ns_path)
  }
  cli::cli_bullets(c("v" = "Added {ns_entry} to NAMESPACE."))

  # Add OPENMP flags to Makevars
  add_makevars_flag("PKG_CXXFLAGS", "$(SHLIB_OPENMP_CXXFLAGS)")
  add_makevars_flag("PKG_LIBS", "$(SHLIB_OPENMP_CXXFLAGS)")
  cli::cli_bullets(c("v" = "Added OMP Makevars flags."))

  # Generate code examples
  generate_cpp_regular_example()
  generate_cpp_template_example()

  cli::cli_bullets(c("v" = "Generated code examples in src/code.cpp and src/code.h"))

  cli::cli_bullets(c(
    "Please run {.run cppally::document()} to finish setup",
    "For continuous development please use {.run cppally::load_all()} and {.run cppally::document()}"
  ))

  # Re-open package doc so editor shows the @useDynLib tag added by use_src()
  pkg_name <- utils::getFromNamespace("project_name", "usethis")()
  pkg_doc <- proj_path("R", paste0(pkg_name, "-package.R"))
  if (file.exists(pkg_doc) && rstudioapi::hasFun("navigateToFile")) {
    rstudioapi::navigateToFile(pkg_doc)
  }

  invisible()
}
