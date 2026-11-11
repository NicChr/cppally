with_encoding <- function(x, encoding) {
  Encoding(x) <- encoding
  x
}

raw_bytes <- function(x) {
  lapply(x, charToRaw)
}

# Same text as `x`, with each element randomly marked UTF-8, native or latin1
mix_encodings <- function(x) {
  utf8 <- enc2utf8(x)
  native <- with_encoding(utf8, "unknown")
  latin1 <- iconv(utf8, "UTF-8", "latin1")
  pick <- sample.int(3L, length(x), TRUE)
  utf8[pick == 2L] <- native[pick == 2L]
  utf8[pick == 3L] <- latin1[pick == 3L]
  utf8
}

c_order <- function(x) {
  withr::with_collate("C", order(x))
}

expect_same_comparisons <- function(x, y) {
  withr::with_collate("C", {
    expect_identical(str_eq(x, y), x == y)
    expect_identical(str_lt(x, y), x < y)
    expect_identical(str_le(x, y), x <= y)
  })
}

expect_c_order <- function(x) {
  expect_identical(test_order(x), c_order(x) - 1L)
}

expect_groups <- function(x) {
  uniques <- unique(x)
  sorted <- withr::with_collate("C", sort(uniques, na.last = TRUE))
  expect_identical(enc_group_ids(x), match(x, uniques) - 1L)
  expect_identical(enc_n_groups(x), length(uniques))
  expect_identical(enc_ordered_group_ids(x), match(x, sorted) - 1L)
}

# Sets a Latin-1 LC_CTYPE for `code`, skipping if the platform doesn't have one
with_latin1_ctype <- function(code) {
  old <- Sys.getlocale("LC_CTYPE")
  on.exit(suppressWarnings(Sys.setlocale("LC_CTYPE", old)), add = TRUE)
  locales <- c(
    "English_United Kingdom.1252", "English_United States.1252",
    "en_GB.ISO-8859-1", "en_US.ISO-8859-1", "en_US.ISO8859-1"
  )
  for (locale in locales) {
    if (nzchar(suppressWarnings(Sys.setlocale("LC_CTYPE", locale)))) {
      break
    }
  }
  skip_if_not(isTRUE(l10n_info()[["Latin-1"]]), "No Latin-1 locale available")
  force(code)
}

e_utf8 <- "é"
e_native <- with_encoding(e_utf8, "unknown")
e_latin1 <- with_encoding("\xe9", "latin1")
e_bytes <- with_encoding(e_utf8, "bytes")

words <- c(
  "é", "e", "ç", "ñ", "Ü", "ä", "z", "Z", "a",
  "café", "cafe", "über"
)

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

  expect_identical(str_eq(utf8_strs, latin1_strs), rep(TRUE, 6))
  expect_identical(str_eq(utf8_strs, native_strs), rep(TRUE, 6))
  expect_identical(str_eq(native_strs, latin1_strs), rep(TRUE, 6))

  combs <- expand.grid(
    ascii_strs = ascii_strs,
    native_strs = native_strs,
    utf8_strs = utf8_strs,
    latin1_strs = latin1_strs,
    stringsAsFactors = FALSE
  )

  for (pair in combn(names(combs), 2, simplify = FALSE)) {
    expect_same_comparisons(combs[[pair[1]]], combs[[pair[2]]])
  }

  # 0x80-0x9F are the bytes where latin1 byte order and UTF-8 order disagree
  latin1_c1 <- c("\x80", "\x85", "\x99", "\xe9", "\xf1", "a")
  Encoding(latin1_c1) <- "latin1"
  c1 <- expand.grid(x = latin1_c1, y = latin1_c1, stringsAsFactors = FALSE)

  expect_same_comparisons(c1$x, c1$y)
})

test_that("Comparisons with NA across encodings", {
  skip_if_not(l10n_info()[["UTF-8"]])

  x <- c(NA, e_utf8, e_latin1, NA, "NA", NA)
  y <- c(e_latin1, NA, NA, NA, NA, "NA")
  expect_same_comparisons(x, y)
  expect_identical(str_eq(NA_character_, "NA"), NA)
})

test_that("Harder strings compare by text", {
  skip_if_not(l10n_info()[["UTF-8"]])

  x <- c(
    "", e_utf8, paste0(e_utf8, "a"), e_latin1, "e\u0301", "日本",
    "\U1F600", "€", with_encoding("\xe9a", "latin1")
  )
  pairs <- expand.grid(x = x, y = x, stringsAsFactors = FALSE)
  expect_same_comparisons(pairs$x, pairs$y)

  # Composed and decomposed e-acute are different text
  expect_false(str_eq(e_utf8, "e\u0301"))
  expect_identical(enc_n_unique(x), 7L)
  expect_identical(enc_unique(x), unique(x))
  expect_c_order(x)
})

