
cpp_decorations <- function(pkg = ".", files = decor::cpp_files(pkg = pkg), is_attribute = TRUE) {

  cpp_attribute_pattern <- function(is_attribute){
    paste0("^[[:blank:]]*", if (!is_attribute)
      "//[[:blank:]]*", "\\[\\[", "[[:space:]]*(.*?)[[:space:]]*",
      "\\]\\]", "[[:space:]]*")
  }

  res <- lapply(files, function(file) {
    if (!file.exists(file)) {
      return(vctrs::data_frame(
        file = character(), line = integer(),
        decoration = character(), params = list(), context = list()
      ))
    }

    lines <- if (is_attribute) readr::read_lines(file) else readLines(file)

    start <- grep(cpp_attribute_pattern(is_attribute), lines)
    if (!length(start)) {
      return(vctrs::data_frame(file = character(), line = integer(),
                               decoration = character(), params = list(), context = list()))
    }

    end <- c(utils::tail(start, -1L) - 1L, length(lines))

    # Adjust 'start' to include preceding 'template <...>' line(s)
    # This effectively pulls the template definition into the 'context'
    # so parse_cpp_function sees it

    real_start <- start
    real_end <- end
    for (i in seq_along(start)) {
      idx <- start[i]
      curr <- idx - 1L
      found <- NA_integer_
      # Scan backwards for a `template <` opener. Unknown lines are tolerated
      # so multi-line template parameter lists (e.g. `T,` / `U>` on their own
      # lines) are traversed; we only give up on blank lines or real code.
      while (curr > 0L) {
        line <- trimws(lines[curr])
        if (stringr::str_detect(line, "^//")){
          curr <- curr - 1L
          next
        }
        if (is_requires_signature(line)){
          curr <- curr - 1L
          next
        }
        if (!nzchar(line)){
          break
        }
        if (is_template_signature(line)){
          found <- curr
          break
        }
        # A `;`, `{`, or `}` at end of line means we've hit real code — stop
        if (stringr::str_detect(line, "[;{}]\\s*$")){
          break
        }
        # Otherwise assume we're still inside a multi-line template parameter list
        curr <- curr - 1L
      }
      if (!is.na(found)) {
        real_start[i] <- found
        # Extending this decoration's start upward steals lines from the
        # previous decoration's end, so shrink it by the same amount.
        n_steps_back <- idx - found
        if (i > 1L) real_end[i - 1L] <- real_end[i - 1L] - n_steps_back
      }
    }

    text <- lines[start]
    content <- sub(paste0(cpp_attribute_pattern(is_attribute), ".*"), "\\1", text)
    decoration <- stringr::str_remove(content, "\\(.*$")
    has_args <- stringr::str_detect(content, "\\(")
    params <- purrr::map_if(content, has_args, function(.x) {
      purrr::set_names(as.list(parse(text = .x)[[1]][-1]))
    })

    # Context uses real start/end
    context <- purrr::map2(real_start, real_end, \(.x, .y) lines[seq(.x, .y)])

    # Remove empty characters (at the end)
    n_empty <- integer(length(context))
    for (j in seq_along(context)){
      fn_line <- context[[j]]
      n_empty[j] <- 0L
      for (i in rev(seq_len(length(fn_line)))){
        if (nzchar(fn_line[i])){
          break
        }
        n_empty[j] <- n_empty[j] + 1L
      }
    }
    context <- purrr::map2(context, n_empty, \(x, y) x[seq_len(length(x) - y)])

    vctrs::data_frame(
      file = file, line = start, decoration = decoration,
      params = params, context = context
    )
  })

  vctrs::vec_rbind(!!!res)
}
