
# Thanks to cpp11 authors and contributors for this code
# Taken and slightly amended to work with cpp20

is_windows <- function(){
  .Platform$OS.type == "windows"
}

generate_makevars <- function (includes, cxx_std, debug, preserve_altrep){
  out <- c(
    sprintf("CXX_STD=%s", cxx_std),
    sprintf("PKG_CPPFLAGS=%s", paste0(includes, collapse = " "))
  )
  if (preserve_altrep){
    out[2] <- paste(out[2], "-DCPP20_PRESERVE_ALTREP")
  }
  if (debug) {
    out <- c(out, "override CXXFLAGS += -O0")
  }
  out
}

generate_cpp_name <- function (name, loaded_dlls = c("cpp20", names(getLoadedDLLs()))){
  ext <- tools::file_ext(name)
  root <- tools::file_path_sans_ext(basename(name))
  count <- 2
  new_name <- root
  while (new_name %in% loaded_dlls) {
    new_name <- sprintf("%s_%i", root, count)
    count <- count + 1
  }
  sprintf("%s.%s", new_name, ext)
}

get_linking_to <- function(decorations){
  out <- decorations[decorations$decoration == "cpp20::linking_to", ]
  if (NROW(out) == 0) {
    return(character())
  }
  gsub("\"", "", as.character(unlist(out$params)))
}

generate_include_paths <- function(packages){
  out <- character(length(packages))
  for (i in seq_along(packages)) {
    path <- system.file(package = packages[[i]], "include")
    if (is_windows()) {
      path <- utils::shortPathName(path)
    }
    out[[i]] <- paste0("-I", shQuote(path))
  }
  out
}

