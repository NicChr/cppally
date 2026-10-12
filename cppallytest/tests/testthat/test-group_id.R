test_that("group ids and counts", {

  set.seed(1)

  r_group_ids <- function(x, ordered){
    u <- unique(x)
    match(x, if (ordered) sort(u, na.last = TRUE, method = "radix") else u)
  }
  r_counts <- function(x, ordered){
    tabulate(r_group_ids(x, ordered), length(unique(x)))
  }
  r_group_starts <- function(x, ordered){
    u <- unique(x)
    match(if (ordered) sort(u, na.last = TRUE, method = "radix") else u, x)
  }
  r_group_order <- function(x, ordered){
    order(r_group_ids(x, ordered))
  }

  inputs <- list(
    few_keys = sample(c(1:10, NA), 2e5, TRUE),
    runs = rep(c(3L, 1L, 2L), each = 333334),
    many_keys = sample.int(5e3, 2e5, TRUE),
    doubles = sample(c(rnorm(50), NA), 2e5, TRUE),
    strings = sample(c(letters, NA), 2e5, TRUE),
    small = sample(c(1:3, NA), 150, TRUE)
  )

  with_threads <- function(n, code){
    old <- cpp_get_threads()
    on.exit(cpp_set_threads(old), add = TRUE)
    cpp_set_threads(n)
    force(code)
  }
  max_threads <- min(4, cpp_get_max_threads())
  threads_to_use <- unique(c(1, max_threads))

  for (x in inputs){
    for (n_threads in threads_to_use){
      with_threads(n_threads, {

        # Group IDs
        expect_identical(test_group_id(x, FALSE), r_group_ids(x, FALSE))
        expect_identical(test_group_id(x, TRUE), r_group_ids(x, TRUE))

        # Group start locations
        expect_identical(test_group_starts(x, FALSE), r_group_starts(x, FALSE))
        expect_identical(test_group_starts(x, TRUE), r_group_starts(x, TRUE))

        # Group order
        expect_identical(test_group_order(x, FALSE), r_group_order(x, FALSE))
        expect_identical(test_group_order(x, TRUE), r_group_order(x, TRUE))

        # Group Counts
        expect_identical(test_group_counts(x, FALSE), r_counts(x, FALSE))
        expect_identical(test_group_counts(x, TRUE), r_counts(x, TRUE))
      })
    }
  }
})
