# To be used on decor context
is_template_signature <- function(x){
  if (!is.character(x)){
    cli::cli_abort("{.arg x} must be a {.cls character} vector")
  }
  # Remove whitespace from start/end
  x <- trimws(x)

  # Match template<>
  template_pattern <- "^template\\s*\\<.*\\>$"
  stringr::str_detect(x, template_pattern)
}

# To be used on decor context
is_requires_signature <- function(x){
  if (!is.character(x)){
    cli::cli_abort("{.arg x} must be a {.cls character} vector")
  }
  # Remove whitespace from start/end
  x <- trimws(x)

  # Match requires clause
  # The nice thing about c++ is that
  # it won't allow lines like: requires () {
  # or requires {}
  # which should separate it from being detected as a function
  requires_pattern <- "^requires\\s*.+"
  stringr::str_detect(x, requires_pattern)
}

is_template_arg <- function(type, arg) {
  # Strip keywords using word boundaries (\\b) so we don't mangle type names
  cleaned <- gsub("\\b(const|volatile|struct|class|enum)\\b", "", type, perl = TRUE)

  # Strip pointers, references, and all whitespace
  cleaned <- gsub("[*&\\s]+", "", cleaned, perl = TRUE)

  #  Check Exact Match
  if (cleaned == arg) {
    return(TRUE)
  }

  pattern <- paste0("(<|,)", arg, "(>|,)")
  stringr::str_detect(cleaned, pattern)
}


get_template_params <- function(context) {
  # Extract content between template < ... >
  # Handles "template <typename T, RVector U>"
  pattern <- "template\\s*<([^>]+)>"
  match <- regmatches(context, regexpr(pattern, context))

  if (length(match) == 0) return(character(0))

  # Remove 'template <' and '>'
  inner <- sub("template\\s*<", "", sub(">$", "", match))

  # Split by comma
  parts <- strsplit(inner, ",")[[1]]

  # Extract the last word of each part (the variable name)
  # e.g., "typename T" -> "T", "RVector U" -> "U"
  params <- trimws(sub(".*\\s+(\\w+)$", "\\1", parts))
  params
}

parse_cpp_function <- function(context, is_attribute = TRUE){

  # Remove lines containing template or requires clauses
  is_template_clause <- is_template_signature(context)
  is_requires_clause <- is_requires_signature(context)

  context <- context[!is_template_clause & !is_requires_clause]
  out <- decor::parse_cpp_function(context, is_attribute = is_attribute)
  attr(out, "cpp_template") <- any(is_template_clause)
  out

}


