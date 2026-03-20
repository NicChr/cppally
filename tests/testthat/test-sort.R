test_that("C++ radix sorting", {
  radix_order <- function(x){
    order(x, method = "radix", na.last = TRUE)
  }
  radix_sort <- function(x){
    sort(x, method = "radix", na.last = TRUE)
  }

  a <- rnorm(10^3)
  a[sample.int(10^3, 50)] <- NA

  b <- sample.int(10^3)
  b[sample.int(length(b), 100)] <- NA

  c <- sample.int(10^3, 10^6, TRUE)
  c[sample.int(length(c), 100)] <- NA

  d <- as.character(a)

  # When n < 500, simple std::sort is used
  e <- d[1:100]
  f <- b[1:100]
  g <- c[1:100]

  expect_identical(test_sort(a, preserve_ties = TRUE), radix_sort(a))
  expect_identical(test_sort(b, preserve_ties = TRUE), radix_sort(b))
  expect_identical(test_sort(c, preserve_ties = TRUE), radix_sort(c))
  expect_identical(test_sort(d, preserve_ties = TRUE), radix_sort(d))
  expect_identical(test_sort(e, preserve_ties = TRUE), radix_sort(e))
  expect_identical(test_sort(f, preserve_ties = TRUE), radix_sort(f))
  expect_identical(test_sort(g, preserve_ties = TRUE), radix_sort(g))

  expect_identical(test_sort(a, preserve_ties = FALSE), radix_sort(a))
  expect_identical(test_sort(b, preserve_ties = FALSE), radix_sort(b))
  expect_identical(test_sort(c, preserve_ties = FALSE), radix_sort(c))
  expect_identical(test_sort(d, preserve_ties = FALSE), radix_sort(d))
  expect_identical(test_sort(e, preserve_ties = FALSE), radix_sort(e))
  expect_identical(test_sort(f, preserve_ties = FALSE), radix_sort(f))
  expect_identical(test_sort(g, preserve_ties = FALSE), radix_sort(g))

  expect_identical(
    lapply(airquality, test_sort, TRUE),
    lapply(airquality, radix_sort)
  )

  expect_identical(
    lapply(airquality, test_sort, FALSE),
    lapply(airquality, radix_sort)
  )

})
