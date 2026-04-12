
generate_code_template <- function(){
  stop_unless_installed("usethis")
  proj_path <- usethis::proj_get()
  utils::getFromNamespace("use_src", "usethis")()
  cpp_path <- file.path(proj_path, "src", "code.cpp")
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
r_dbl cpp_sum(r_vec<r_dbl> x){
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
