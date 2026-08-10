cppally_scalar_types <- c(
  "r_lgl", "r_int", "r_int64",
  "r_dbl", "r_str", "r_cplx",
  "r_raw", "r_date", "r_psxct"
)

check_scalar_types <- function(x) {

  if (!is.character(x)){
    cli::cli_abort("{.arg x} must be a character vector")
  }

  bad <- setdiff(x, cppally_scalar_types)

  if (length(bad) > 0) {
    cli::cli_abort(c(
      "Invalid scalar type{?s}: {.val {bad}}.",
      "i" = "Valid types are {.val {cppally_scalar_types}}."
    ))
  }

}

#' Restrict the C++/R types a template function will dispatch on
#'
#' @description
#' Use this when:
#'
#' - You want to speed up compilation time.
#' - Your code accepts a subset of types.
#'
#' For example, let's say you are writing a C++ matrix algebra library using
#' cppally and you are only interested in working with integers
#' and doubles.
#'
#' Because the only types your library accepts are based on these two
#' types, you could restrict all template accepted types to `r_int` and `r_dbl`
#' (and equivalently `r_vector<r_int>` and `r_vector<r_dbl>`). Doing this will
#' dramatically improve development time by reducing compile times and also
#' reducing the size of your package dll.
#'
#' Because cppally is a header-only library, templates you call from your
#' registered templated functions, including cppally's own, are instantiated
#' once per surviving candidate rather than once per default candidate.
#'
#' @details
#' Adds flags to Makevars restricting the set of R types the dispatcher
#' considers when calling a registered templated function.
#'
#' The dispatch table holds `N^k` entries for `k` template parameters, so the
#' saving grows sharply with the number of template parameters on a function.
#'
#' Calling it with no arguments restores the defaults.
#'
#' ### This changes behaviour, not just build time
#'
#' A type left out of the candidate set is no longer accepted at the R boundary.
#' Dropping `r_date` does not make a `Date` argument slower, it makes it an
#' error. The rejection is explicit and happens before dispatch, so an excluded
#' type is never quietly claimed by the catch-all and reinterpreted as something
#' else. The same source therefore behaves differently depending on
#' these flags, so narrow the set only to types the package genuinely intends to
#' support.
#'
#' This does not affect the `r_sexp` visitor functions defined in `r_visit.h`.
#' Those dispatch over their own type list and continue to handle every R type
#' regardless of what is set here.
#'
#' @param scalar_types `[character(n)]` - Template candidate scalar types to allow package-wide.
#' Vector candidates are derived from this, e.g. `r_dbl`
#' allows both scalar `r_dbl` and vector `r_vector<r_dbl>`.
#' The default includes all cppally scalar types.
#' @param scalars `[logical(1)]` - Should bare scalar candidates be included?
#' Default is `TRUE`. Scalars share a type mapping with their vector counterpart.
#' Setting this to `FALSE` prevents scalars from being valid template arguments, but
#' the vector equivalents will still be allowed.
#' @param vectors `[logical(1)]` - Should `r_vector<>` candidates be included?
#' Default is `TRUE`.
#' @param factors `[logical(1)]` - Should `r_factors` be a candidate?
#' Default is `TRUE`.
#' @param data_frames `[logical(1)]` - Should `r_df` be a candidate?
#' Default is `TRUE`.
#' @param quiet `[logical(1)]` - Should messages be suppressed?
#' Default is `FALSE`.
#' @param r_sexp `[logical(1)]` - Should `r_sexp` be a candidate?
#' Default is `TRUE`. `r_sexp` is not a scalar type, so it has its own switch
#' rather than belonging to `scalar_types`.
#'
#' @returns
#' Invisibly adds dispatch candidate flags to Makevars.
#'
#' @export
use_template_dispatch_candidates <- function(scalar_types = cppally_scalar_types,
                                             scalars = TRUE,
                                             vectors = TRUE,
                                             factors = TRUE,
                                             data_frames = TRUE,
                                             r_sexp = TRUE,
                                             quiet = FALSE) {

  check_scalar_types(scalar_types)

  # Keep consistent order
  scalar_types <- cppally_scalar_types[cppally_scalar_types %in% scalar_types]

  # Omit a flag entirely when it matches the header default, so Makevars only
  # records deliberate narrowing
  if (identical(scalar_types, cppally_scalar_types)) {
    types_flag <- NULL
  } else {
    types_flag <- paste0(
      "-DCPPALLY_DISPATCH_CANDIDATES=", paste0(scalar_types, collapse = ",")
    )
  }

  if (scalars) {
    scalars_flag <- NULL
  } else {
    scalars_flag <- "-DCPPALLY_NO_SCALAR_CANDIDATES"
  }

  if (vectors) {
    vectors_flag <- NULL
  } else {
    vectors_flag <- "-DCPPALLY_NO_VECTOR_CANDIDATES"
  }

  if (factors) {
    factors_flag <- NULL
  } else {
    factors_flag <- "-DCPPALLY_NO_FACTOR_CANDIDATE"
  }

  if (data_frames) {
    df_flag <- NULL
  } else {
    df_flag <- "-DCPPALLY_NO_DF_CANDIDATE"
  }

  if (r_sexp) {
    sexp_flag <- NULL
  } else {
    sexp_flag <- "-DCPPALLY_NO_SEXP_CANDIDATE"
  }

  # Mirrors the candidate order in all_candidate_types: classed, then vectors,
  # then scalars, with the r_sexp catch-all last
  scalar_candidates <- character()
  vector_types <- character()
  other_types <- character()

  if (scalars) {
    scalar_candidates <- scalar_types
  }

  if (vectors) {
    vector_types <- paste0("r_vector<", scalar_types, ">")
  }

  if (factors) {
    other_types <- c(other_types, "r_factors")
  }

  if (data_frames) {
    other_types <- c(other_types, "r_df")
  }

  if (r_sexp) {
    other_types <- c(other_types, "r_sexp")
    if (vectors){
      vector_types <- c(vector_types, "r_vector<r_sexp>")
    }
  }

  n_candidates <- length(scalar_candidates) + length(vector_types) + length(other_types)

  # The header's static_assert would catch this at compile time, but failing
  # here means never writing a config that cannot build
  if (n_candidates == 0) {
    cli::cli_abort(c(
      "No dispatch candidates left.",
      "i" = "Enable at least one of {.arg scalars}, {.arg vectors}, {.arg factors}, {.arg data_frames} or {.arg r_sexp}."
    ))
  }

  # Silenced individually - the summary below reports the whole set at once
  set_makevars_value("PKG_CPPFLAGS", "-DCPPALLY_DISPATCH_CANDIDATES=", types_flag, quiet = TRUE)
  set_makevars_value("PKG_CPPFLAGS", "-DCPPALLY_NO_SCALAR_CANDIDATES", scalars_flag, quiet = TRUE)
  set_makevars_value("PKG_CPPFLAGS", "-DCPPALLY_NO_VECTOR_CANDIDATES", vectors_flag, quiet = TRUE)
  set_makevars_value("PKG_CPPFLAGS", "-DCPPALLY_NO_FACTOR_CANDIDATE", factors_flag, quiet = TRUE)
  set_makevars_value("PKG_CPPFLAGS", "-DCPPALLY_NO_DF_CANDIDATE", df_flag, quiet = TRUE)
  set_makevars_value("PKG_CPPFLAGS", "-DCPPALLY_NO_SEXP_CANDIDATE", sexp_flag, quiet = TRUE)

  n_default <- 2 + 2 * (length(cppally_scalar_types) + 1)

  cppally_bullets(c(
    "v" = "Set dispatch candidates ({n_candidates} candidates, default is {n_default}).",
    "!" = "Template functions now accept only the following types at the R boundary:",
    "",
    "*" = "scalars: {scalar_candidates}",
    "*" = "vectors: {vector_types}",
    "*" = "other: {other_types}",
    "",
    "i" = "Recompile with {.run cppally::load_all()}"
  ), quiet = quiet)
}