test_that("unique, duplicated and n_unique treat encodings as one value", {
  skip_if_not(l10n_info()[["UTF-8"]])
  set.seed(1)

  x <- c(e_utf8, e_latin1, e_native, "e", e_latin1, NA, "NA", NA)
  expect_identical(enc_n_unique(x), 4L)
  expect_identical(enc_unique(x), c(e_utf8, "e", NA, "NA"))
  expect_identical(enc_duplicated(x), duplicated(x))

  low <- mix_encodings(sample(c(words, NA), 1e4, TRUE))
  high <- mix_encodings(paste0(sample(words, 1e4, TRUE), sample.int(2e3, 1e4, TRUE)))

  for (data in list(low, high)) {
    expect_identical(enc_unique(data), unique(data))
    expect_identical(enc_duplicated(data), duplicated(data))
    expect_identical(enc_n_unique(data), length(unique(data)))
  }
})

test_that("match treats encodings as one value", {
  skip_if_not(l10n_info()[["UTF-8"]])
  set.seed(2)

  # A single needle takes a linear path, several go through the hash table
  expect_identical(enc_match(e_latin1, c("a", e_utf8)), 1L)
  expect_identical(enc_match("zz", c("a", e_utf8)), NA_integer_)
  expect_identical(enc_match(c(e_latin1, e_native), c("a", e_utf8, e_latin1)), c(1L, 1L))
  expect_identical(enc_match(c(NA, e_utf8), c(e_latin1, NA)), c(1L, 0L))

  table <- mix_encodings(words)
  needles <- mix_encodings(sample(c(words, "missing", NA), 1e3, TRUE))
  expect_identical(enc_match(needles, table), match(needles, table) - 1L)
})

test_that("Groups merge the same text across encodings", {
  skip_if_not(l10n_info()[["UTF-8"]])
  set.seed(3)

  x <- c(e_utf8, e_latin1, e_native, "a", NA, "NA", e_latin1)
  expect_identical(enc_group_ids(x), c(0L, 0L, 0L, 1L, 2L, 3L, 0L))
  expect_identical(enc_n_groups(x), 4L)
  expect_identical(enc_n_groups(c(e_utf8, e_latin1, e_native)), 1L)

  expect_groups(x)
  expect_groups(mix_encodings(sample(c(words, NA), 150, TRUE)))
  expect_groups(mix_encodings(sample(c(words, NA), 1e4, TRUE)))
  expect_groups(mix_encodings(paste0(sample(words, 1e4, TRUE), sample.int(1e4))))
})

test_that("order and sort use the UTF-8 text at every size and cardinality", {
  skip_if_not(l10n_info()[["UTF-8"]])
  set.seed(4)

  # Ties across encodings keep their input order
  expect_identical(test_order(c(e_utf8, e_latin1, e_native, "a")), c(3L, 0L, 1L, 2L))

  small <- mix_encodings(sample(c(words, NA), 150, TRUE))
  low <- mix_encodings(sample(c(words, NA), 1e4, TRUE))
  high <- mix_encodings(paste0(sample(words, 1e4, TRUE), sample.int(1e4)))
  high[sample.int(1e4, 50)] <- NA
  # A shared prefix longer than the 8-byte radix window, only after translation for latin1
  prefixed <- mix_encodings(paste0(strrep("é", 6), sample.int(1e4)))
  c1 <- with_encoding(sample(c("\x80", "\x85", "\x99", "\xe9", "\xf1", "a"), 1e4, TRUE), "latin1")

  for (data in list(small, low, high, prefixed, c1)) {
    expect_c_order(data)
    expected <- data[c_order(data)]
    expect_identical(test_sort(data), expected)
    expect_identical(enc_sort(data), expected)
  }
})

test_that("Factors treat encodings as one level", {
  skip_if_not(l10n_info()[["UTF-8"]])

  x <- c(e_utf8, "a", e_latin1, e_native, "a")

  out <- enc_factor(x)
  expect_identical(levels(out), c(e_utf8, "a"))
  expect_identical(as.integer(out), c(1L, 2L, 1L, 1L, 2L))

  out <- enc_factor_levels(x, c("a", e_latin1))
  expect_identical(as.integer(out), c(2L, 1L, 2L, 2L, 1L))

  # The first lookup is a linear scan, later ones use the levels table
  f <- factor(c(e_utf8, "a"), levels = c("a", e_utf8))
  keys <- c(e_latin1, e_native, e_utf8, "a", "zz")
  expect_identical(enc_get_code(f, rep(keys, 2)), rep(c(2L, 2L, 2L, 1L, NA), 2))
  expect_identical(enc_get_code(f, e_latin1), 2L)

  f <- factor(c(e_latin1, "a"), levels = c("a", e_latin1))
  expect_identical(enc_get_code(f, c(e_utf8, e_native)), c(2L, 2L))

  needles <- factor(c(e_utf8, "a", e_latin1), levels = c(e_utf8, "a"))

  # Different levels, so the codes are remapped by text
  haystack <- factor(c("b", "a", e_latin1), levels = c("b", "a", e_latin1))
  expect_identical(enc_match_factors(needles, haystack), c(2L, 1L, 2L))

  # Same levels in a different encoding, so the codes are matched directly
  haystack <- factor(c("a", e_latin1), levels = c(e_latin1, "a"))
  expect_identical(enc_match_factors(needles, haystack), c(1L, 0L, 1L))

  other <- factor(c(e_latin1, "a", e_utf8), levels = c("a", e_latin1))
  expect_identical(enc_factor_eq(needles, other), rep(TRUE, 3))
})

