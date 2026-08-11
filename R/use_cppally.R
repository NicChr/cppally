
cppally_bullets <- function(text, quiet = FALSE, .envir = parent.frame()) {
  if (!quiet) {
    cli::cli_bullets(text, .envir = .envir)
  }
  invisible()
}

makevars_paths <- function() {
  c(usethis::proj_path("src", "Makevars"), usethis::proj_path("src", "Makevars.win"))
}

makevars_var_pattern <- function(variable) {
  stringr::str_c("^(\\s*", variable, "\\s*[+:]?=)(.*)$")
}

# Modifies the RHS of a makevars variable
set_makevars_value <- function(variable, prefix, value, op = "=", quiet = FALSE) {

  pattern <- makevars_var_pattern(variable)

  if (is.null(prefix)){
    prefix <- ""
  }

  for (path in makevars_paths()) {

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
      parts <- stringr::str_match(lines[idx[1]], pattern)
      assignment <- stringr::str_trim(parts[, 2])
      current <- stringr::str_trim(parts[, 3])

      if (current != "") {
        values <- stringr::str_split_1(current, "[ \t]+")
      } else {
        values <- character()
      }

      if (prefix != "") {
        # fixed() because the prefix is a literal value, not a pattern
        values <- values[!stringr::str_starts(values, stringr::fixed(prefix))]
      } else {
        # An empty prefix means "match everything". stringi rejects an empty
        # fixed() pattern and returns NA rather than TRUE, which would end up
        # written into the file as the literal text NA
        values <- character()
      }

      values <- c(values, value)

      if (length(values) == 0) {
        # Removing the last value leaves a bare `VAR =`
        lines <- lines[-idx[1]]
      } else {
        lines[idx[1]] <- stringr::str_flatten(c(assignment, values), collapse = " ")
      }
    } else if (!is.null(value)) {
      lines <- c(lines, stringr::str_c(variable, " ", op, " ", value))
    } else {
      # Nothing to remove and nothing to add - leave the file untouched
      next
    }
    brio::write_lines(lines, path)
  }

  if (is.null(value) && prefix == "") {
    cppally_bullets(c("v" = "Cleared {variable}."), quiet = quiet)
  } else if (is.null(value)) {
    cppally_bullets(c("v" = "Removed {prefix} from {variable}."), quiet = quiet)
  } else if (!nzchar(prefix)) {
    cppally_bullets(c("v" = "Set {variable} to {value}."), quiet = quiet)
  } else {
    cppally_bullets(c("v" = "Added {value} to {variable}."), quiet = quiet)
  }
}

# A value with no value part of its own is its own prefix, so adding one is
# just setting it
add_makevars_flag <- function(variable, value, quiet = FALSE) {
  set_makevars_value(variable, prefix = value, value = value, quiet = quiet)
}

# Deletes the whole assignment rather than one value from it
remove_makevars_variable <- function(variable, quiet = FALSE) {
  set_makevars_value(variable, prefix = "", value = NULL, quiet = quiet)
}

# CXX_STD is single-valued, so an empty prefix clears whatever is there first
use_cxx_std <- function(cxx_std = "CXX20", quiet = FALSE) {
  set_makevars_value("CXX_STD", prefix = "", value = cxx_std, quiet = quiet)
}

# `override` is needed to beat R's own CXXFLAGS, and `+=` keeps the rest of them
# rather than replacing the lot. The "-O" prefix clears any existing -O level.
use_debug <- function(quiet = FALSE) {
  set_makevars_value(
    "override CXXFLAGS", prefix = "-O", value = "-O0", op = "+=", quiet = quiet
  )
}

# Hide all symbols by default
use_symbol_visibility <- function(quiet = FALSE) {
  add_makevars_flag("PKG_CXXFLAGS", "$(CXX_VISIBILITY)", quiet = quiet)
}

#' Helper for developing packages with cppally
#'
#' @description
#' usethis style helper to add the necessary setup to a new package to help
#' users get started with writing C++ code.
#'
#' @param quiet `[logical(1)]` - Should messages be suppressed?
#' Default is `FALSE`.
#'
#' @returns
#' Invisibly sets up the necessary conditions for
#' developing a package with cppally.
#'
#' @export
use_cppally <- function(quiet = FALSE){
  stop_unless_installed(c("usethis", "desc", "purrr", "brio", "cli", "rstudioapi"))
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
  cppally_bullets(c("v" = "Added cppally to LinkingTo field in DESCRIPTION."), quiet = quiet)
  desc <- desc::desc()
  cppally_bullets(c("v" = "Added C++20 to SystemRequirements field in DESCRIPTION."), quiet = quiet)
  desc$set(SystemRequirements = "C++20")
  desc$write()

  ns_path <- usethis::proj_path("NAMESPACE")
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
  cppally_bullets(c("v" = "Added {ns_entry} to NAMESPACE."), quiet = quiet)

  use_openmp(quiet = quiet)
  use_symbol_visibility(quiet = quiet)

  # Generate code examples
  generate_cpp_regular_example()
  generate_cpp_template_example()

  cppally_bullets(c("v" = "Generated code examples in src/code.cpp and src/code.h"), quiet = quiet)

  cppally_bullets(c(
    "Please run {.run cppally::document()} to finish setup",
    "For continuous development please use {.run cppally::load_all()} and {.run cppally::document()}"
  ), quiet = quiet)

  # Re-open package doc so editor shows the @useDynLib tag added by use_src()
  pkg_name <- utils::getFromNamespace("project_name", "usethis")()
  pkg_doc <- usethis::proj_path("R", paste0(pkg_name, "-package.R"))
  if (file.exists(pkg_doc) && rstudioapi::hasFun("navigateToFile")) {
    rstudioapi::navigateToFile(pkg_doc)
  }

  invisible()
}
