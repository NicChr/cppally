test_that("which", {
  x <- c(TRUE, NA, TRUE, TRUE, FALSE, FALSE, NA, NA, TRUE, NA)
  expect_identical(test_find(x, TRUE) + 1L, which(x))
  expect_identical(setdiff(seq_along(x), test_find(x, TRUE) + 1L), which(!x %in% TRUE))
})
