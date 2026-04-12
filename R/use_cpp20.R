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
  stop_unless_installed(c("rlang", "usethis", "desc", "purrr"))
  usethis:::check_is_package("use_cpp20()")
  rlang::check_installed("cpp20")
  usethis:::check_uses_roxygen("use_cpp20()")
  usethis:::check_has_package_doc("use_cpp20()")
  usethis:::use_src()
  usethis:::use_dependency("cpp20", "LinkingTo")
  cpp20:::generate_code_template()
  desc <- desc::desc(package = usethis:::project_name())
  desc$set(SystemRequirements = "C++20")
  desc$write()
  cpp_register_deps <- desc::desc(package = "cpp20")$get_list("Config/Needs/cpp20/cpp_register")[[1]]
  installed <- purrr::map_lgl(cpp_register_deps, rlang::is_installed)
  if (!all(installed)) {
    usethis:::ui_bullets(c(`_` = "Now install {.pkg {cpp_register_deps[!installed]}} to use {.pkg cpp20}."))
  }
  invisible()
}
