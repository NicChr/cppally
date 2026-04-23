test_that("factors", {
  x <- as.factor(letters)
  expect_identical(
    test_factor1(x),
    list(x, letters, x, factor(), factor(rep(NA, 3)), letters, letters)
  )
  expect_identical(
    test_factor2(x),
    list(x, letters, x, factor(), factor(rep(NA, 3)), letters, letters)
  )

  expect_equal(test_factor3(as.factor(2.5)), 2.5)
})
