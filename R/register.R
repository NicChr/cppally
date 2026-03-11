
# Utils -------------------------------------------------------------------

utils::globalVariables(c("name", "return_type", "line", "decoration", "context", ".", "functions", "res"))

# All credits to cpp11 & decor authors and contributors

glue_collapse_data <- function(data, ..., sep = ", ", last = "") {
  res <- glue::glue_collapse(glue::glue_data(data, ...), sep = sep, last = last)
  if (length(res) == 0) {
    return("")
  }
  unclass(res)
}

viapply <- function(x, f, ...) vapply(x, f, integer(1), ...)
vcapply <- function(x, f, ...) vapply(x, f, character(1), ...)

stop_unless_installed <- function(pkgs) {
  has_pkg <- logical(length(pkgs))
  for (i in seq_along(pkgs)) {
    has_pkg[[i]] <- requireNamespace(pkgs[[i]], quietly = TRUE)
  }
  if (any(!has_pkg)) {
    msg <- sprintf(
      "The %s package(s) are required for this functionality",
      paste(pkgs[!has_pkg], collapse = ", ")
    )

    if (is_interactive()) {
      ans <- readline(paste(c(msg, "Would you like to install them? (y/N) "), collapse = "\n"))
      if (tolower(ans) == "y") {
        utils::install.packages(pkgs[!has_pkg])
        stop_unless_installed(pkgs)
        return()
      }
    }

    stop(msg, call. = FALSE)
  }
}

# This is basically the same as rlang::is_interactive(), which we can't really
# use for stop_if_not_installed(), because rlang itself could be one of the
# input pkgs.
is_interactive <- function() {
  opt <- getOption("rlang_interactive", NULL)
  if (!is.null(opt)) {
    return(opt)
  }
  if (isTRUE(getOption("knitr.in.progress"))) {
    return(FALSE)
  }
  if (isTRUE(getOption("rstudio.notebook.executing"))) {
    return(FALSE)
  }
  if (identical(Sys.getenv("TESTTHAT"), "true")) {
    return(FALSE)
  }
  interactive()
}

file_extension <- function(x){
  stringr::str_extract(x, "(\\.[^.]+)$")
}

is_header <- function(x){
  file_extension(x) %in% c(".h", ".hpp")
}

modified_time <- function(file){
  info <- file.info(file)
  `names<-`(info[["mtime"]], basename(file))
}

current_package <- function(path = "."){
  package <- desc::desc_get("Package", file = file.path(path, "DESCRIPTION"))
  gsub("[.]", "_", package)
}

package_dll <- function(path = "."){
  package <- current_package(path)
  file.path(path, "src", paste0(unname(package), .Platform$dynlib.ext))
}

get_registered_functions <- function(decorations, tag, quiet = !is_interactive()) {
  if (NROW(decorations) == 0) {
    return(vctrs::data_frame(file = character(), line = integer(), decoration = character(), params = list(), context = list(), name = character(), return_type = character(), args = list()))
  }

  out <- decorations[decorations$decoration == tag, ]
  out$functions <- lapply(out$context, parse_cpp_function, is_attribute = TRUE)
  is_template <- vapply(out$functions, \(x) attr(x, "cpp_template"), TRUE)
  out <- vctrs::vec_cbind(out, vctrs::vec_rbind(!!!out$functions))
  out$is_template <- is_template
  out$template_params <- lapply(out$context, function(x) {
    # Combine context lines into one string for regex
    full_ctx <- paste(x, collapse = " ")
    get_template_params(full_ctx)
  })

  out <- out[!(names(out) %in% "functions")]
  out$decoration <- sub("::[[:alpha:]]+", "", out$decoration)

  n <- nrow(out)

  if (!quiet && n > 0) {
    cli::cli_alert_info(glue::glue("{n} functions decorated with [[{tag}]]"))
  }

  out
}

