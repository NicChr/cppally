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

})
