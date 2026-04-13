test_that("identicality", {

  left <- iris
  right <- test_copy(left) # Deep copy

  expect_true(test_identical(left, right))
  attr(right, ".a") <- FALSE
  expect_false(test_identical(left, right))
  attr(right, ".a") <- NULL
  expect_true(test_identical(left, right))

  # Negative zeroes should equal positive ones
  expect_true(test_identical(round(-0.4), round(0.4)))

  # NA equality
  expect_true(test_identical(NA, NA))
  expect_true(test_identical(NA_real_, NA_real_))
  expect_true(test_identical(NaN, NaN))
  expect_false(test_identical(NaN, NA_real_))

  expect_false(test_identical(NA_real_, 1.0))

})