generate_cpp_functions <- function(funs, package = "cpp20") {
  funs <- funs[c("name", "return_type", "args", "file", "line", "decoration", "is_template", "template_params")]
  funs$real_params <- vcapply(funs$args, glue_collapse_data, "{type} {name}")
  funs$sexp_params <- vcapply(funs$args, glue_collapse_data, "SEXP {name}")

  funs$calls <- mapply(wrap_call, funs$name, funs$return_type, funs$args, funs$is_template, funs$template_params, SIMPLIFY = TRUE)
  funs$package <- package

  funs$declaration <- ifelse(funs$is_template,
                             "",
                             glue::glue_data(funs, "{return_type} {name}({real_params});"))

  out <- ifelse(
    funs$is_template,
    glue::glue_data(funs,
                    '
    // {basename(file)}
    extern "C" SEXP _{package}_{name}({sexp_params}) {{
      BEGIN_CPP20
      {calls}
      END_CPP20
    }}
    '
    ),
    glue::glue_data(funs,
                    '
    // {basename(file)}
    {declaration}
    extern "C" SEXP _{package}_{name}({sexp_params}) {{
      BEGIN_CPP20
      {calls}
      END_CPP20
    }}
    '
    )
  )

  out <- glue::glue_collapse(out, sep = "\n")
  unclass(out)
}

generate_init_functions <- function(funs) {
  if (nrow(funs) == 0) {
    return(list(declarations = "", calls = ""))
  }

  funs <- funs[c("name", "return_type", "args", "file", "line", "decoration")]
  funs$declaration_params <- vcapply(funs$args, glue_collapse_data, "{type} {name}")
  funs$call_params <- vcapply(funs$args, `[[`, "name")

  declarations <- glue::glue_data(funs,
                                  '
    {return_type} {name}({declaration_params});
    '
  )

  declarations <- paste0("\n", glue::glue_collapse(declarations, "\n"), "\n")

  calls <- glue::glue_data(funs,
                           '
      {name}({call_params});
    '
  )
  calls <- paste0("\n", glue::glue_collapse(calls, "\n"));

  list(
    declarations = declarations,
    calls = calls
  )
}

