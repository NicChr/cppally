test_that("NAs", {
  na_insert <- function(x, prop = 1/5){
    x[sample.int(length(x), size = floor(prop * length(x)), replace = FALSE)] <- NA
    x
  }

  base_na_info <- function(x){
    list(
      is_na = is.na(x),
      na_count = sum(is.na(x)),
      any_na = anyNA(x),
      all_na = sum(is.na(x)) == length(x)
    )
  }

  df <- data.frame(
    a = na_insert(rnorm(10^3)),
    b = na_insert(sample(-20:20, 10^3, TRUE)),
    c = na_insert(as.character(round(rnorm(10^3), 2))),
    d = rep(NA, 10^3)
  )
  df

  expect_identical(
    lapply(df, test_nas),
    lapply(df, base_na_info)
  )

})
