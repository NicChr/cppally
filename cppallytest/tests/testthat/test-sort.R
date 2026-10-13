radix_order <- function(x){
  order(x, method = "radix", na.last = TRUE)
}
radix_sort <- function(x){
  sort(x, method = "radix", na.last = TRUE)
}

with_threads <- function(n, code){
  old <- cpp_get_threads()
  on.exit(cpp_set_threads(old), add = TRUE)
  cpp_set_threads(n)
  force(code)
}

max_threads <- min(4, cpp_get_max_threads())
threads_to_use <- unique(c(1, max_threads))

expect_radix_order <- function(x, label = deparse1(substitute(x))){
  force(label)
  expected <- radix_order(x)
  for (n_threads in threads_to_use){
    with_threads(n_threads, {
      expect_identical(
        test_order(x) + 1L, expected,
        info = paste0(label, ", threads = ", n_threads)
      )
    })
  }
}

test_that("comparison sort - small vectors", {
  set.seed(1)

  expect_identical(test_order(integer(0)), integer(0))
  expect_identical(test_order(numeric(0)), integer(0))
  expect_identical(test_order(character(0)), integer(0))

  expect_radix_order(5L)
  expect_radix_order(NA_integer_)
  expect_radix_order(NA_real_)
  expect_radix_order(NA_character_)
  expect_radix_order(c(2L, 1L))
  expect_radix_order(c(NA, 1L))
  expect_radix_order(c(NaN, NA, 1, NA, NaN, -Inf, Inf, 0, -0))
  expect_radix_order(c("b", "a", NA, "", "B", "A", "a"))

  # Comparison sort for small vectors (n < 200), so test both sides
  for (n in c(199, 200, 201)){
    expect_radix_order(sample(c(1:5, NA), n, TRUE), paste0("ints with ties, n = ", n))
    expect_radix_order(sample(c(rnorm(5), NA), n, TRUE), paste0("doubles with ties, n = ", n))
    expect_radix_order(sample(c(letters[1:5], "", NA), n, TRUE), paste0("strings with ties, n = ", n))
    expect_radix_order(sample.int(1e9, n), paste0("wide ints, n = ", n))
    expect_radix_order(rnorm(n), paste0("rnorm, n = ", n))
  }
})

test_that("all NA and constant vectors", {
  expect_radix_order(rep(NA_integer_, 500))
  expect_radix_order(rep(NA_real_, 500))
  expect_radix_order(rep(c(NA, NaN), 250))
  expect_radix_order(rep(NA_character_, 500))
  expect_radix_order(rep(5L, 1e3))
  expect_radix_order(c(rep(5L, 999), NA))
  expect_radix_order(c(NA, rep(5L, 999)))
  expect_radix_order(rep(0.5, 1e3))
  expect_radix_order(rep(0.5, 2e5))
  expect_radix_order(c(rep(0.5, 999), NA))
  expect_radix_order(rep("same", 1e3))
  expect_radix_order(rep(strrep("x", 30), 1e3))
})

test_that("counting sort - small range integers (and whole numbers)", {
  set.seed(2)

  expect_radix_order(sample(c(1:10, NA), 2e5, TRUE))
  expect_radix_order(sample(-5:5, 2e5, TRUE))
  expect_radix_order(sample(c(-1e3:1e3, NA), 1e5 + 1, TRUE))

  # Long runs
  expect_radix_order(rep(1:3, each = 333334))
  expect_radix_order(rep(c(2L, 1L), each = 2e5))
  expect_radix_order(rep(c(3L, NA, 1L, 2L), each = 1e5))

  # Pre-sorted
  expect_radix_order(sort(sample.int(100, 2e5, TRUE)))
  expect_radix_order(rev(sort(sample.int(100, 2e5, TRUE))))

  # Too many keys per histogram chunk, stays single-threaded
  expect_radix_order(sample.int(5e5))
  expect_radix_order(c(sample.int(5e5), NA, NA))

  # Span cap of 4n either side (n = 1000)
  expect_radix_order(c(1L, 4000L, sample.int(4000, 998, TRUE)))
  expect_radix_order(c(1L, 4001L, sample.int(4001, 998, TRUE)))

  # Whole number doubles
  expect_radix_order(as.double(sample(c(-50:50, NA), 2e5, TRUE)))
  expect_radix_order(sample(c(0, -0, 1, -1, NA, NaN), 2e5, TRUE))

  # Other numeric types
  expect_radix_order(sample(c(TRUE, FALSE, NA), 2e5, TRUE))
  expect_radix_order(as.Date("2020-01-01") + sample(c(1:1000, NA), 2e5, TRUE))

  f <- factor(sample(c(letters, NA), 2e5, TRUE))
  expect_identical(test_order(f) + 1L, radix_order(f))
})

test_that("narrow radix - whole number offsets that fit into uint32_t", {
  set.seed(3)

  expect_radix_order(sample.int(1e9, 2e5, TRUE))
  expect_radix_order(c(sample.int(1e9, 2e5, TRUE), NA, NA))
  expect_radix_order(as.integer(runif(2e5, -2e9, 2e9)))
  expect_radix_order(sample.int(1e9, 300, TRUE))
  expect_radix_order(sample.int(1e9, 1e5 + 1, TRUE))

  # Largest possible int span
  int_max <- .Machine$integer.max
  expect_radix_order(sample(c(int_max, -int_max, NA, 0L, sample.int(1e9, 1e3)), 2e5, TRUE))

  # Constant low byte (skipped pass) and many ties
  expect_radix_order(sample.int(2e5) * 256L)
  expect_radix_order(sample.int(1e4, 2e5, TRUE) * 1e4L)

  # Power of two bucket sizes
  expect_radix_order((sample.int(2^18) - 1L) * 64L)
  expect_radix_order(sample.int(2^21) * 8L)

  # Whole number doubles
  expect_radix_order(round(runif(2e5, 0, 1e9)))
  expect_radix_order(c(round(runif(2e5, -1e9, 1e9)), NA, NaN, -0))
  expect_radix_order(round(runif(2e5, 0, 2e6)))
})

