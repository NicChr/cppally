test_that("test universal inputs", {

  expect_equal(test_by_value(0.5), 0.5)
  expect_equal(test_by_lvalue(0.5), 0.5)
  expect_equal(test_by_const_lvalue(0.5), 0.5)
  expect_equal(test_by_rvalue(0.5), 0.5)

  expect_equal(test_temp_by_value(0.5), 0.5)
  expect_equal(test_temp_by_lvalue(0.5), 0.5)
  expect_equal(test_temp_by_const_lvalue(0.5), 0.5)
  expect_equal(test_temp_by_rvalue(0.5), 0.5)

})