test_that("Name lookup treats encodings as one name", {
  skip_if_not(l10n_info()[["UTF-8"]])

  x <- 1:3
  names(x) <- c("a", e_utf8, with_encoding("\xe7", "latin1"))
  keys <- c(e_latin1, e_native, e_utf8, "ç", "a", "e")
  expected <- c(1L, 1L, 1L, 2L, 0L, NA)

  # The first lookup is a linear scan, later ones use the names table
  expect_identical(enc_name_index(x, rep(keys, 2)), rep(expected, 2))
  for (i in seq_along(keys)) {
    expect_identical(enc_name_index(x, keys[[i]]), expected[[i]])
  }
})

test_that("Strings marked as bytes are never translated", {
  skip_if_not(l10n_info()[["UTF-8"]])

  expect_false(str_eq(e_bytes, e_utf8))
  expect_false(str_eq(e_bytes, e_latin1))
  expect_true(str_eq(e_bytes, with_encoding(e_utf8, "bytes")))

  # Unlike base R these don't error, a bytes string orders by its raw bytes
  expect_false(str_lt(e_bytes, e_utf8))
  expect_true(str_lt(e_latin1, with_encoding("\xe9", "bytes")))

  # The same bytes marked as bytes are not identical, so they sort after the text
  expect_true(str_lt(e_utf8, e_bytes))
  expect_true(str_lt(e_latin1, e_bytes))
  expect_true(str_lt(e_native, e_bytes))
  expect_false(str_le(e_bytes, e_utf8))
  expect_identical(test_order(c(e_utf8, e_bytes, e_latin1, "a")), c(3L, 0L, 2L, 1L))

  # Base unique() gives 3 here, any bytes string switches off its text comparison
  expect_identical(enc_n_unique(c(e_utf8, e_latin1, e_bytes)), 2L)
  expect_identical(enc_group_ids(c(e_utf8, e_latin1, e_bytes)), c(0L, 0L, 1L))
  expect_identical(Encoding(enc_unique(c(e_bytes, e_utf8))), c("bytes", "UTF-8"))
})

test_that("Bytes strings never tie with the same bytes as text", {
  skip_if_not(l10n_info()[["UTF-8"]])
  set.seed(5)

  x <- c(e_bytes, e_utf8, e_bytes, e_utf8)
  expect_identical(test_order(x), c(1L, 3L, 0L, 2L))
  expect_identical(enc_group_ids(x), c(0L, 1L, 0L, 1L))
  expect_identical(enc_ordered_group_ids(x), c(1L, 0L, 1L, 0L))

  # Each string followed by the same bytes as text, so radix alone would tie them
  with_twins <- function(x) {
    c(rbind(with_encoding(enc2utf8(x), "bytes"), mix_encodings(x)))
  }

  expect_twins_order <- function(x) {
    is_bytes <- Encoding(x) == "bytes"
    text <- x
    text[is_bytes] <- with_encoding(x[is_bytes], "UTF-8")
    text <- enc2utf8(text)
    o <- withr::with_collate("C", order(text, is_bytes))
    ids <- integer(length(x))
    ids[o] <- cumsum(!duplicated(paste(text, is_bytes)[o])) - 1L
    expect_identical(test_order(x), o - 1L)
    expect_identical(Encoding(enc_sort(x)), Encoding(x[o]))
    expect_identical(enc_ordered_group_ids(x), ids)
  }

  expect_twins_order(with_twins(words))
  expect_twins_order(with_twins(sample(words, 5e3, TRUE)))
  expect_twins_order(with_twins(paste0(sample(words, 5e3, TRUE), sample.int(5e3))))
})

test_that("r_str compares with UTF-8 C++ strings", {
  skip_if_not(l10n_info()[["UTF-8"]])

  expect_identical(
    enc_eq_utf8_literal(c(e_utf8, e_native, e_latin1, "e", NA)),
    c(TRUE, TRUE, TRUE, FALSE, NA)
  )
  # A bytes string never ties with the same bytes as text
  expect_false(enc_eq_utf8_literal(e_bytes))
  expect_identical(
    enc_lt_utf8_literal(c("a", "z", e_latin1, with_encoding("\xff", "latin1"), NA)),
    c(TRUE, TRUE, FALSE, FALSE, NA)
  )
})