test_that("full-width radix - fractional and huge doubles", {
  set.seed(4)

  x <- rnorm(2e5)
  x[sample.int(length(x), 1e3)] <- NA
  x[sample.int(length(x), 1e3)] <- NaN
  x[sample.int(length(x), 1e3)] <- Inf
  x[sample.int(length(x), 1e3)] <- -Inf
  x[sample.int(length(x), 1e3)] <- 0.0
  x[sample.int(length(x), 1e3)] <- -0.0
  expect_radix_order(x)

  expect_radix_order(rnorm(2e5))
  expect_radix_order(runif(2e5))
  expect_radix_order(-abs(rnorm(2e5)))
  expect_radix_order(sort(rnorm(2e5)))
  expect_radix_order(rev(sort(rnorm(2e5))))

  # Heavy ties
  expect_radix_order(sample(c(rnorm(100), NA, NaN), 2e5, TRUE))
  for (digits in 1:6){
    expect_radix_order(round(rnorm(1e5), digits), paste0("round(rnorm(1e5), ", digits, ")"))
  }

  # Extremes
  extremes <- c(
    5e-324, -5e-324, .Machine$double.xmin, -.Machine$double.xmin,
    .Machine$double.xmax, -.Machine$double.xmax, .Machine$double.eps,
    Inf, -Inf, 0, -0, NA, NaN
  )
  expect_radix_order(sample(c(extremes, rnorm(100)), 2e5, TRUE))

  # Whole numbers too wide for a uint32 offset, or beyond 2^53
  expect_radix_order(round(runif(2e5, 0, 1e12)))
  expect_radix_order(round(runif(2e5, -1e15, 1e15)))
  expect_radix_order(sample(c(2^53, 2^53 + 2, 2^60, -2^60, 2^63, 1.5, 0), 2e5, TRUE))
  expect_radix_order(c(2^53, 2^53 + 2, 1, sample.int(1e5)))

  # Date-times with fractional seconds
  t <- as.POSIXct("2026-01-01", tz = "UTC") + runif(2e5, 0, 86400)
  t[sample.int(length(t), 100)] <- NA
  expect_radix_order(t)
})

test_that("strings", {
  set.seed(5)

  # High cardinality (sorted directly without de-duplication)
  expect_radix_order(as.character(sample.int(1e5)))
  expect_radix_order(c(as.character(sample.int(1e5)), NA, NA, ""))
  expect_radix_order(do.call(paste0, replicate(10, sample(c(letters, LETTERS, 0:9), 1e5, TRUE), simplify = FALSE)))

  # Low cardinality (sorted after de-duplication)
  expect_radix_order(sample(c(letters, LETTERS, "", NA), 1e5, TRUE))
  expect_radix_order(paste0(sample.int(50, 1e5, TRUE)))

  # 8-byte window edges
  edges <- c(
    "", strrep("a", 1:9), strrep("a", 15:17),
    paste0(strrep("a", 8), c("a", "b", "")), paste0(strrep("a", 16), "b"),
    paste0(strrep("a", 7), c("b", "é")), NA
  )
  expect_radix_order(edges)
  expect_radix_order(sample(edges, 1e4, TRUE))
  expect_radix_order(c(paste0(strrep("x", 8), sample.int(1e5)), edges))

  # Long shared prefixes (re-keyed several windows deep)
  expect_radix_order(strrep(sprintf("customer_%07d", sample.int(1e5)), 3))
  expect_radix_order(paste0(strrep("p", 40), sample.int(1e5)))

  # Many tiny tie runs
  expect_radix_order(sample(sprintf("customer_%07d", 1:6e4), 1e5, TRUE))
  expect_radix_order(sample(sprintf("id%03d", 1:300), 1e5, TRUE))

  # Bytes >= 0x80 sort after ASCII (C-locale, unsigned bytes)
  utf8 <- c("é", "e", "z", "Z", "ä", "日本", paste0("a", "é"), "a", NA)
  expect_radix_order(utf8)
  expect_radix_order(sample(utf8, 1e4, TRUE))
  expect_radix_order(paste0(sample(c("é", "e", "ä", "z"), 1e5, TRUE), sample.int(1e5)))

  # Case and sorted input
  expect_radix_order(sample(c(letters, LETTERS), 1e3, TRUE))
  expect_radix_order(sort(as.character(sample.int(1e5))))
})

test_that("Sorting", {
  set.seed(7)

  x <- sample(c(1:10, NA), 2e5, TRUE)
  expect_identical(test_sort(x), radix_sort(x))
  x <- c(rnorm(2e5), NA, NaN, Inf, -Inf)
  expect_identical(test_sort(x), radix_sort(x))
  x <- round(runif(2e5, -1e9, 1e9))
  expect_identical(test_sort(x), radix_sort(x))
  x <- sample(c(letters, NA, ""), 1e5, TRUE)
  expect_identical(test_sort(x), radix_sort(x))
  x <- as.character(sample.int(1e5))
  expect_identical(test_sort(x), radix_sort(x))

  expect_identical(
    lapply(mtcars, test_sort),
    lapply(mtcars, radix_sort)
  )
})
