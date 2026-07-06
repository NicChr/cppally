test_that("Scalar arithmetic", {
  expect_no_error(test_arithmetic())
})

test_that("Integer overflow", {
  expect_no_error(test_overflow())
})

test_that("Arithmetic edge cases", {
  expect_no_error(test_arithmetic_edge_cases())
})

