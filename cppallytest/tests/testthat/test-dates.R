test_that("dates", {
  curr_time <- Sys.time()
  curr_date <- as.Date(curr_time)
  expect_equal(test_coerce(curr_time, ptype = curr_date), curr_date)
  expect_equal(test_coerce(curr_date, ptype = curr_time), as.POSIXct(curr_date, tz = "UTC"))
})