test_that("Encoding accessors report the mark and the locale", {
  skip_if_not(l10n_info()[["UTF-8"]])

  x <- c("a", e_utf8, e_native, e_latin1, e_bytes)

  expect_identical(enc_encoding(c(x, NA)), Encoding(c(x, NA)))
  expect_identical(enc_is_ascii(x), c(TRUE, FALSE, FALSE, FALSE, FALSE))
  expect_identical(enc_is_utf8(x), c(TRUE, TRUE, TRUE, FALSE, FALSE))
  expect_identical(enc_is_latin1(x), c(TRUE, FALSE, FALSE, TRUE, FALSE))
  expect_identical(enc_needs_translation(c(x, NA)), c(FALSE, FALSE, FALSE, TRUE, FALSE, FALSE))
})

test_that("as_utf8 and with_encoding match base R", {
  skip_if_not(l10n_info()[["UTF-8"]])

  x <- c(
    "a", e_utf8, e_native, e_latin1, e_bytes, NA,
    with_encoding(c("\x80", "\x85", "\x99"), "latin1")
  )

  out <- enc_as_utf8(x)
  expected <- enc2utf8(x)
  expect_identical(raw_bytes(out), raw_bytes(expected))
  expect_identical(Encoding(out), Encoding(expected))
  expect_identical(is.na(out), is.na(x))

  for (encoding in c("unknown", "UTF-8", "latin1", "bytes")) {
    out <- enc_with_encoding(x, encoding)
    expected <- with_encoding(x, encoding)
    expect_identical(raw_bytes(out), raw_bytes(expected))
    expect_identical(Encoding(out), Encoding(expected))
    expect_identical(is.na(out), is.na(x))
  }
})

test_that("paste and collapse mark their results like base R", {
  skip_if_not(l10n_info()[["UTF-8"]])

  x <- c(e_latin1, "a", e_native)
  y <- c(e_utf8, e_latin1, "b")

  out <- enc_paste(x, y, "-")
  expected <- paste(x, y, sep = "-")
  expect_identical(out, expected)
  expect_identical(Encoding(out), Encoding(expected))
  expect_identical(raw_bytes(out), raw_bytes(enc2utf8(expected)))

  for (strs in list(c(x, y), c(e_native, "a"))) {
    out <- enc_collapse(strs, "-")
    expected <- paste(strs, collapse = "-")
    expect_identical(out, expected)
    expect_identical(Encoding(out), Encoding(expected))
  }

  # A non-ASCII separator marks the result even when every input is ASCII
  out <- enc_paste("a", "b", e_utf8)
  expected <- paste("a", "b", sep = e_utf8)
  expect_identical(out, expected)
  expect_identical(Encoding(out), Encoding(expected))

  out <- enc_collapse(c("a", "b"), e_utf8)
  expected <- paste(c("a", "b"), collapse = e_utf8)
  expect_identical(out, expected)
  expect_identical(Encoding(out), Encoding(expected))

  # A bytes input makes the result bytes, even alongside UTF-8
  x <- c(e_bytes, e_bytes, e_bytes)
  y <- c("a", e_utf8, e_native)
  out <- enc_paste(x, y, "-")
  expected <- paste(x, y, sep = "-")
  expect_identical(Encoding(out), rep("bytes", 3))
  expect_identical(Encoding(out), Encoding(expected))
  expect_identical(raw_bytes(out), raw_bytes(expected))

  out <- enc_paste(e_bytes, "a", e_utf8)
  expected <- paste(e_bytes, "a", sep = e_utf8)
  expect_identical(Encoding(out), Encoding(expected))
  expect_identical(raw_bytes(out), raw_bytes(expected))

  out <- enc_collapse(c(e_bytes, e_utf8, "a"), "-")
  expected <- paste(c(e_bytes, e_utf8, "a"), collapse = "-")
  expect_identical(Encoding(out), Encoding(expected))
  expect_identical(raw_bytes(out), raw_bytes(expected))
})

test_that("Native strings are translated outside UTF-8 sessions", {
  with_latin1_ctype({
    native <- with_encoding("\xe9", "unknown")

    expect_true(enc_needs_translation(native))
    expect_true(enc_is_latin1(native))
    expect_true(str_eq(native, e_utf8))
    expect_true(str_eq(native, e_latin1))
    expect_identical(enc_n_unique(c(native, e_utf8, e_latin1)), 1L)

    out <- enc_as_utf8(native)
    expect_identical(raw_bytes(out), list(as.raw(c(0xc3, 0xa9))))
    expect_identical(Encoding(out), "UTF-8")
  })
})
