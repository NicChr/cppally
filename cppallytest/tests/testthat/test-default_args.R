formals_list <- function(fun, envir = parent.frame()) {
  as.list(formals(fun, envir))
}

test_that("Default args", {
  expect_identical(
    formals_list(test_default_lgls),
    alist(
      x = FALSE,
      y = TRUE,
      xx = FALSE,
      yy = TRUE,
      zz = .Call(
        `_cppallytest_test_default_lgls_arg_zz`,
        cpp_default = "true || false"
      )
    )
  )

  expect_identical(
    formals_list(test_default_ints),
    alist(
      x = -1L,
      y = 2L,
      z = 0L,
      xx = .Call(`_cppallytest_test_default_ints_arg_xx`, cpp_default = "1 + 2"),
      yy = .Call(
        `_cppallytest_test_default_ints_arg_yy`,
        cpp_default = "r_int(2) + 3"
      ),
      zz = 2.5
    )
  )

  expect_identical(
    formals_list(test_default_dbls),
    alist(
      x = -1,
      y = 2.5,
      z = 3,
      xx = .Call(`_cppallytest_test_default_dbls_arg_xx`, cpp_default = "1 + 3"),
      yy = 1e+10,
      zz = .Call(
        `_cppallytest_test_default_dbls_arg_zz`,
        cpp_default = "std::numeric_limits<double>::max()"
      ),
      zzz = .Call(
        `_cppallytest_test_default_dbls_arg_zzz`,
        cpp_default = "std::sqrt(std::numeric_limits<double>::epsilon())"
      )
    )
  )

  expect_identical(
    formals_list(test_default_strs),
    alist(
      a = .Call(`_cppallytest_test_default_strs_arg_a`, cpp_default = "\"a\""),
      b = "b",
      c = "c"
    )
  )

  expect_identical(
    formals_list(test_default_args_templated),
    alist(
      x = .Call(
        `_cppallytest_test_default_args_templated_arg_x`,
        cpp_default = "r_dbl(1) / r_dbl(2)"
      ),
      a = .Call(
        `_cppallytest_test_default_args_templated_arg_a`,
        cpp_default = "r_int(1) + 2"
      ),
      b = .Call(
        `_cppallytest_test_default_args_templated_arg_b`,
        cpp_default = "r_dbl(1) / r_dbl(4)"
      ),
      c = .Call(
        `_cppallytest_test_default_args_templated_arg_c`,
        cpp_default = "r_int64(5)"
      ),
      d = 2L,
      e = NA,
      f = .Call(
        `_cppallytest_test_default_args_templated_arg_f`,
        cpp_default = "as<r_str>(r_int(42))"
      ),
      g = .Call(
        `_cppallytest_test_default_args_templated_arg_g`,
        cpp_default = "as<r_str>(r_dbl(0.5))"
      ),
      h = 7L
    )
  )

  expect_identical(
    test_default_lgls(),
    c(FALSE, TRUE, FALSE, TRUE, TRUE)
  )

  expect_identical(
   test_default_ints(),
   c(-1L, 2L, 0L, 3L, 5L, 2L)
  )

  expect_equal(
    test_default_dbls(),
    c(-1, 2.5, 3, 4, 1e+10, .Machine$double.xmax, sqrt(.Machine$double.eps))
  )

  expect_identical(
    test_default_strs(),
    c("a", "b", "c")
  )

  expect_identical(
    test_default_args_templated(),
    c("0.5", "3", "0.25", "5", "TRUE", NA_character_, "42", "0.5", "7")
  )

  # std::optional arg with default = 10
  expect_identical(
    test_optional_default(),
    10L
  )

  expect_identical(
    test_optional_default(20L),
    20L
  )

  expect_identical(
    test_optional_default(NULL),
    100L
  )

})
