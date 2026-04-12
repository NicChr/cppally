
generate_code_template <- function(){
  dir <- getwd()
  src_dir <- normalizePath(file.path(dir, "src"), winslash = "/")
  cpp_path <- file.path(src_dir, "code.cpp")
  brio::write_lines(c(
    "#include <cpp20.hpp>",
    "using namespace cpp20;",
    "
// Simple example showing how to register a C++ function to R
// You can compile and register this via `cpp20::cpp_source('src/code.cpp')`

// For more info on cpp20 see https://nicchr.github.io/cpp20/index.html
    ",
    '
[[cpp20::register]]
r_dbl sum(r_vec<r_dbl> x){
  r_size_t n = x.length();
  r_dbl out(0);

  for (r_size_t i = 0; i < n; ++i){
    out += x.get(i);
  }
  return out;
}
    '),
cpp_path
  )
}
