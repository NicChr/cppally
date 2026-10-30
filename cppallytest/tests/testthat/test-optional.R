test_that("std::optional arguments", {

  expect_identical(test_optional_add(1.5), 1.5)
  expect_identical(test_optional_add(1.5, 3L), 1.5 + 3L)

  expect_identical(test_optional_add_template(1.5), 1.5)
  expect_identical(test_optional_add_template(2L), 2L)
  expect_identical(test_optional_add_template(2L, 3L), 5L)
  expect_identical(test_optional_add_template(2L, 3), 5)

  expect_null(test_optional_return(0))
  expect_identical(test_optional_return(1), 1)
})
