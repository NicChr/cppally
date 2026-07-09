# These tests compile throwaway units via cpp_source() so they can toggle the
# CPPALLY_PRESERVE_ALTREP flag, which is otherwise invisible in the default
# build. They probe the materialised()/is_altrep() state directly rather than
# relying on benchmarks. skip_if_cannot_cpp_source() lives in helper-cpp-source.R.

altrep_src <- '
  #include <cppally/r_vec.h>
  using namespace cppally;

  [[cppally::register]]
  bool altrep_input_is_altrep(r_vec<r_int> x){
    return x.is_altrep();
  }

  // TRUE if the wrapper has NOT materialised its data pointer yet.
  [[cppally::register]]
  bool altrep_lazy(r_vec<r_int> x){
    return !x.materialised();
  }

  // Force materialisation via data(), then report the state.
  [[cppally::register]]
  bool altrep_materialised_after_data(r_vec<r_int> x){
    (void) x.data();
    return x.materialised();
  }

  // Sum via per-element get(), which must be correct whether or not the
  // vector is materialised.
  [[cppally::register]]
  r_dbl altrep_sum(r_vec<r_int> x){
    r_dbl total(0);
    r_size_t n = x.length();
    for (r_size_t i = 0; i < n; ++i){
      total += x.get(i);
    }

    if (x.materialised()){
      abort("`x` has materialised");
    }

    return total;
  }
'

test_that("preserve_altrep keeps ALTREP inputs lazy but correct", {
  skip_if_cannot_cpp_source()

  env <- new.env()
  cpp_source(
    code = altrep_src,
    preserve_altrep = TRUE,
    env = env,
    debug = TRUE
  )

  x <- 1:1000                                       # ALTREP compact integer sequence
  y <- c(5L, 4L, 3L, 2L, 1L)                        # Non-ALTREP

  expect_true(env$altrep_input_is_altrep(x))        # sanity: the input is ALTREP
  expect_true(env$altrep_lazy(x))                   # not materialised at construction
  expect_true(env$altrep_materialised_after_data(x)) # data() materialises on demand

  # Lazy per-element access still yields the correct answer (with no error)
  expect_no_error(env$altrep_sum(x))
  expect_identical(env$altrep_sum(x), as.double(sum(x)))


  expect_false(env$altrep_input_is_altrep(y))
  expect_false(env$altrep_lazy(y))
  expect_true(env$altrep_materialised_after_data(y))
  expect_error(env$altrep_sum(y))
})

test_that("without preserve_altrep, ALTREP inputs are materialised eagerly", {
  skip_if_cannot_cpp_source()

  env <- new.env()
  cpp_source(
    code = altrep_src,
    preserve_altrep = FALSE,
    env = env,
    debug = TRUE
  )

  x <- 1:1000
  y <- c(5L, 4L, 3L, 2L, 1L)

  expect_true(env$altrep_input_is_altrep(x))        # still an ALTREP SEXP
  expect_false(env$altrep_lazy(x))                  # materialised eagerly at construction
  expect_error(env$altrep_sum(x))

  expect_false(env$altrep_input_is_altrep(y))
  expect_false(env$altrep_lazy(y))
  expect_error(env$altrep_sum(y))

})

