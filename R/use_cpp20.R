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
  utils::getFromNamespace("check_is_package", "usethis")("use_cpp20()")
  rlang::check_installed("cpp20")
  utils::getFromNamespace("check_uses_roxygen", "usethis")("use_cpp20()")
  utils::getFromNamespace("check_has_package_doc", "usethis")("use_cpp20()")
  utils::getFromNamespace("use_src", "usethis")()
  utils::getFromNamespace("use_dependency", "usethis")("cpp20", "LinkingTo")
  generate_code_template()
  desc <- desc::desc()
  desc$set(SystemRequirements = "C++20")
  desc$write()
  cat(
    paste0("useDynLib(", utils::getFromNamespace("project_name", "usethis")(), ", .registration = TRUE)\n"),
    file = utils::getFromNamespace("proj_path", "usethis")("NAMESPACE"),
    append = TRUE
  )
  invisible()
}
