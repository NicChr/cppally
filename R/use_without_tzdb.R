#' Adds the `CPPALLY_WITHOUT_TZDB` flag to Makevars
#'
#' @description
#'
#' Use this if you will never need time-zone support in your code or package.
#'
#' `use_without_tzdb()` adds `-DCPPALLY_WITHOUT_TZDB` to `PKG_CPPFLAGS` in Makevars.
#' With this flag enabled, cppally's embedded time-zone database (`tz.h` and
#' `tzdata.h`) is not included, reducing binary size and compile memory.
#'
#' `r_date` is unaffected and `r_psxct` with a `tzone` of exactly `"UTC"`
#' works as normal. In any other time zone the instant (seconds since epoch)
#' is kept as-is, but anything that needs clock-time returns `NA`, such as
#' `hour()`, `year()`, and `add<unit>()`.
#'
#' @param quiet `[logical(1)]` - Should messages be suppressed?
#' Default is `FALSE`.
#'
#' @returns
#' Invisibly adds the `CPPALLY_WITHOUT_TZDB` flag to Makevars.
#'
#' @export
use_without_tzdb <- function(quiet = FALSE) {
  add_makevars_flag("PKG_CPPFLAGS", "-DCPPALLY_WITHOUT_TZDB", quiet = quiet)
}
