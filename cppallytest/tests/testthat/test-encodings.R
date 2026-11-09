
test_that("Strings with mixed encodings", {
  skip_if_not(l10n_info()[["UTF-8"]])

  ascii_strs <- c("1", "2", "e", "c", "n", "u")
  native_strs <- c("1", "2", "é", "ç", "ñ", "Ü")
  Encoding(native_strs) <- "unknown"
  utf8_strs <- c("1", "2", "é", "ç", "ñ", "Ü")
  Encoding(utf8_strs) <- "UTF-8"
  latin1_strs <- c("1", "2", "\xe9", "\xE7", "\xf1", "\xDC")
  Encoding(latin1_strs) <- "latin1"

  expect_identical(Encoding(native_strs), rep("unknown", 6))
  expect_identical(Encoding(utf8_strs), c("unknown", "unknown", "UTF-8", "UTF-8", "UTF-8", "UTF-8"))
  expect_identical(Encoding(latin1_strs), c("unknown", "unknown", "latin1", "latin1", "latin1", "latin1"))

  expect_identical(
    ascii_strs == native_strs,
    str_eq(ascii_strs, native_strs)
  )
  expect_identical(
    ascii_strs == utf8_strs,
    str_eq(ascii_strs, utf8_strs)
  )
  expect_identical(
    ascii_strs == latin1_strs,
    str_eq(ascii_strs, latin1_strs)
  )
  expect_identical(
    utf8_strs == latin1_strs,
    str_eq(utf8_strs, latin1_strs)
  )
  expect_identical(
    utf8_strs == native_strs,
    str_eq(utf8_strs, native_strs)
  )

  combs <- expand.grid(
    ascii_strs = ascii_strs,
    native_strs = native_strs,
    utf8_strs = utf8_strs,
    latin1_strs = latin1_strs,
    stringsAsFactors = FALSE
  )

  # 0x80-0x9F are the bytes where latin1 byte order and UTF-8 order disagree
  latin1_c1 <- c("\x80", "\x85", "\x99", "\xe9", "\xf1", "a")
  Encoding(latin1_c1) <- "latin1"
  c1 <- expand.grid(x = latin1_c1, y = latin1_c1, stringsAsFactors = FALSE)

  withr::with_collate("C", {
    for (pair in combn(names(combs), 2, simplify = FALSE)) {
      x <- combs[[pair[1]]]
      y <- combs[[pair[2]]]
      expect_identical(x < y, str_lt(x, y))
    }
    expect_identical(c1$x < c1$y, str_lt(c1$x, c1$y))
  })
})
