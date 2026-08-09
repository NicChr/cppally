# Removes every token in `var` starting with `prefix`, then appends `value`.
# Repeated calls must not leave two conflicting -D definitions of the same
# macro. A NULL `value` just removes the flag.
set_makevars_flag <- function(var, prefix, value) {

  proj_path <- utils::getFromNamespace("proj_path", "usethis")
  paths <- c(proj_path("src", "Makevars"), proj_path("src", "Makevars.win"))
  pattern <- stringr::str_c("^\\s*", var, "\\s*[+:]?=")

  for (path in paths) {

    file_exists <- file.exists(path)

    # Nothing to add and no file to update - don't create an empty Makevars
    if (!file_exists && is.null(value)) {
      next
    }

    if (file_exists) {
      lines <- brio::read_lines(path)
    } else {
      lines <- character()
    }

    idx <- stringr::str_which(lines, pattern)

    if (length(idx) > 0) {
      tokens <- stringr::str_split_1(lines[idx[1]], "[ \t]+")
      # fixed() because the prefix is a literal flag, not a pattern
      tokens <- tokens[!stringr::str_starts(tokens, stringr::fixed(prefix))]
      lines[idx[1]] <- stringr::str_flatten(c(tokens, value), collapse = " ")
    } else {
      lines <- c(lines, stringr::str_c(var, " = ", value))
    }

    brio::write_lines(lines, path)
  }
}

# A flag with no value part is its own prefix, so adding one is just setting it
add_makevars_flag <- function(var, value) {
  set_makevars_flag(var, prefix = value, value = value)
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
  stop_unless_installed("cppally")
  d <- desc::desc()
  has_roxygen <- !is.null(d$get("RoxygenNote")[[1]]) || !is.null(d$get("Config/roxygen2/version")[[1]])
  if (!has_roxygen) {
    cli::cli_abort(c(
      "x" = "Package does not appear to use roxygen2.",
      "i" = "Add {.code Roxygen: list(markdown = TRUE)} to DESCRIPTION, then run {.run devtools::document()}."
    ))
  }
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
