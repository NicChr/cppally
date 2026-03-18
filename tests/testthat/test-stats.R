test_that("C++ summary stats", {

  na_insert <- function(x, prop = 1/5){
    x[sample.int(length(x), size = floor(prop * length(x)), replace = FALSE)] <- NA
    x
  }

  x <- na_insert(rnorm(10^3))
  expect_equal(test_range(x, na_rm = TRUE), range(x, na.rm = TRUE))
  expect_equal(test_range(x, na_rm = FALSE), c(NA_real_, NA_real_))

  expect_equal(test_sum(x, na_rm = TRUE), sum(x, na.rm = TRUE))
  expect_equal(test_sum(x, na_rm = FALSE), NA_real_)

  expect_equal(test_mean(x, na_rm = TRUE), mean(x, na.rm = TRUE))
  expect_equal(test_mean(x, na_rm = FALSE), NA_real_)

  expect_equal(test_var(x, na_rm = TRUE), stats::var(x, na.rm = TRUE))
  expect_equal(test_var(x, na_rm = FALSE), NA_real_)

})
