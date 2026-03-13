test_that("Correct registration of cpp fns to R", {
  cpp_set_threads(2L)
  expect_equal(cpp_get_threads(), 2)
  cpp_set_threads(1L)
  expect_equal(cpp_get_threads(), 1)

  expect_error(cpp_set_threads(1.5), regexp = "Expected input type: C\\/C\\+\\+ integer")

  expect_null(test_null())

})

test_that("Type deduction on template disptch", {

  expect_null(test_multiple_deduction(1, 2))
  expect_error(test_multiple_deduction(1, 2L))

  # Deducing type when template has no constraints (defaults to vectors)
  expect_identical(test_deduced_type(1:3), "r_vec<r_int>")
  expect_identical(test_deduced_type(1L), "r_vec<r_int>")
  expect_identical(test_deduced_type(0), "r_vec<r_dbl>")
  expect_identical(test_deduced_type(letters), "r_vec<r_str>")
  expect_identical(test_deduced_type(list(1)), "r_vec<r_sexp>")
  expect_identical(test_deduced_type(iris$Species), "r_factors")
  expect_identical(test_deduced_type(as.symbol("a")), "r_sym")
  expect_identical(test_deduced_type(mean), "r_sexp")


  # Deducing type when template constrains to vector (should always find vector)
  expect_identical(test_deduced_vec_type(T), "r_vec<r_lgl>")
  expect_identical(test_deduced_vec_type(1:3), "r_vec<r_int>")
  expect_identical(test_deduced_vec_type(1L), "r_vec<r_int>")
  expect_identical(test_deduced_vec_type(0), "r_vec<r_dbl>")
  expect_identical(test_deduced_vec_type(letters), "r_vec<r_str>")
  expect_identical(test_deduced_vec_type(list(1)), "r_vec<r_sexp>")
  expect_error(test_deduced_vec_type(as.symbol("a"))) # Not a vector
  expect_error(test_deduced_vec_type(mean)) # Also not a vector

  # Deducing type when template constrains to scalar
  expect_identical(test_deduced_scalar_type(T), "r_lgl")
  expect_identical(test_deduced_scalar_type(1L), "r_int")
  expect_identical(test_deduced_scalar_type(2), "r_dbl")
  expect_identical(test_deduced_scalar_type("yes"), "r_str")
  expect_error(test_deduced_scalar_type(list(1))) # Template is RScalar (doesn't include lists)
  expect_identical(test_deduced_scalar_type(as.symbol("a")), "r_sym")
  expect_error(test_deduced_scalar_type(mean)) # Not an RScalar

})

