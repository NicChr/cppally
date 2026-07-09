
cases <- list(
  int = 1:8,
  dbl = seq(0.5, 4, by = 0.5),
  chr = letters[1:8],
  lgl = c(TRUE, FALSE, NA, TRUE, FALSE, NA, TRUE, FALSE)
)

test_that("subset by position (1-based)", {
  for (x in cases){
    i <- c(3L, 1L, 5L, 5L, 8L)          # reordered + duplicated, all in bounds
    expect_identical(test_subset(x, i, invert = FALSE), x[i])
  }
})

test_that("subset out-of-bounds positions return NA (like base `[`)", {
  for (x in cases){
    oob <- c(2L, 9L, 4L, 100L)          # 9 and 100 are past the end
    expect_identical(test_subset(x, oob, invert = FALSE), x[oob])
  }
})

test_that("subset with invert drops positions (base x[-i])", {
  for (x in cases){
    i <- c(2L, 4L, 6L)
    expect_identical(test_subset(x, i, invert = TRUE), x[-i])
    # an out-of-range exclusion index is ignored, like base
    expect_identical(test_subset(x, c(2L, 99L), invert = TRUE), x[-c(2L, 99L)])
  }
})

test_that("subset with a logical mask", {
  x    <- 1:8
  mask <- c(TRUE, FALSE, TRUE, FALSE, TRUE, TRUE, FALSE, FALSE)
  expect_identical(test_subset(x, mask, invert = FALSE), x[mask])
})

test_that("subset logical mask with NA: current behaviour drops NA positions", {
  x    <- c(10L, 20L, 30L, 40L)
  mask <- c(TRUE, NA, TRUE, NA)
  expect_identical(test_subset(x, mask, invert = FALSE), c(10L, 30L))
})

test_that("subset by name (including duplicates)", {
  x <- c(a = 1L, b = 2L, c = 3L, d = 4L)
  expect_identical(test_subset(x, c("a", "c"), invert = FALSE), x[c("a", "c")])
  expect_identical(test_subset(x, c("d", "a", "d"), invert = FALSE), x[c("d", "a", "d")])
})

test_that("subset preserves names", {
  x <- c(a = 1L, b = 2L, c = 3L, d = 4L, e = 5L)
  i <- c(2L, 4L)
  expect_identical(test_subset(x, i, invert = FALSE), x[i])
})
