# Each C++ test asserts pool internals (slot counts, chunk counts, chunk
# capacities, watermark, reserve accounting) and errors with a descriptive
# message on the first mismatch, so expect_true doubles as expect_no_error

test_that("protection slots track wrapper lifetimes", {
  expect_true(test_protect_count_tracking())
})

test_that("insert/release ping-pong causes no chunk churn", {
  expect_true(test_protect_slot_reuse())
})

test_that("a full pool grows by one doubled chunk", {
  expect_true(test_protect_chunk_growth())
})

test_that("a max burst then quiet retains one reserved watermark chunk", {
  expect_true(test_protect_burst_reserve())
})