test_that("Simple registration tests", {
  expect_error(test_scalar(1, "2"))
  expect_error(test_scalar(1, 2))
  expect_identical(test_scalar(1L, 2L), 3L)
  expect_error(test_scalar(1, 2L))


  expect_equal(scalar1(10), 10)
  expect_equal(scalar2(10), 10)

  expect_equal(vector1(c(11, 12)), 11)
  expect_equal(vector2(c(11, 12)), 12)

  expect_equal(scalar3(12L, 10L), 22)

  expect_equal(scalar4(12L, 10.5), (12 + 10.5))

  expect_equal(test_sexp(1:3), 1:3)


  expect_equal(scalar_vec1(c(1, 2), 3), c(4, 5))

  # Since scalar_vec1 expects both inputs to be the same exact type, this should error
  expect_error(scalar_vec1(1:2, 3))

  expect_equal(scalar_vec2(1:2, 3), c(4, 5))

  expect_equal(scalar_vec3(1:2, 3L, 4, 5), c(13, 14))

  expect_equal(test_mix2(1,2,3,4L,5,6,7), sum(1:7))

  expect_identical(test_specialisation(1:3), 1L)
  expect_identical(test_specialisation(c(1, 2, 3)), 0)


  expect_equal(test_str1("hi"), "hi")
  expect_equal(test_str2("hi"), "hi")
  expect_equal(test_str3("hi"), "hi")
  expect_equal(test_str4("hi"), "hi")

  expect_equal(test_as_sym("hi"), as.symbol("hi"))

  expect_identical(test_sexp2(mean), test_sexp2(mean))
  expect_error(test_sexp3(mean)) # Expects a list
  expect_identical(test_sexp3(list(mean)), list(mean))

  # Templated dispatch of SEXP/r_sexp
  # Should act as identity()
  expect_identical(test_sexp4(mean), mean)
  expect_identical(test_sexp4(1:3), 1:3)
  expect_identical(test_sexp4(1L), 1L)
  expect_identical(test_sexp4(list(1:10)), list(1:10))

  expect_identical(test_rval_identity(3), 3)
  expect_identical(test_rval_identity("a"), "a") # Expects r_str output to be STRSXP and NOT CHARSXP!
  expect_identical(test_rval_identity(as.symbol("a")), as.symbol("a"))
  expect_error(test_rval_identity(1:3))

  expect_identical(test_rval_identity(list(1L)), list(1L)) # Not sure about this

  # Doesn't error because it deduces as `r_sexp` (catch-all) :)
  expect_identical(test_rval_identity(list(1:3)), list(1:3))

  expect_identical(test_identity(1L), 1L)
  expect_identical(test_identity(1:3), 1:3)
  expect_identical(test_identity(letters), letters)
  expect_identical(test_identity("a"), "a")

  expect_identical(test_list_to_scalars(list(0)), list(FALSE, 0L, 0, "0", list(0), as.symbol("0")))

  expect_identical(test_coerce1(as.list(1:3)), 1:3)
  expect_identical(test_coerce1(list(1, 2, 3)), 1:3)

  expect_identical(test_coerce(1:10, integer()), 1:10)
  expect_identical(test_coerce(1:10, numeric()), as.double(1:10))
  expect_identical(test_coerce(letters, integer()), rep(NA_integer_, 26))
  expect_identical(test_coerce(1:10, character()), as.character(1:10))
  expect_identical(test_coerce(list(1), integer()), 1L)
  expect_identical(test_coerce(as.list(1:3), integer()), 1:3)
  expect_error(test_coerce(list(1:3), integer()))
  expect_identical(test_coerce(as.list(1:3), character()), c("1", "2", "3"))

  expect_identical(test_coerce(1:3, list()), as.list(1:3))
})

test_that("make_vec<>", {
  expect_identical(
    test_combine2(1, 2),
    list(first = c(1, 2), second = c(x = 1, y = 2))
  )
  expect_identical(
    test_combine2(list(1), list(2)),
    list(first = list(list(1), list(2)), second = list(x = list(1), y = list(2)))
  )
  expect_identical(
    test_combine2("a", "b"),
    list(first = c("a", "b"), second = c(x = "a", y = "b"))
  )
})


test_that("classed vectors", {
  curr_date <- Sys.Date()
  curr_time <- Sys.time()

  expect_equal(test_dates1(curr_date + 0:1), curr_date + 0:1)
  expect_equal(test_dates2(curr_date + 0:1), curr_date + 0:1)

  expect_equal(test_classed_vec(curr_date + 0:1), curr_date + 0:1)
  expect_equal(test_classed_vec(curr_time + 0:1), curr_time + 0:1)
})

test_that("factors", {
  x <- as.factor(letters)
  expect_identical(
    test_factor1(x),
    list(x, letters, x, factor(), factor(rep(NA, 3)), letters, letters)
    )
  expect_identical(
    test_factor2(x),
    list(x, letters, x, factor(), factor(rep(NA, 3)), letters, letters)
  )
})

test_that("sorting", {
  radix_order <- function(x){
    order(x, method = "radix", na.last = TRUE)
  }
  radix_sort <- function(x){
    sort(x, method = "radix", na.last = TRUE)
  }

  test_sort <- function(x){
    x[test_order(x) + 1L]
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

  expect_identical(test_sort(a), radix_sort(a))
  expect_identical(test_sort(b), radix_sort(b))
  expect_identical(test_sort(c), radix_sort(c))
  expect_identical(test_sort(d), radix_sort(d))
  expect_identical(test_sort(e), radix_sort(e))
  expect_identical(test_sort(f), radix_sort(f))
  expect_identical(test_sort(g), radix_sort(g))

})
