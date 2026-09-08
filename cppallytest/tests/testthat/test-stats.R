
na_insert <- function(x, prop = 1/5){
  x[sample.int(length(x), size = floor(prop * length(x)), replace = FALSE)] <- NA
  x
}

x <- na_insert(rnorm(500))
y <- as.integer(x)
z <- as.character(x)

test_that("C++ range", {

  expect_equal(test_range(x, na_rm = TRUE), range(x, na.rm = TRUE))
  expect_identical(test_range(x, na_rm = FALSE), c(NA_real_, NA_real_))
  expect_identical(test_range(numeric(), na_rm = FALSE), c(NA_real_, NA_real_))
  expect_identical(test_range(numeric(), na_rm = TRUE), c(NA_real_, NA_real_))
  expect_identical(test_range(NA_real_, na_rm = FALSE), c(NA_real_, NA_real_))
  expect_identical(test_range(NA_real_, na_rm = TRUE), c(NA_real_, NA_real_))

  expect_equal(test_range(y, na_rm = TRUE), range(y, na.rm = TRUE))
  expect_identical(test_range(y, na_rm = FALSE), c(NA_integer_, NA_integer_))
  expect_identical(test_range(integer(), na_rm = FALSE), c(NA_integer_, NA_integer_))
  expect_identical(test_range(integer(), na_rm = TRUE), c(NA_integer_, NA_integer_))
  expect_identical(test_range(NA_integer_, na_rm = FALSE), c(NA_integer_, NA_integer_))
  expect_identical(test_range(NA_integer_, na_rm = TRUE), c(NA_integer_, NA_integer_))

  expect_equal(test_range(z, na_rm = TRUE), range(z, na.rm = TRUE))
  expect_identical(test_range(z, na_rm = FALSE), c(NA_character_, NA_character_))
  expect_identical(test_range(character(), na_rm = FALSE), c(NA_character_, NA_character_))
  expect_identical(test_range(character(), na_rm = TRUE), c(NA_character_, NA_character_))
  expect_identical(test_range(NA_character_, na_rm = FALSE), c(NA_character_, NA_character_))
  expect_identical(test_range(NA_character_, na_rm = TRUE), c(NA_character_, NA_character_))
})


test_that("C++ sum", {

  expect_equal(test_sum(x, na_rm = TRUE), sum(x, na.rm = TRUE))
  expect_identical(test_sum(x, na_rm = FALSE), NA_real_)
  expect_identical(test_sum(numeric(), na_rm = FALSE), 0.0)
  expect_identical(test_sum(numeric(), na_rm = TRUE), 0.0)
  expect_identical(test_sum(NA_real_, na_rm = FALSE), NA_real_)

  expect_equal(test_sum(y, na_rm = TRUE), sum(y, na.rm = TRUE))
  expect_identical(test_sum(y, na_rm = FALSE), NA_real_)
  expect_identical(test_sum(integer(), na_rm = FALSE), 0.0)
  expect_identical(test_sum(integer(), na_rm = TRUE), 0.0)
  expect_identical(test_sum(NA_integer_, na_rm = FALSE), NA_real_)

})

test_that("C++ mean", {

  expect_equal(test_mean(x, na_rm = TRUE), mean(x, na.rm = TRUE))
  expect_identical(test_mean(x, na_rm = FALSE), NA_real_)
  expect_identical(test_mean(numeric(), na_rm = FALSE), NaN)
  expect_identical(test_mean(numeric(), na_rm = TRUE), NaN)
  expect_identical(test_mean(NA_real_, na_rm = FALSE), NA_real_)

  expect_equal(test_mean(y, na_rm = TRUE), mean(y, na.rm = TRUE))
  expect_identical(test_mean(y, na_rm = FALSE), NA_real_)
  expect_identical(test_mean(integer(), na_rm = FALSE), NaN)
  expect_identical(test_mean(integer(), na_rm = TRUE), NaN)
  expect_identical(test_mean(NA_integer_, na_rm = FALSE), NA_real_)

})

test_that("C++ var", {

  expect_equal(test_var(x, na_rm = TRUE), var(x, na.rm = TRUE))
  expect_identical(test_var(x, na_rm = FALSE), NA_real_)
  expect_identical(test_var(numeric(), na_rm = FALSE), NA_real_)
  expect_identical(test_var(numeric(), na_rm = TRUE), NA_real_)
  expect_identical(test_var(NA_real_, na_rm = FALSE), NA_real_)

  expect_equal(test_var(y, na_rm = TRUE), var(y, na.rm = TRUE))
  expect_identical(test_var(y, na_rm = FALSE), NA_real_)
  expect_identical(test_var(integer(), na_rm = FALSE), NA_real_)
  expect_identical(test_var(integer(), na_rm = TRUE), NA_real_)
  expect_identical(test_var(NA_integer_, na_rm = FALSE), NA_real_)

})
