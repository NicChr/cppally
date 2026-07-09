# These tests compile throwaway units via cpp_source() so they can toggle the
# CPPALLY_COPY_ON_MODIFY flag, which is otherwise invisible in the default build.
# skip_if_cannot_cpp_source() lives in helper-cpp-source.R.

# A by-value in-place mutator: reverses `x` and returns it.
reverse_src <- '
  #include <cppally/r_vec.h>
  using namespace cppally;

  [[cppally::register]]
  r_vec<r_int> com_reverse(r_vec<r_int> x){
    x.rev();
    return x;
  }
'

test_that("copy-on-modify preserves the caller's vector", {
  skip_if_cannot_cpp_source()

  env <- new.env()
  cppally::cpp_source(
    code = reverse_src,
    copy_on_modify = TRUE,
    env = env,
    debug = TRUE
  )

  x   <- c(1L, 2L, 3L, 4L)
  y <- x
  out <- env$com_reverse(x)

  expect_identical(out, c(4L, 3L, 2L, 1L))   # the return carries the mutation
  expect_identical(x, c(1L, 2L, 3L, 4L))     # ... but the caller is untouched
  expect_identical(x, y)
})

test_that("without copy-on-modify, in-place mutation reaches the caller", {
  skip_if_cannot_cpp_source()

  env <- new.env()
  cppally::cpp_source(
    code = reverse_src,
    copy_on_modify = FALSE,
    env = env,
    debug = TRUE
  )

  x   <- c(1L, 2L, 3L, 4L)
  y <- x
  out <- env$com_reverse(x)

  expect_identical(out, c(4L, 3L, 2L, 1L))
  expect_identical(x, c(4L, 3L, 2L, 1L)) # x was mutated by reference
  expect_identical(y, c(4L, 3L, 2L, 1L)) # y also mutated by reference
})

test_that("copy-on-modify still mutates in place when the wrapper is sole owner", {
  skip_if_cannot_cpp_source()

  # A freshly allocated vector inside C++ is exclusively owned, so COM should
  # mutate it directly (no wasted copy) and return the correct result.
  src <- '
    #include <cppally/r_vec.h>
    using namespace cppally;

    [[cppally::register]]
    r_vec<r_int> com_iota_reversed(r_int n){
      r_vec<r_int> out(unwrap(n));
      for (r_size_t i = 0; i < out.length(); ++i){
        out.set(i, r_int(static_cast<int>(i) + 1));
      }
      out.rev();
      return out;
    }
  '
  env <- new.env()
  cppally::cpp_source(
    code = src,
    copy_on_modify = TRUE,
    env = env,
    debug = TRUE
  )

  expect_identical(env$com_iota_reversed(5L), c(5L, 4L, 3L, 2L, 1L))
})
