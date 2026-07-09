inputs <- list(
  lgl = c(TRUE, FALSE, NA, TRUE, FALSE, NA),
  int = c(NA, -1:6, NA),
  dbl = c(NA, seq(-3, 5, by = 0.5), NA),
  chr = letters[1:6],
  list = list(1, NULL, NA, -3.5, "aA"),
  raw = as.raw(0:6),
  cplx = complex(
    real = c(NA, seq(-3, 5, by = 0.5), NA),
    imaginary = seq(-4, 5, by = 0.5)
  ),
  date = as.Date("2000-01-01") + 0:5
)

test_that("rep_len", {

  for (x in inputs){
    for (n in c(0L, 1L, 4L, 6L, 13L)){
      expect_identical(test_rep_len(x, n), rep_len(x, n))
    }
  }
})

test_that("rep", {

  for (x in inputs){
    for (k in c(0L, 1L, 3L)){
      expect_identical(test_rep(x, k), rep(x, k))
    }
  }

  for (x in inputs){
    rep_vec <- seq(0L, length = length(x), by = 3L)
    expect_identical(test_rep(x, rep_vec), rep(x, rep_vec))
  }

})

test_that("rep_each", {

  for (x in inputs){
    for (k in c(0L, 1L, 3L)){
      expect_identical(test_rep_each(x, k), rep(x, each = k))
    }
  }

})

