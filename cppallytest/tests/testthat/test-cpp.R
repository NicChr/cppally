test_that("Correct registration of cpp fns to R", {

  cpp_set_threads(2)
  expect_equal(cpp_get_threads(), min(2, cpp_get_max_threads()))
  cpp_set_threads(1)
  expect_equal(cpp_get_threads(), 1)

  cpp_set_threads(1.7)
  expect_equal(cpp_get_threads(), 1)

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
  expect_identical(test_deduced_type(Sys.Date()), "r_vec<r_date>")
  expect_identical(test_deduced_type(Sys.time()), "r_vec<r_psxct>")
  expect_identical(test_deduced_type(iris$Species), "r_factors")
  expect_identical(test_deduced_type(as.symbol("a")), "r_sexp")
  expect_identical(test_deduced_type(mean), "r_sexp")


  # Deducing type when template constrains to vector (should always find vector)
  expect_identical(test_deduced_vec_type(T), "r_vec<r_lgl>")
  expect_identical(test_deduced_vec_type(1:3), "r_vec<r_int>")
  expect_identical(test_deduced_vec_type(1L), "r_vec<r_int>")
  expect_identical(test_deduced_vec_type(0), "r_vec<r_dbl>")
  expect_identical(test_deduced_vec_type(letters), "r_vec<r_str>")
  expect_identical(test_deduced_vec_type(list(1)), "r_vec<r_sexp>")
  expect_identical(test_deduced_vec_type(Sys.Date()), "r_vec<r_date>")
  expect_identical(test_deduced_vec_type(Sys.time()), "r_vec<r_psxct>")
  expect_error(test_deduced_vec_type(as.symbol("a"))) # Not a vector
  expect_error(test_deduced_vec_type(mean)) # Also not a vector

  # Deducing type when template constrains to scalar
  expect_identical(test_deduced_scalar_type(T), "r_lgl")
  expect_identical(test_deduced_scalar_type(1L), "r_int")
  expect_identical(test_deduced_scalar_type(2), "r_dbl")
  expect_identical(test_deduced_scalar_type("yes"), "r_str")
  expect_identical(test_deduced_scalar_type(Sys.Date()), "r_date")
  expect_identical(test_deduced_scalar_type(Sys.time()), "r_psxct")

  expect_error(test_deduced_scalar_type(Sys.Date() + 0:2))
  expect_error(test_deduced_scalar_type(Sys.time() + 0:2))


  expect_error(test_deduced_scalar_type(list(1))) # Template is RScalar (doesn't include lists)
  # expect_identical(test_deduced_scalar_type(as.symbol("a")), "r_sym")

  # Explanation: `r_sym` is not supported in template deduction
  # Internally it is mapped to `r_sexp` which is not an RScalar and therefore
  # can't be deduced
  expect_error(test_deduced_scalar_type(as.symbol("a")))

  expect_error(test_deduced_scalar_type(mean)) # Not an RScalar

  expect_identical(test_deduced_scalar_type2(as.symbol("a")), "r_sexp")

})

test_that("Runtime type ID (via CPPALLY_TYPEOF) on SEXP", {
  expect_identical(cpp_typeof(TRUE), "logical")
  expect_identical(cpp_typeof(1L), "integer")
  expect_identical(cpp_typeof(1.5), "double")
  expect_identical(cpp_typeof(letters), "character")
  expect_identical(cpp_typeof(0 + 0i), "complex")
  expect_identical(cpp_typeof(raw(1)), "raw")
  expect_identical(cpp_typeof(list()), "list")
  expect_identical(cpp_typeof(list(1)), "list")
  expect_identical(cpp_typeof(Sys.Date()), "CPPALLY_REALDATESXP")
  expect_identical(cpp_typeof(Sys.time()), "CPPALLY_REALPSXTSXP")
  # expect_identical(cpp_typeof(`storage.mode<-`(Sys.Date(), "integer")), "CPPALLY_INTDATESXP")
  expect_identical(cpp_typeof(as.factor(0)), "CPPALLY_FCTSXP")

})

test_that("Explicit symbol arguments", {
  expect_identical(test_sym(as.symbol("1")), as.symbol("1"))
})


test_that("Simple registration tests", {
  expect_error(test_scalar(1, "2"))
  expect_error(test_scalar(1, 2))
  expect_identical(test_scalar(1L, 2L), 3L)
  expect_identical(test_scalar(1, 2L), 3L)

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

  # expect_equal(test_as_sym("hi"), as.symbol("hi"))

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
  expect_error(test_coerce(letters, integer()))
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
