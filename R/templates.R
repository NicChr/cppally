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

# decor cuts the signature at the first `{` and counts parens and `//` inside string
# literals, so these are masked before parsing and restored in the parsed defaults.
# Brace tokens carry parens so decor doesn't split on a comma inside braces
brace_mask <- c(
  "{" = "CPPALLY_LBRACE_(",
  "}" = ")CPPALLY_RBRACE_"
)

literal_mask <- c(
  "{" = "CPPALLY_LBRACE_",
  "}" = "CPPALLY_RBRACE_",
  "(" = "CPPALLY_LPAREN_",
  ")" = "CPPALLY_RPAREN_",
  "/" = "CPPALLY_SLASH_"
)

# Masks up to the `)` that closes the parameter list, comments become a single space
mask_signature <- function(context) {
  depth <- 0L
  in_string <- FALSE
  in_comment <- FALSE
  escaped <- FALSE
  for (i in seq_along(context)) {
    chars <- strsplit(context[[i]], "", fixed = TRUE)[[1]]
    n <- length(chars)
    for (j in seq_len(n)) {
      ch <- chars[[j]]
      nxt <- if (j < n) chars[[j + 1L]] else ""
      if (in_comment) {
        if (ch == "*" && nxt == "/") {
          in_comment <- FALSE
          chars[[j + 1L]] <- ""
        }
        chars[[j]] <- ""
        next
      }
      if (in_string) {
        if (escaped) {
          escaped <- FALSE
        } else if (ch == "\\") {
          escaped <- TRUE
        } else if (ch == '"') {
          in_string <- FALSE
        }
      } else if (ch == '"') {
        in_string <- TRUE
      } else if (ch == "/" && nxt == "/") {
        chars <- c(chars[seq_len(j - 1L)], " ")
        break
      } else if (ch == "/" && nxt == "*") {
        # The `*` is consumed too, so `/*/` is not read as a closed comment
        in_comment <- TRUE
        chars[j:(j + 1L)] <- c(" ", "")
        next
      } else if (ch == "(") {
        depth <- depth + 1L
      } else if (ch == ")") {
        depth <- depth - 1L
        if (depth <= 0L) {
          context[[i]] <- paste(chars, collapse = "")
          return(context)
        }
      }
      mask <- if (in_string) literal_mask else brace_mask
      if (depth > 0L && ch %in% names(mask)) {
        chars[[j]] <- mask[[ch]]
      }
    }
    context[[i]] <- paste(chars, collapse = "")
  }
  context
}

# Paren-carrying brace tokens are restored first, before the bare ones
unmask_signature <- function(x) {
  masks <- c(brace_mask, literal_mask)
  for (i in seq_along(masks)) {
    x <- gsub(masks[[i]], names(masks)[[i]], x, fixed = TRUE)
  }
  x
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
  out <- decor::parse_cpp_function(mask_signature(context), is_attribute = is_attribute)
  out$args[[1]]$default <- unmask_signature(out$args[[1]]$default)
  attr(out, "cpp_template") <- has_template
  out
}
