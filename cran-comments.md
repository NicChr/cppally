## cppally 0.1.0

This is a new release.

The package was previously submitted under the name `cpp20`. It has been
renamed to `cppally` as I was made aware that the name `cpp20` may be used
by a different project in the future.

## Response to previous review feedback (submission as `cpp20`)

Below I address each point raised by the CRAN team.

* **Title in DESCRIPTION.** Removed the redundant "with R" at the end of the
  title. Title is now "A 'C++20' API for R".

* **Quoting software/API names.** Software names ('C++20', 'C++', 'SIMD')
  are now single-quoted in both the Title and Description fields.

* **Web reference in Description.** Added
  `<https://nicchr.github.io/cppally/>` to the end of the Description field
  using angle brackets for auto-linking.

* **Missing `\value` tags.** Added `\value` to `load_all.Rd` (and verified
  all other exported functions have `\value` documented).

* **`\dontrun{}` usage.** Replaced `\dontrun{}` with `\donttest{}` in
  `cpp_source.Rd`. The example is wrapped in `\donttest{}` because it
  compiles C++ code and may take longer than 5 seconds on some systems.

* **Executable code in vignettes.** The vignettes now execute R code.
  The vignette previously named `cpp20.Rmd` has been renamed to
  `cppally.Rmd` and both it and `protection.Rmd` contain live code chunks.

* **Authors, contributors and copyright holders.** The `Authors@R` field
  now lists all copyright holders for bundled or adapted third-party code,
  with `cph` roles:

  - Martin Leitner-Ankerl — author of the bundled `ankerl::unordered_dense`
    library (MIT), included at `inst/include/ankerl/`.
  - Malte Skarupke — author of the bundled `ska_sort` library
    (Boost Software License 1.0), included at `inst/include/ska_sort/`.
  - Posit Software, PBC — the SEXP protection mechanism in
    `inst/include/cppally/r_protect.h` was inspired by `cpp11::sexp` (MIT).

  An `inst/COPYRIGHTS` file has also been added listing each third-party
  component, its copyright holder, and its license. The original copyright
  and license notices are preserved in the source headers.
