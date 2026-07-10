test_that("Coercion", {

  expect_identical(
    test_as_sym("a", "b", 1, as.symbol("c"), "d", 1.5),
    lapply(c("a", "b", 1, "c", "d", 1.5), as.symbol)
  )

  expect_identical(
    test_as_int("3.5", "1.6", 1.7, as.symbol("1.8"), "10.7", 11.5),
    c(3L, 1L, 1L, 1L, 10L, 11L,
      3L, 1L, 1L, 1L, 10L, 11L)
  )

  expect_equal(
    test_as_dbl("3.5", "1.6", 2L, as.symbol("1.8"), "10.7", 11L),
    c(3.5, 1.6, 2, 1.8, 10.7, 11,
      3.5, 1.6, 2, 1.8, 10.7, 11)
  )

  expect_identical(
    test_as_str(3.5, 1L, 2.5, as.symbol("1.7"), 7L, 7.7),
    c("3.5", "1", "2.5", "1.7", "7", "7.7",
      "3.5", "1", "2.5", "1.7", "7", "7.7")
  )

  expect_no_error(test_to_int())
  expect_no_error(test_to_int64())
  expect_no_error(test_to_double())
  expect_no_error(test_to_uint())
  expect_no_error(test_to_r_size_t())
  expect_no_error(test_to_bool())
  expect_no_error(test_to_r_int())
  expect_no_error(test_to_r_int64())
  expect_no_error(test_to_r_dbl())
  expect_no_error(test_to_r_lgl())

  # Non-aborting edge cases: truncation, widening, boundaries, NaN, +/-Inf
  expect_no_error(test_coerce_edge())

  # Lossy coercions from a non-NA source abort inside scalar_coerce
  expect_error(test_coerce_int64_to_int_overflow())
  expect_error(test_coerce_dbl_to_int_overflow())
  expect_error(test_coerce_pos_inf_to_int())
  expect_error(test_coerce_neg_inf_to_int())
  expect_error(test_coerce_dbl_to_int64_overflow())

  # values that cast exactly onto the target's NA sentinel (INT_MIN / INT64_MIN)
  expect_error(test_coerce_int_min_to_int())
  expect_error(test_coerce_int64_min_to_int64())

})
