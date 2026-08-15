# Regression tests for narrowed template dispatch candidates
# (use_template_dispatch_candidates() / cpp_source(scalar_types = ...)).
#
# The core property: every compiled library dispatches against its OWN
# candidate tables, independent of what else is loaded in the process and in
# what order. This broke on Linux because GCC emits the shared type table with
# STB_GNU_UNIQUE binding, which glibc resolves process-wide (ignoring
# RTLD_LOCAL) - so a narrowed library silently scanned the table of whichever
# default-candidate library loaded first, wrongly rejecting its own types.
#
# cppallytest itself is built with default candidates and its dispatch tables
# are already loaded when these tests run, so compiling a narrowed library
# here reproduces the original failure direction exactly.

narrow_src <- '
  #include <cppally_light.hpp>
  using namespace cppally;

  template <RVector T>
  [[cppally::register]]
  T narrow_identity(T x){
    return x;
  }

  template <RVector T>
  [[cppally::register]]
  r_vec<r_str> narrow_type(T x){
    return r_vec<r_str>(1, r_str(internal::type_str<decltype(x)>()));
  }

  template <RVector T, RVector U>
  [[cppally::register]]
  r_vec<r_str> narrow_pair_type(T x, U y){
    r_vec<r_str> out(2);
    out.set(0, r_str(internal::type_str<decltype(x)>()));
    out.set(1, r_str(internal::type_str<decltype(y)>()));
    return out;
  }

  template <RVector T>
  [[cppally::register]]
  T narrow_same(T x, T y){
    return x;
  }

  [[cppally::register]]
  r_dbl add_hygiene(r_dbl err, r_dbl buf){
    return err + buf;
  }
'

# A second, disjoint narrowing - proves candidate sets are per-library state,
# not process-global state
str_src <- '
  #include <cppally_light.hpp>
  using namespace cppally;

  template <RVector T>
  [[cppally::register]]
  r_vec<r_str> str_type(T x){
    return r_vec<r_str>(1, r_str(internal::type_str<decltype(x)>()));
  }
'

# Default candidates, deliberately compiled AFTER the narrowed libraries
default_src <- '
  #include <cppally_light.hpp>
  using namespace cppally;

  template <RVector T>
  [[cppally::register]]
  r_vec<r_str> default_type(T x){
    return r_vec<r_str>(1, r_str(internal::type_str<decltype(x)>()));
  }
'

# Compile once, lazily, so each test_that can skip cleanly on its own
dispatch_env <- new.env()

compile_narrowed_libs <- function(){
  skip_if_cannot_cpp_source()
  if (!isTRUE(dispatch_env$narrowed_ready)){
    cppally::cpp_source(
      code = narrow_src, env = dispatch_env, debug = TRUE,
      scalar_types = c("r_int", "r_dbl"),
      r_sexp = FALSE, data_frames = FALSE, factors = FALSE
    )
    cppally::cpp_source(
      code = str_src, env = dispatch_env, debug = TRUE,
      scalar_types = "r_str",
      r_sexp = FALSE, data_frames = FALSE, factors = FALSE
    )
    dispatch_env$narrowed_ready <- TRUE
  }
}

compile_default_lib <- function(){
  skip_if_cannot_cpp_source()
  if (!isTRUE(dispatch_env$default_ready)){
    cppally::cpp_source(code = default_src, env = dispatch_env, debug = TRUE)
    dispatch_env$default_ready <- TRUE
  }
}

test_that("narrowed candidates accept their own types", {
  compile_narrowed_libs()

  # The original repro: pre-fix this errored on Linux with
  # "Argument 1 of type double does not satisfy the template constraints"
  expect_identical(dispatch_env$narrow_identity(c(1, 1, 2, 2, 3, 3)), c(1, 1, 2, 2, 3, 3))
  expect_identical(dispatch_env$narrow_identity(c(2L, 1L, 1L)), c(2L, 1L, 1L))

  expect_identical(dispatch_env$narrow_type(1.5), "r_vec<r_dbl>")
  expect_identical(dispatch_env$narrow_type(1:3), "r_vec<r_int>")
})

test_that("narrowed candidates reject excluded types with the exclusion error", {
  compile_narrowed_libs()

  # Types in the default candidate set but excluded here: explicit message
  expect_error(dispatch_env$narrow_type(letters), "excludes from its dispatch candidates")
  expect_error(dispatch_env$narrow_type(TRUE), "excludes from its dispatch candidates")
  expect_error(dispatch_env$narrow_type(Sys.Date()), "excludes from its dispatch candidates")
  expect_error(dispatch_env$narrow_type(iris$Species), "excludes from its dispatch candidates")
  expect_error(dispatch_env$narrow_type(iris), "excludes from its dispatch candidates")

  # r_sexp = FALSE takes r_vec<r_sexp> with it, so a plain list is excluded too
  expect_error(dispatch_env$narrow_type(list(1)), "excludes from its dispatch candidates")

  # Types cppally never had a candidate for fail the constraints instead, and must
  # not be claimed by the (disabled) r_sexp catch-all
  expect_error(dispatch_env$narrow_type(mean), "does not satisfy the template constraints")
})

test_that("two-param templates dispatch under a narrowed candidate set", {
  compile_narrowed_libs()

  expect_identical(
    dispatch_env$narrow_pair_type(1L, 2.5),
    c("r_vec<r_int>", "r_vec<r_dbl>")
  )
  expect_identical(
    dispatch_env$narrow_pair_type(2.5, 1L),
    c("r_vec<r_dbl>", "r_vec<r_int>")
  )
  expect_error(dispatch_env$narrow_pair_type(1L, letters), "excludes from its dispatch candidates")
})

test_that("shared template params still require matching types", {
  compile_narrowed_libs()

  expect_identical(dispatch_env$narrow_same(1.5, 2.5), 1.5)
  expect_error(dispatch_env$narrow_same(1L, 2.5), "does not match the first instance")
})

test_that("registered functions may name parameters err and buf", {
  compile_narrowed_libs()

  expect_identical(dispatch_env$add_hygiene(1, 2), 3)
})

test_that("independent libraries keep independent candidate sets", {
  compile_narrowed_libs()

  # The strings-only library accepts exactly what the int/dbl library rejects
  expect_identical(dispatch_env$str_type(letters), "r_vec<r_str>")
  expect_error(dispatch_env$str_type(1.5), "excludes from its dispatch candidates")

  # ... and loading it changed nothing for the int/dbl library
  expect_identical(dispatch_env$narrow_type(1.5), "r_vec<r_dbl>")
  expect_error(dispatch_env$narrow_type(letters), "excludes from its dispatch candidates")
})

test_that("a default-candidate library compiled after narrowed ones is unaffected", {
  compile_narrowed_libs()
  compile_default_lib()

  # Pre-fix this direction was worse than a wrong error: scanning a full-size
  # table bound to a narrowed library's smaller symbol read out of bounds
  expect_identical(dispatch_env$default_type(1:3), "r_vec<r_int>")
  expect_identical(dispatch_env$default_type(1.5), "r_vec<r_dbl>")
  expect_identical(dispatch_env$default_type(letters), "r_vec<r_str>")
  expect_identical(dispatch_env$default_type(TRUE), "r_vec<r_lgl>")
  expect_identical(dispatch_env$default_type(Sys.Date()), "r_vec<r_date>")
  expect_identical(dispatch_env$default_type(list(1)), "r_vec<r_sexp>")
})
