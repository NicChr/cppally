
# Thanks to cpp11 authors and contributors for this code
# Taken and slightly amended to work with cpp20

is_windows <- function(){
  .Platform$OS.type == "windows"
}

generate_makevars <- function (includes, cxx_std){
  c(
    sprintf("CXX_STD=%s", cxx_std),
    sprintf("PKG_CPPFLAGS=%s", paste0(includes, collapse = " "))
  )
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


cpp_source <- function(file, code = NULL, env = parent.frame(), clean = TRUE,
          quiet = TRUE, cxx_std = Sys.getenv("CXX_STD", "CXX20"), dir = tempfile()){
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
  makevars_content <- generate_makevars(includes, cxx_std)
  brio::write_lines(makevars_content, file.path(new_dir, "Makevars"))
  source_files <- normalizePath(c(new_file_path, cpp_path),
                                winslash = "/")
  res <- callr::rcmd("SHLIB", source_files, user_profile = TRUE,
                     show = !quiet, wd = new_dir)
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

cpp_eval <- function(code, env = parent.frame(), clean = TRUE, quiet = TRUE,
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
    env = env, clean = clean, quiet = quiet, cxx_std = cxx_std
  )
  f()
}
