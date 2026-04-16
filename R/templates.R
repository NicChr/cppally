# Detects the opening line of a template declaration (single- or multi-line).
# `template` is a reserved keyword, so a line starting with it (as a whole
# word) can only begin a template
is_template_signature <- function(x){
  stringr::str_detect(x, "^\\s*template\\b")
}

# To be used on decor context
is_requires_signature <- function(x){
  # The nice thing about c++ is that it won't allow lines like:
  # `requires () {` or `requires {}`, which separates it from a function.
  stringr::str_detect(x, "^\\s*requires\\b")
}

is_template_arg <- function(type, arg) {
  # Strip keywords using word boundaries (\\b) so we don't mangle type names
  cleaned <- gsub("\\b(const|volatile|struct|class|enum)\\b", "", type, perl = TRUE)

  # Strip pointers, references, and all whitespace
  cleaned <- gsub("[*&\\s]+", "", cleaned, perl = TRUE)

  pattern <- paste0("(<|,)", arg, "(>|,)")
  cleaned == arg | stringr::str_detect(cleaned, pattern)
}

get_template_params <- function(context) {
  # Extract content between template < ... >
  # Handles "template <typename T, RVector U>"
  pattern <- "template\\s*<([^>]+)>"
  match <- regmatches(context, regexpr(pattern, context, perl = TRUE))

  if (length(match) == 0) return(character(0))

  # Remove 'template <' and '>'
  inner <- sub("template\\s*<", "", sub(">$", "", match, perl = TRUE), perl = TRUE)

  # Split by comma and trim whitespace (trimming BEFORE the regex is critical,
  # since the `$` anchor in the extractor fails if there's trailing whitespace)
  parts <- trimws(strsplit(inner, ",", perl = TRUE)[[1]])

  # Extract the last word of each part (the variable name)
  # e.g., "typename T" -> "T", "RVector U" -> "U"
  sub(".*\\s+(\\w+)$", "\\1", parts, perl = TRUE)
}

parse_cpp_function <- function(context, is_attribute = TRUE){
  # Strip any template/requires preamble by dropping everything before the
  # [[ attribute line. Only called with is_attribute = TRUE (see register.R).
  attr_idx <- which(grepl("^\\s*\\[\\[", context, perl = TRUE))[1]
  has_template <- FALSE
  if (!is.na(attr_idx) && attr_idx > 1L) {
    preamble <- context[seq_len(attr_idx - 1L)]
    has_template <- any(is_template_signature(preamble))
    context <- context[attr_idx:length(context)]
  }
  out <- decor::parse_cpp_function(context, is_attribute = is_attribute)
  attr(out, "cpp_template") <- has_template
  out
}
