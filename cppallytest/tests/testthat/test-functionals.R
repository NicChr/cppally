set.seed(42)
a <- rnorm(20)
b <- sample.int(10, 20, replace = TRUE)
b[c(2, 15)] <- NA


test_that("reduce", {

  expect_equal(reduce_gcd(c(5, 10, 25)), 5)
  expect_equal(reduce_gcd(c(5, 10, 25, 1)), 1)
  expect_equal(reduce_gcd(c(5, 10, 25, 1, 0)), 1)
  expect_equal(reduce_gcd(c(5, 10, 25, NA, 1, 0)), NA_real_)
  expect_equal(reduce_gcd(c(1L, 10L, NA, 10L)), 1L)

  expect_equal(reduce_max(a, na_rm = TRUE), max(a, na.rm = TRUE))
  expect_equal(reduce_max(a, na_rm = FALSE), max(a, na.rm = FALSE))

  expect_equal(reduce_max(b, na_rm = TRUE), max(b, na.rm = TRUE))
  expect_equal(reduce_max(b, na_rm = FALSE), max(b, na.rm = FALSE))

  expect_equal(reduce_sum(a, na_rm = TRUE), sum(a, na.rm = TRUE))
  expect_equal(reduce_sum(a, na_rm = FALSE), sum(a, na.rm = FALSE))

  expect_equal(reduce_sum(b, na_rm = TRUE), sum(b, na.rm = TRUE))
  expect_equal(reduce_sum(b, na_rm = FALSE), sum(b, na.rm = FALSE))

  expect_equal(reduce_cumulative_sum(a, na_rm = TRUE), cumsum(a))
  expect_equal(reduce_cumulative_sum(a, na_rm = FALSE), cumsum(a))

  expect_equal(reduce_cumulative_sum(b, na_rm = TRUE), Reduce(\(a, b) sum(a, b, na.rm = TRUE), b, accumulate = TRUE))
  expect_equal(reduce_cumulative_sum(b, na_rm = FALSE), cumsum(b))

})


test_that("pmap", {

  expect_equal(pmap2_add(a, b), a + b)
  expect_equal(pmap_add(list(a, b)), a + b)

})
