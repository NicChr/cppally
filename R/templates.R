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

# decor cuts the signature at the first `{` or `;` line and counts parens, angle brackets and `//` even inside string literals.
# So literals are swapped whole for placeholders,
# and the remaining characters decor misreads are masked, both restored after parsing.
# Brace tokens carry parens so decor doesn't split on a comma inside braces
brace_mask <- c(
  "{" = "CPPALLY_LBRACE_(",
  "}" = ")CPPALLY_RBRACE_"
)

# A `;` in a lambda body would otherwise end the signature early
top_mask <- c(
  brace_mask,
  ";" = "CPPALLY_SEMI_"
)

# Commas inside nested parens are already protected, so `<` `>` there (e.g. `1 << 4`) are masked
nested_mask <- c(
  top_mask,
  "<" = "CPPALLY_LT_",
  ">" = "CPPALLY_GT_"
)

# Masks up to the `)` that closes the parameter list, comments become a single space.
# String and char literals become `CPPALLY_LIT_<i>_`, the originals kept in attr "literals"
mask_signature <- function(context) {

  # depth goes up at each ( and down at each )
  depth <- 0L

  # Are we currently in a string, e.g. "string"
  in_string <- FALSE

  # Are we currently in a char, e.g. 'char'
  in_char <- FALSE

  # Are we currently in a C++ block comment, e.g. /* comment */
  in_comment <- FALSE

  # Is the character escaped?
  escaped <- FALSE

  # Original text of each literal swapped for a placeholder
  literals <- character()

  for (i in seq_along(context)) {
    chars <- strsplit(context[[i]], "", fixed = TRUE)[[1]]

    # Unmasked copy, for the previous char and the literal text
    original <- chars

    n <- length(chars)
    literal_start <- 0L

    for (j in seq_len(n)) {

      curr_char <- chars[[j]] # Current char

      if (j < n){
        next_char <- chars[[j + 1L]]
      } else {
        next_char <- ""
      }
      if (j > 1L){
        prev_char <- original[[j - 1L]]
      } else {
        prev_char <- ""
      }

      # ----- Inside a C++ block comment -----

      if (in_comment) {

        # Block comment chars are dropped, the opening `/*` already left a space
        if (curr_char == "*" && next_char == "/") {
          in_comment <- FALSE
          chars[[j + 1L]] <- ""
        }
        chars[[j]] <- ""
        next
      }

      # ----- Inside a C++ string or char -----

      if (in_string || in_char) {
        if (escaped) {
          escaped <- FALSE
        } else if (curr_char == "\\") {
          escaped <- TRUE
        } else if ((in_string && curr_char == '"') || (in_char && curr_char == "'")) {
          in_string <- FALSE
          in_char <- FALSE
          if (depth > 0L) {
            literals <- c(literals, paste(original[literal_start:j], collapse = ""))
            chars[literal_start:j] <- ""
            chars[[literal_start]] <- paste0("CPPALLY_LIT_", length(literals), "_")
          }
        }
        next
      }

      # ----- Start of a C++ string -----

      if (curr_char == '"') {
        in_string <- TRUE
        literal_start <- j

        # ----- Start of a C++ char -----

        # C++ treats ' as either the start/end of a char or a digit separator (e.g. 10'000)
        # If the char before ' is a letter, digit or underscore it's a separator, otherwise it starts a char

      } else if (curr_char == "'" && !grepl("[[:alnum:]_]", prev_char)) {
        in_char <- TRUE
        literal_start <- j

        # ----- Start of a C++ line comment -----

      } else if (curr_char == "/" && next_char == "/") {
        chars <- c(chars[seq_len(j - 1L)], " ")
        break

        # ----- Start of a C++ block comment -----

      } else if (curr_char == "/" && next_char == "*") {
        # The `*` is consumed too, so `/*/` is not read as a closed comment
        in_comment <- TRUE
        chars[j:(j + 1L)] <- c(" ", "")
        next
      } else if (curr_char == "(") {
        depth <- depth + 1L
      } else if (curr_char == ")") {
        depth <- depth - 1L
        # If we're at the top then we are at the end of the arguments list
        if (depth <= 0L) {
          context[[i]] <- paste(chars, collapse = "")
          return(structure(context, literals = literals))
        }
      }
      if (depth > 1L){
        mask <- nested_mask
      } else {
        mask <- top_mask
      }
      if (depth > 0L && curr_char %in% names(mask)) {
        chars[[j]] <- mask[[curr_char]]
      }
    }
    # C++ literals can't span lines, so an un-terminated one was misread, e.g. a raw string
    if (in_string || in_char) {
      stop("Unterminated string or char literal (raw strings aren't supported).", call. = FALSE)
    }
    context[[i]] <- paste(chars, collapse = "")
  }
  structure(context, literals = literals)
}

# Literals are restored last, so their contents are never unmasked
unmask_signature <- function(x, literals) {
  for (i in seq_along(nested_mask)) {
    x <- gsub(nested_mask[[i]], names(nested_mask)[[i]], x, fixed = TRUE)
  }
  for (i in seq_along(literals)) {
    x <- gsub(paste0("CPPALLY_LIT_", i, "_"), literals[[i]], x, fixed = TRUE)
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
  out <- tryCatch(
    {
      masked <- mask_signature(context)
      decor::parse_cpp_function(masked, is_attribute = is_attribute)
    },
    error = function(e) {
      signature <- trimws(sub("^\\s*\\[\\[.*?\\]\\]", "", context, perl = TRUE))
      signature <- signature[nzchar(signature)][1]
      cli::cli_abort(
        c("Can't parse the signature of a registered function.", "x" = "{.code {signature}}"),
        parent = e
      )
    }
  )
  literals <- attr(masked, "literals")
  out$args[[1]]$type <- unmask_signature(out$args[[1]]$type, literals)
  out$args[[1]]$default <- unmask_signature(out$args[[1]]$default, literals)
  attr(out, "cpp_template") <- has_template
  out
}
