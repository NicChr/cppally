test_that("bounded() is uniform on the rejection path", {

  counts <- test_rng_lemire_huge(1L, 2e6L)

  expect_length(counts, 16L)
  expect_equal(sum(counts), 2e6)

  # Bucket 15 covers [15 * 2^59, 2^63 + 1), which includes the single extra
  # value, so it is a hair wider than the rest - far below detection at this n
  expect_gt(chisq.test(counts)$p.value, 0.001)
})

test_that("bounded() is uniform on the ordinary path", {

  for (r in c(2L, 3L, 7L, 8L)) {
    counts <- test_rng_bounded_small(42L, r, 1e6L)
    expect_length(counts, r)
    expect_equal(sum(counts), 1e6)
    expect_gt(chisq.test(counts)$p.value, 0.001)
  }

  # range 1 can only ever produce 0
  expect_equal(test_rng_bounded_small(42L, 1L, 1000L), 1000L)
})

test_that("index() covers its range inclusively and refuses a reversed one", {

  x <- test_rng_index(1L, 0L, 2L, 1e5L)
  expect_setequal(x, 0:2)               # both endpoints reachable
  expect_true(all(x >= 0L & x <= 2L))
  expect_gt(chisq.test(table(x))$p.value, 0.001)

  y <- test_rng_index(1L, -5L, 5L, 1e5L)
  expect_setequal(y, -5:5)

  # degenerate range
  expect_equal(unique(test_rng_index(1L, 7L, 7L, 100L)), 7L)

  expect_error(test_rng_index(1L, 5L, 3L, 10L), "upper bound")

  # range wraps to 0 and must take the full-64-bit branch
  expect_true(test_rng_index_extremes(1L))
})

test_that("unif() respects its half-open range", {

  u <- test_rng_unif(1L, 0, 1, 1e6L)

  expect_true(all(u >= 0))
  expect_true(all(u < 1))               # half-open: 1 must never appear
  expect_lt(abs(mean(u) - 0.5), 1e-3)
  expect_lt(abs(var(u) - 1/12), 1e-4)

  # regression: `a + u * (b - a)` overflowed to Inf here, and 0 * Inf gave NaN
  wide <- test_rng_unif(1L, -1e308, 1e308, 1000L)
  expect_true(all(is.finite(wide)))

  # a == b collapses to a single value
  expect_equal(unique(test_rng_unif(1L, 3, 3, 100L)), 3)
})

test_that("set.seed() determines the stream", {

  set.seed(1); a <- test_rng_from_r(10L)
  set.seed(1); b <- test_rng_from_r(10L)
  expect_identical(a, b)

  set.seed(2); c <- test_rng_from_r(10L)
  expect_false(identical(a, c))

  # the seed itself, not just the output, is reached by set.seed()
  set.seed(1); s1 <- test_rng_seed_from_r()
  set.seed(1); s2 <- test_rng_seed_from_r()
  set.seed(2); s3 <- test_rng_seed_from_r()
  expect_identical(s1, s2)
  expect_false(identical(s1, s3))
})

test_that("drawing advances R's stream", {

  set.seed(1); invisible(test_rng_from_r(5L)); x <- runif(1)
  set.seed(1);                                 y <- runif(1)
  expect_false(identical(x, y))

  # Saving and restoring .Random.seed round-trips, because the stream is built
  # fresh from R's state on every call rather than held across calls.
  set.seed(1)
  s <- .Random.seed
  a <- test_rng_from_r(5L)
  assign(".Random.seed", s, envir = globalenv())
  b <- test_rng_from_r(5L)
  expect_identical(a, b)
})

test_that("results are reproducible under each RNGkind", {

  old <- RNGkind()
  on.exit(RNGkind(old[1], old[2], old[3]), add = TRUE)

  for (k in c("Mersenne-Twister", "L'Ecuyer-CMRG")) {
    suppressWarnings(RNGkind(k))
    set.seed(1); a <- test_rng_from_r(5L)
    set.seed(1); b <- test_rng_from_r(5L)
    expect_identical(a, b)
  }
})

test_that("rng_guard writes the seed back even when the body errors", {

  set.seed(1)
  before <- .Random.seed
  expect_error(test_rng_error_inside_with_rng(), "deliberate error")

  # PutRNGstate must have run during unwinding. If this fails, the guard is not
  # doing its job on the throwing path, which is the only reason it exists
  expect_false(identical(before, .Random.seed))
})

test_that("known-answer test against the reference implementation", {
  expect_identical(
    test_rng_raw_hex(1L, 8L),
    c(
      "cfc5d07f6f03c29b", "bf424132963fe08d", "19a37d5757aaf520",
      "bf08119f05cd56d6", "2f47184b86186fa4", "97299fcae7202345",
      "fca3c79508f41507", "85fea5c90363f221"
    )
  )
})
