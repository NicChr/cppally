test_that("Memory leak test", {
  expect_error(test_valgrind()) # This should error and release memory
})