#' Compile C++20 code
#'
#' @description
#' cpp11-style helpers to compile cpp20 code outside of a cpp20-linked package
#' context.
#'
#' `cpp_source()` compiles and loads a single C++ file for use in R,
#' either from an expression or a cpp file.
#' This may include multiple C++ functions.
#'
#' `cpp_eval()` evaluates a single C++ expression and returns the result.
#' For example `cpp_eval('get_threads()')` will run the C++ function
#' `cpp20::get_threads()` and return the number of OMP threads currently set
#' for use.
#' `void` return is not supported in `cpp_eval()`.
#'
#' @param file C++ file.
#' @param code If `file` is `NULL` then a string of C++ code to compile.
#' @param env Environment where R functions should be defined.
#' @param clean Should files be cleaned up after sourcing? Default is `TRUE`.
#' @param quiet Should compiler output be suppressed? Default is `TRUE`.
#' @param cxx_std C++ standard to use. Should be >= C++20.
#' @param debug Should C++ code be compiled in a debug build?
#' Default is `FALSE`.
#' @param preserve_altrep Should ALTREP vectors be preserved by avoiding
#' materialisation where possible? Default is `FALSE`.
#' @param dir Directory to store the source files.
#' The default is a temporary directory via `tempfile()` which is removed when
#' `clean = TRUE`.
#'
#' @returns
#' `cpp_source()` invisibly compiles the C++ code and registers
#' the `[[cpp20::register]]` tagged functions to R. \cr
#' `cpp_eval()` returns the result of the evaluated C++ expression.
#'
#' @examples
#'
#' library(cpp20)
#'
#' \donttest{
#' cpp_eval("r_int(0)")
#' cpp_source(code = '
#'   #include <cpp20.hpp>
#'   using namespace cpp20;
#'
#'   [[cpp20::register]]
#'   r_dbl add(r_dbl x, r_dbl y){
#'     return x + y;
#'   }
#' ', debug = TRUE)
#' add(1, 2)
#' add(2, NA)
#'
#' ### ALTREP ###
#'
#' # cpp20 also supports lazy ALTREP materialisation as an opt-in feature.
#' # To opt-in, set `preserve_altrep = TRUE`
#'
#' cpp_source(
#'   code = '
#'   #include <cpp20.hpp>
#'   using namespace cpp20;
#'
#'   [[cpp20::register]]
#'   r_int last_altrep_unaware(r_vec<r_int> x){
#'     r_int out;
#'     r_size_t n = x.length();
#'
#'     if (n > 0){
#'       out = x.get(n - 1);
#'     }
#'     return out;
#'   }
#' ', debug = TRUE
#' )
#'
#' cpp_source(
#'   code = '
#'   #include <cpp20.hpp>
#'   using namespace cpp20;
#'
#'   [[cpp20::register]]
#'   r_int last_altrep_aware(r_vec<r_int> x){
#'     r_int out;
#'     r_size_t n = x.length();
#'
#'     if (n > 0){
#'       out = x.get(n - 1);
#'     }
#'     return out;
#'   }
#' ', debug = TRUE,
#'   preserve_altrep = TRUE
#' )
#'
#' library(bench)
#' mark(last_altrep_aware(1:10^5)) # No materialisation
#' mark(last_altrep_unaware(1:10^5)) # Materialises full vector
#'
#' }
#'
#' @rdname cpp_source
#' @export
cpp_source <- function(file, code = NULL, env = parent.frame(),
                       clean = TRUE, quiet = TRUE, debug = FALSE,
                       preserve_altrep = FALSE,
                       cxx_std = Sys.getenv("CXX_STD", "CXX20"),
                       dir = tempfile()){
  stop_unless_installed(
    c("brio", "callr", "cli", "decor",
      "desc", "glue", "vctrs")
  )
  if (!missing(file) && !file.exists(file)) {
    stop("Can't find `file` at this path:\n", file, "\n", call. = FALSE)
  }
  dir.create(dir, showWarnings = FALSE, recursive = TRUE)
  dir.create(file.path(dir, "R"), showWarnings = FALSE)
  dir.create(file.path(dir, "src"), showWarnings = FALSE)
  if (!is.null(code)) {
    tf <- tempfile(pattern = "code_", fileext = ".cpp")
    file <- tf
    if (isTRUE(clean)) {
      on.exit(unlink(tf))
    }
    brio::write_lines(code, file)
  }
  if (!any(tools::file_ext(file) %in% c("cpp", "cc"))) {
    stop("`file` must have a `.cpp` or `.cc` extension")
  }
  name <- generate_cpp_name(file)
  package <- tools::file_path_sans_ext(name)
  orig_dir <- normalizePath(dirname(file), winslash = "/")
  new_dir <- normalizePath(file.path(dir, "src"), winslash = "/")
  file.copy(file, file.path(new_dir, name))
  new_file_path <- file.path(new_dir, name)
  new_file_name <- basename(new_file_path)
  orig_file_path <- file.path(orig_dir, new_file_name)
  suppressWarnings(all_decorations <- cpp_decorations(dir, is_attribute = TRUE))
  check_valid_attributes(all_decorations, file = orig_file_path)
  funs <- get_registered_functions(all_decorations, "cpp20::register", quiet = quiet)
  cpp_functions_definitions <- generate_cpp_functions(funs, package = package)
  cpp_path <- file.path(dirname(new_file_path), "cpp20.cpp")
  brio::write_lines(c("#include <cpp20/r_dispatch.h>",
                      glue::glue('#include "{new_file_name}"'),
                      "using namespace cpp20;",
                      "using internal::cpp_to_sexp;",
                      "using internal::dispatch_template_impl;",
                      cpp_functions_definitions),
                    cpp_path)
  linking_to <- union(get_linking_to(all_decorations), "cpp20")
  includes <- generate_include_paths(linking_to)
  if (isTRUE(clean)) {
    on.exit(unlink(dir, recursive = TRUE), add = TRUE)
  }
  r_functions <- generate_r_functions(funs, package = package,
                                      use_package = TRUE)
  makevars_content <- generate_makevars(includes, cxx_std, debug, preserve_altrep)
  brio::write_lines(makevars_content, file.path(new_dir, "Makevars"))
  shared_lib_name <- paste0(tools::file_path_sans_ext(new_file_name), .Platform$dynlib.ext)
  res <- callr::rcmd("SHLIB", c(cpp_path, "-o", shared_lib_name),
                     user_profile = TRUE, show = !quiet, wd = new_dir)
  if (res$status != 0) {
    error_messages <- res$stderr
    error_messages <- gsub(tools::file_path_sans_ext(new_file_path),
                           tools::file_path_sans_ext(orig_file_path), error_messages,
                           fixed = TRUE)
    cat(error_messages)
    stop("Compilation failed.", call. = FALSE)
  }
  shared_lib <- file.path(dir, "src", paste0(tools::file_path_sans_ext(new_file_name),
                                             .Platform$dynlib.ext))
  r_path <- file.path(dir, "R", "cpp20.R")
  brio::write_lines(r_functions, r_path)
  source(r_path, local = env)
  dyn.load(shared_lib, local = TRUE, now = TRUE)
}
#' @rdname cpp_source
#' @export
cpp_eval <- function(code, env = parent.frame(), clean = TRUE,
                     quiet = TRUE, debug = FALSE,
                     cxx_std = Sys.getenv("CXX_STD", "CXX20")){
  cpp_source(
    code = paste(c(
      "#include <cpp20/r_dispatch.h>",
      "#include <cpp20.hpp>",
      "using namespace cpp20;",
      "using internal::cpp_to_sexp;",
      "[[cpp20::register]]",
      paste0("SEXP f() { return cpp_to_sexp(", code, "); }")
    ), collapse = "\n"),
    env = env, clean = clean, quiet = quiet,
    debug = debug, cxx_std = cxx_std
  )
  get("f", envir = env)()
}
