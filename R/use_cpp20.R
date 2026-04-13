add_makevars_omp_flag <- function(lines, var) {
  pattern <- paste0("^\\s*", var, "\\s*[+:]?=")
  idx <- grep(pattern, lines)
  if (length(idx) > 0) {
    if (!grepl("$(SHLIB_OPENMP_CXXFLAGS)", lines[idx[1]], fixed = TRUE)) {
      lines[idx[1]] <- paste(lines[idx[1]], "$(SHLIB_OPENMP_CXXFLAGS)")
    }
  } else {
    lines <- c(lines, paste(var, "=", "$(SHLIB_OPENMP_CXXFLAGS)"))
  }
  lines
}

#' Helper for developing packages with cpp20
#'
#' @description
#' usethis style helper to add the necessary setup to a new package to help
#' users get started with writing C++ code.
#'
#' @returns
#' Invisibly sets up the necessary conditions for
#' developing a package with cpp20.
#'
#' @export
use_cpp20 <- function (){
  stop_unless_installed(c("rlang", "usethis", "desc", "purrr", "brio", "cli", "rstudioapi"))
  proj_path <- utils::getFromNamespace("proj_path", "usethis")
  utils::getFromNamespace("check_is_package", "usethis")("use_cpp20()")
  rlang::check_installed("cpp20")
  utils::getFromNamespace("check_uses_roxygen", "usethis")("use_cpp20()")
  utils::getFromNamespace("check_has_package_doc", "usethis")("use_cpp20()")
  suppressMessages(utils::getFromNamespace("use_src", "usethis")())
  suppressMessages(utils::getFromNamespace("use_dependency", "usethis")("cpp20", "LinkingTo"))
  cli::cli_bullets(c("v" = "Added cpp20 to LinkingTo field in DESCRIPTION."))
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
  makevars_path <- proj_path("src", "Makevars")
  if (file.exists(makevars_path)) {
    lines <- brio::read_lines(makevars_path)
  } else {
    lines <- character()
  }
  lines <- add_makevars_omp_flag(lines, "PKG_CXXFLAGS")
  lines <- add_makevars_omp_flag(lines, "PKG_LIBS")
  brio::write_lines(lines, makevars_path)

  cli::cli_bullets(c("v" = "Added OMP Makevars flags."))

  # Generate code examples
  generate_cpp_regular_example()
  generate_cpp_template_example()

  cli::cli_bullets(c("v" = "Generated code examples in src/code.cpp and src/code.h"))

  cli::cli_bullets(c(
    "Please run {.run cpp20::document()} to finish setup",
    "For continuous development please use {.run cpp20::load_all()} and {.run cpp20::document()}"
  ))

  # Re-open package doc so editor shows the @useDynLib tag added by use_src()
  pkg_name <- utils::getFromNamespace("project_name", "usethis")()
  pkg_doc <- proj_path("R", paste0(pkg_name, "-package.R"))
  if (file.exists(pkg_doc) && rstudioapi::hasFun("navigateToFile")) {
    rstudioapi::navigateToFile(pkg_doc)
  }

  invisible()
}
