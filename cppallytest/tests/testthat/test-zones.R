test_that("timezones", {
  withr::local_envvar(TZ = "America/New_York")
  expect_no_error(test_timezones())
})
