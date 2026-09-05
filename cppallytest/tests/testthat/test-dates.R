test_that("dates", {
  curr_time <- Sys.time()
  curr_date <- as.Date(curr_time)
  expect_equal(test_coerce(curr_time, ptype = curr_date), curr_date)
  expect_equal(test_coerce(curr_date, ptype = curr_time), as.POSIXct(curr_date, tz = "UTC"))
})

test_that("Date accessors", {
  expect_no_error(test_date_accessors())
})

test_that("Date arithmetic", {
  expect_no_error(test_date_add())
})

test_that("Date rounding", {
  expect_no_error(test_date_rounding())
})

test_that("Date-time accessors", {
  expect_no_error(test_datetime_accessors())
})

test_that("Date and date-time conversions", {
  expect_no_error(test_date_conversions())
})

test_that("Date-time arithmetic", {
  expect_no_error(test_datetime_add())
})

test_that("Date-time rounding", {
  expect_no_error(test_datetime_rounding())
})

test_that("Date and date-time edge cases", {
  expect_no_error(test_date_edge_cases())
})