generate_r_functions <- function(funs, package = "cpp20", use_package = FALSE) {
  funs <- funs[c("name", "return_type", "args")]

  if (use_package) {
    package_call <- glue::glue(', PACKAGE = "{package}"')
    package_names <- glue::glue_data(funs, '"_{package}_{name}"')
  } else {
    package_names <- glue::glue_data(funs, '`_{package}_{name}`')
    package_call <- ""
  }

  funs$package <- package
  funs$package_call <- package_call
  funs$list_params <- vcapply(funs$args, glue_collapse_data, "{name}")
  funs$params <- vcapply(funs$list_params, function(x) if (nzchar(x)) paste0(", ", x) else x)
  is_void <- funs$return_type == "void"
  funs$calls <- ifelse(is_void,
                       glue::glue_data(funs, 'invisible(.Call({package_names}{params}{package_call}))'),
                       glue::glue_data(funs, '.Call({package_names}{params}{package_call})')
  )

  out <- glue::glue_data(funs, '
    {name} <- function({list_params}) {{
      {calls}
    }}
    ')
  out <- glue::glue_collapse(out, sep = "\n\n")
  unclass(out)
}

wrap_call <- function(name, return_type, args, is_template, template_params) {

  if (is_template){
    return(wrap_call_template(name, args, template_params))
  }

  checks <- ""
  if (length(args$name) > 0) {
    checks_list <- glue::glue_data(
      args,
      "cpp20::internal::check_r_cpp_mapping<{type}>({name});"
    )
    checks <- paste0(checks_list, collapse = "\n\t")
    checks <- paste0(checks, "\n")
  }

  call <- glue::glue('{name}({list_params})', list_params = glue_collapse_data(args, "cpp20::as<std::remove_cvref_t<{type}>>({name})"))

  if (return_type == "void") {
    unclass(glue::glue("{checks}  {call};\n  return R_NilValue;", .trim = FALSE))
  } else {
    unclass(glue::glue("{checks}  return cpp20::internal::cpp_to_sexp({call});"))
  }
}

wrap_call_template <- function(name, args, template_params) {

  # Number of unique template parameters
  num_template_params <- length(template_params)

  # Total number of arguments
  num_args <- length(args$type)

  # Map each arg to its template param index (-1 if not template)
  arg_to_template <- vapply(args$type, function(type) {
    for (i in seq_along(template_params)) {
      if (is_template_arg(type, template_params[i])) {
        return(i - 1L)  # 0-indexed
      }
    }
    return(-1L)
  }, integer(1))

  # Format as C++ array: {0, 0, 1, 1} or {-1, 0, -1, 1}
  map_str <- paste0("{", paste(arg_to_template, collapse = ", "), "}")

  # Construct the lambda signature using template param names
  template_args_def <- paste(paste0("typename ", template_params), collapse = ", ")

  # Construct the lambda parameters (ALL args)
  lambda_params <- glue::glue_collapse(glue::glue("SEXP {args$name}_internal"), ", ")

  # Conversion logic
  conversions <- glue::glue("cpp20::as<std::remove_cvref_t<{args$type}>>({args$name}_internal)")
  call_args_str <- paste(conversions, collapse = ", ")

  call_str <- paste0(name, "(", call_args_str, ")")
  full_expr <- glue::glue("cpp20::internal::cpp_to_sexp({call_str})")

  outer_args <- glue::glue_collapse(args$name, ", ")

  non_template_args <- vctrs::vec_slice(args, arg_to_template == -1L)

  non_template_checks <- ""

  if (nrow(non_template_args) > 0) {
    checks_list <- glue::glue_data(
      non_template_args,
      "cpp20::internal::check_r_cpp_mapping<{type}>({name});"
    )
    non_template_checks <- paste0(checks_list, collapse = "\n\t")
    non_template_checks <- paste0(non_template_checks, "\n")
  }

  # Generate code with indices
  result <- glue::glue('
  {non_template_checks}return cpp20::internal::dispatch_template_impl<{num_template_params}, {num_args}, std::array<int, {num_args}>{map_str}>(
      []<{template_args_def}>({lambda_params}) -> decltype({full_expr}) {{
          return {full_expr};
      }},
      {outer_args}
    );
  ')

  unclass(result)
}



get_call_entries <- function(path, names, package) {
  con <- textConnection("res", local = TRUE, open = "w")

  withr::with_collate("C",
                      tools::package_native_routine_registration_skeleton(path,
                                                                          con,
                                                                          character_only = FALSE,
                                                                          include_declarations = TRUE
                      )
  )

  close(con)

  start <- grep("/* .Call calls */", res, fixed = TRUE)

  end <- grep("};", res, fixed = TRUE)

  if (length(start) == 0) {
    return("")
  }

  redundant <- glue::glue_collapse(glue::glue('extern SEXP _{package}_{names}'), sep = '|')

  if (length(redundant) > 0 && nzchar(redundant)) {
    redundant <- paste0("^", redundant)
    res <- res[!grepl(redundant, res)]
  }

  end <- grep("};", res, fixed = TRUE)

  call_calls <- startsWith(res, "extern SEXP")

  if(any(call_calls)) {
    return(res[seq(start, end)])
  }

  mid <- grep("static const R_CallMethodDef CallEntries[] = {", res, fixed = TRUE)

  res[seq(mid, end)]
}

pkg_links_to_rcpp <- function(path) {
  deps <- desc::desc_get_deps(file.path(path, "DESCRIPTION"))

  any(deps$type == "LinkingTo" & deps$package == "Rcpp")
}

get_cpp_register_needs <- function() {
  res <- read.dcf(system.file("DESCRIPTION", package = "cpp20"))[, "Config/Needs/cpp20/cpp_register"]
  strsplit(res, "[[:space:]]*,[[:space:]]*")[[1]]
}

check_valid_attributes <- function(decorations, file = decorations$file) {

  bad_decor <- startsWith(decorations$decoration, "cpp20::") &
    (!decorations$decoration %in% c("cpp20::register", "cpp20::init", "cpp20::linking_to"))

  if(any(bad_decor)) {
    lines <- decorations$line[bad_decor]
    names <- decorations$decoration[bad_decor]
    bad_lines <- glue::glue_collapse(glue::glue("- Invalid attribute `{names}` on
                 line {lines} in file '{file}'."), "\n")

    msg <- glue::glue("cpp20 attributes must be one of `cpp20::register`, `cpp20::init` or `cpp20::linking_to`:
      {bad_lines}
      ")
    stop(msg, call. = FALSE)

  }
}
# Tweaked version of cpp11::cpp_register

#' Generates wrappers for registered C++ functions
#'
#' @description
#' Register C++ functions to be callable from R. C++ functions decorated with
#' `[[cpp20::register]]` will be registered (including template functions).
#'
#'
#' @param path Path to package root directory.
#' @param quiet If `TRUE` suppresses output from this function.
#' @param extension The file extension to use for the generated src/cpp20 file.
#' Options are either '.cpp' (the default) or '.cc'.
#'
#' @returns
#' The paths to the generated R and C++ source files.
#'
#' @export
cpp_register <- function(path = ".", quiet = !is_interactive(), extension = c(".cpp", ".cc")) {
  stop_unless_installed(get_cpp_register_needs())
  extension <- match.arg(extension)

  r_path <- file.path(path, "R", "cpp20.R")
  src_path <- file.path(path, "src")
  cpp_path <- file.path(src_path, paste0("cpp20", extension))
  dll_path <- package_dll(path)
  if (file.exists(cpp_path) && file.exists(r_path) && file.exists(dll_path)){
    # If no C++ code has been modified after package dll, then no need to re-register
    dll_modified_time <- modified_time(dll_path)
    cpp_modified_times <- modified_time(list.files(src_path, full.names = TRUE))
    r_modified_time <- modified_time(r_path)

    # Continue only if any modified times are > dll modified time
    cpp_no_changes <- isTRUE(all(cpp_modified_times <= dll_modified_time))
    r_no_changes <- isTRUE(r_modified_time <= dll_modified_time)
    no_changes <- cpp_no_changes && r_no_changes

    if (no_changes){
      return(invisible(c(r_path, cpp_path)))
    }
  }

  unlink(c(r_path, cpp_path))

  suppressWarnings(
    all_decorations <- cpp_decorations(path, is_attribute = TRUE)
  )

  if (nrow(all_decorations) == 0) {
    return(invisible(character()))
  }

  check_valid_attributes(all_decorations)

  funs <- get_registered_functions(all_decorations, "cpp20::register", quiet)

  package <- current_package(path)

  cpp_functions_definitions <- generate_cpp_functions(funs, package)

  init <- generate_init_functions(get_registered_functions(all_decorations, "cpp20::init", quiet))

  r_functions <- generate_r_functions(funs, package, use_package = FALSE)

  dir.create(dirname(r_path), recursive = TRUE, showWarnings = FALSE)

  brio::write_lines(path = r_path, glue::glue('
      # Generated by cpp20: do not edit by hand

      {r_functions}
      '
  ))
  if (!quiet) {
    cli::cli_alert_success("generated file {.file {basename(r_path)}}")
  }


  call_entries <- get_call_entries(path, funs$name, package)

  cpp_function_registration <- glue::glue_data(funs, '    {{
    "_cpp20_{name}", (DL_FUNC) &_{package}_{name}, {n_args}}}, ',
                                               n_args = viapply(funs$args, nrow)
  )

  cpp_function_registration <- glue::glue_collapse(cpp_function_registration, sep  = "\n")

  extra_includes <- character()
  if (pkg_links_to_rcpp(path)) {
    extra_includes <- c(extra_includes, "#include <cpp11/R.hpp>", "#include <Rcpp.h>", "using namespace Rcpp;")
  }

  extra_includes <- paste0(extra_includes, collapse = "\n")

  user_header_files <- unique(funs$file[is_header(funs$file)])
  user_includes <- glue::glue('#include "{basename(user_header_files)}"')
  user_includes <- paste0(user_includes, collapse = "\n")


  brio::write_lines(path = cpp_path, glue::glue('
      // Generated by cpp20: do not edit by hand
      // clang-format off

      {extra_includes}
      #include <cpp20/internal/declarations.hpp>
      #include <cpp20/internal/dispatch.hpp>
      using namespace cpp20;
      #include <R_ext/Visibility.h>
      {user_includes}

      {cpp_functions_definitions}

      extern "C" {{
      {call_entries}
      }}
      {init$declarations}
      extern "C" attribute_visible void R_init_{package}(DllInfo* dll){{
        R_registerRoutines(dll, NULL, CallEntries, NULL, NULL);
        R_useDynamicSymbols(dll, FALSE);{init$calls}
        R_forceSymbols(dll, TRUE);
      }}
      ',
                                                call_entries = glue::glue_collapse(call_entries, "\n")
  ))

  if (!quiet) {
    cli::cli_alert_success("generated file {.file {basename(cpp_path)}}")
  }

  invisible(c(r_path, cpp_path))
}
