test_that("replace_at at positional (1-based) indices", {

  cases <- list(
    int = list(x = 1:10,                                 with = c(100L, NA)),
    dbl = list(x = seq(0.5, 5, 0.5),                     with = c(NA, 42)),
    chr = list(x = letters[1:6],                        with = c("X", NA)),
    lgl = list(x = c(TRUE, FALSE, NA, TRUE, FALSE, NA), with = c(FALSE, NA))
  )

  where <- c(0L, 1L, 3L)
  for (case in cases){
    expect_identical(
      test_replace_at(case$x, where, case$with),
      replace(case$x, where, case$with)
    )
  }
})

test_that("replace_at recycles `with`", {

  x <- 1:6
  where <- 1:4

  # scalar `with` recycled across all positions
  expect_identical(test_replace_at(x, where, 99L), replace(x, where, 99L))
  # `with` shorter than `where`, recycled
  expect_identical(test_replace_at(x, where, c(7L, 8L)), replace(x, where, c(7L, 8L)))
})

test_that("replace_at with duplicate positions -> last write wins", {

  x     <- 1:5
  where <- c(1L, 1L, 3L)
  with  <- c(10L, 20L, 30L)

  expect_identical(test_replace_at(x, where, with), replace(x, where, with))
})

test_that("replace_at silently ignores out-of-bounds positions", {

  x <- 1:5
  # (base R would extend the vector, so compare against an explicit expected value)
  expect_identical(test_replace_at(x, c(1L, 11L), 9L), c(9L, 2L, 3L, 4L, 5L))
  # all positions out of bounds -> unchanged
  expect_identical(test_replace_at(x, c(8L, 9L), 9L), x)
})

test_that("replace_at with a logical vector", {

  x    <- 1:6
  mask <- c(TRUE, FALSE, TRUE, FALSE, TRUE, FALSE)

  expect_identical(test_replace_at(x, mask, c(10L, 20L, 30L)), replace(x, mask, c(10L, 20L, 30L)))
})

test_that("replace_at by name", {

  x <- c(a = 1L, b = 2L, c = 3L, d = 4L)

  expect_identical(test_replace_at(x, c("a", "c"), c(10L, 30L)), replace(x, c("a", "c"), c(10L, 30L)))
})

