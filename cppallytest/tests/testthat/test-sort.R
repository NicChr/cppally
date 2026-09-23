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

  # When n is small, std::sort/std::stable_sort is used
  e <- d[1:100]
  f <- b[1:100]
  g <- c[1:100]

  expect_identical(test_sort(a), radix_sort(a))
  expect_identical(test_sort(b), radix_sort(b))
  expect_identical(test_sort(c), radix_sort(c))
  expect_identical(test_sort(d), radix_sort(d))
  expect_identical(test_sort(e), radix_sort(e))
  expect_identical(test_sort(f), radix_sort(f))
  expect_identical(test_sort(g), radix_sort(g))

  expect_identical(test_order(a) + 1L, radix_order(a))
  expect_identical(test_order(b) + 1L, radix_order(b))
  expect_identical(test_order(c) + 1L, radix_order(c))
  expect_identical(test_order(d) + 1L, radix_order(d))
  expect_identical(test_order(e) + 1L, radix_order(e))
  expect_identical(test_order(f) + 1L, radix_order(f))
  expect_identical(test_order(g) + 1L, radix_order(g))

  expect_identical(
    lapply(airquality, test_sort),
    lapply(airquality, radix_sort)
  )

})
