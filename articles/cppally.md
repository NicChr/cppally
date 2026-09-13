# Getting started with cppally

Below we explore some of the capabilities of cppally, from its custom
C++ scalar and vectors, to using templates and concepts.

**Note:** The classes `r_vec` and `r_vector` are aliases of one another
and thus can be used interchangeably.

## Setup

Let’s start by loading cppally

``` r

library(cppally)
```

## Registering C++ functions (to R)

To make a C++ function available to R we use the `[[cppally::register]]`
tag.

``` cpp
#include <cppally.hpp>
using namespace cppally;

[[cppally::register]]
void hello_world(){
  print("Hello World!");
}
```

After tagging our functions we want to make them available to R. To do
that we have a few routes.

### Registering C++ functions outside of a package context

After writing our hello world program in foo.cpp we can use
[`cpp_source()`](https://nicchr.github.io/cppally/reference/cpp_source.md)
to compile and register the function to R.

``` r

cpp_source(file = "src/foo.cpp")
```

Now the function is available in R

``` r

hello_world()
#> Hello World!
```

Similarly we can use the helper `cpp_eval` to run simple expressions and
return the result without needing to include cppally.hpp and register
the function.

``` r

cpp_eval('print("Hello World Again!")')
#> Hello World Again!
```

**Note** - For the rest of the examples it is assumed that the following
code is always included beforehand.

``` cpp
#include <cppally.hpp>
using namespace cppally;
```

### Registering C++ functions inside a cppally-linked package

Since cppally is header-only, we can include the headers directly into
our own package.

### General steps to using cppally in a package

1.  Create package (if you haven’t already done so) using
    [`usethis::create_tidy_package()`](https://usethis.r-lib.org/reference/tidyverse.html)
2.  Run
    [`cppally::use_cppally()`](https://nicchr.github.io/cppally/reference/use_cppally.md)
3.  Run
    [`cppally::document()`](https://nicchr.github.io/cppally/reference/document.md)

This will automatically add the necessary package content needed to
start working with cppally. For continuous development, use
[`cppally::load_all()`](https://nicchr.github.io/cppally/reference/load_all.md)
to compile and register cppally tagged functions, including our hello
world function.

**Note:** We aim to integrate cppally registration into the `devtools`
framework for ease-of-use.

## C++ types

cppally offers a rich set of R types in C++ that are NA-aware. This
means that common arithmetic and logical operations will account for
`NA` in a similar fashion to R.

## Scalars

### logical scalar - `r_lgl`

cppally’s scalar version of `logical`, `r_lgl` can represent true, false
or NA.

``` cpp
r_true
r_false
r_na
```

    #> [1] TRUE
    #> [1] FALSE
    #> [1] NA

Logical operators work just like in R

``` cpp

[[cppally::register]]
r_vector<r_lgl> lgl_ops(){
  return make_vec<r_lgl>(
    r_true || r_false, // true
    r_true && r_false, // false
    r_na || r_true,    // true
    r_na && r_true,    // NA
    r_na && r_false,   // false
    r_na || r_na,      // NA
    r_na && r_na      // NA
  );
}
```

``` r

lgl_ops()
#> [1]  TRUE FALSE  TRUE    NA FALSE    NA    NA
```

**Using `r_lgl`** in if-statements

For type-safety reasons `r_lgl` cannot be implicitly converted to `bool`
except in if-statements where an error is thrown if the value is `NA`.

**DON’T** do this:

``` cpp

[[cppally::register]]
void bad_lgl_print(r_lgl condition){
  if (condition){
    print("true");
  } else {
    print("false");
  }
}
```

``` r

bad_lgl_print(TRUE)
#> true
bad_lgl_print(FALSE)
#> false
bad_lgl_print(NA) # Can't implicitly convert NA to bool
#> Error:
#> ! Cannot implicitly convert r_lgl NA to bool, please check
```

**DO** this:

``` cpp

[[cppally::register]]
void good_lgl_print(r_lgl condition){
  if (is_na(condition)){
    print("NA");
  } else if (condition){
    print("true");
  } else {
    print("false");
  }
}
```

``` r

good_lgl_print(TRUE)
#> true
good_lgl_print(FALSE)
#> false
good_lgl_print(NA) # NA is handled explicitly so no issues
#> NA
```

We can also use `r_lgl` members `is_true()` and `is_false()` which
return `bool` and are equivalent to R’s
[`isTRUE()`](https://rdrr.io/r/base/Logic.html) and
[`isFALSE()`](https://rdrr.io/r/base/Logic.html)

``` cpp

[[cppally::register]]
void also_good_lgl_print(r_lgl condition){
  if (condition.is_true()){
    print("true");
  } else {
    print("not true");
  }
}
```

``` r

also_good_lgl_print(TRUE)
#> true
also_good_lgl_print(FALSE)
#> not true
also_good_lgl_print(NA) # Falls into 'not true' branch here as expected
#> not true
```

**Important:** The `&&` and `||` operators for `r_lgl` do **NOT
short-circuit** like they do for `bool`. Both sides of the expression
are always evaluated. If you specifically require short-circuiting
behaviour, use `is_true()` and `is_false()` before using `&&` and `||`.

All cppally scalar types are implemented as structs that contain the
underlying C/C++ types as well as other member functions.

| cppally type | Description                 | Built on               |
|:-------------|-----------------------------|:-----------------------|
| `r_lgl`      | Scalar logical              | `int`                  |
| `r_int`      | Scalar integer              | `int`                  |
| `r_int64`    | Scalar 64-bit integer       | `int64_t`              |
| `r_dbl`      | Scalar double               | `double`               |
| `r_str`      | Scalar string               | `r_sexp`               |
| `r_str_view` | Scalar string (view)        | `SEXP`                 |
| `r_cplx`     | Scalar double complex       | `std::complex<double>` |
| `r_raw`      | Scalar raw                  | `unsigned char`        |
| `r_sym`      | Symbol                      | `SEXP`                 |
| `r_date`     | Scalar date                 | `r_dbl`                |
| `r_psxct`    | Scalar date-time            | `r_dbl`                |
| `r_sexp`     | Generic R object (SEXP)[^1] | `SEXP`                 |

### NA values

Use `is_na()` to check that a value is NA and `na<T>()` to generate NA
values.

``` cpp
is_na(na<r_int>())
```

    #> [1] TRUE

### C++ NA values and their R C API equivalents

[TABLE]

## Checking equality

There are two ways to check for exact equality of cppally scalars - with
the `==` operator or with
[`identical()`](https://rdrr.io/r/base/identical.html).

The cppally `==` operator always returns `r_lgl` and
[`identical()`](https://rdrr.io/r/base/identical.html) always returns
`bool`, which is a particularly important distinction when dealing with
`NA` values as the former can represent `NA` while the latter cannot.

``` cpp

[[cppally::register]]
void cppally_equality(){

  r_int x = na<r_int>();
  r_int y = na<r_int>();
  
  r_lgl x_equal_to_y = x == y;
  bool x_identical_to_y = identical(x, y);
  
  // NA so not printed
  if ( x_equal_to_y.is_true() ){
    print("x is equal to y\n");
  }
  
  // NA so not printed
  if ( x_equal_to_y.is_false() ){
    print("x is not equal to y\n");
  }
  
  // NA so printed
  if (is_na(x_equal_to_y)){
    print("`x == y` produces `NA`\n");
  }
  
  // Both na<r_int>() therefore they are identical to each other
  if (x_identical_to_y){
    print("x is identical to y\n");
  }
}
```

``` r

cppally_equality()
#> `x == y` produces `NA`
#> x is identical to y
```

[`identical()`](https://rdrr.io/r/base/identical.html) can not only
compare scalars, but also vectors, lists, factors, and data frames.

``` cpp

template <typename T, typename U>
[[cppally::register]]
bool cpp_identical(T x, U y){
  return identical(x, y);
}
```

``` r

cpp_identical(3L, 3L)
#> [1] TRUE
cpp_identical(NA, NA)
#> [1] TRUE
cpp_identical(3L, 3) # int != double
#> [1] FALSE
cpp_identical(1:10, 1:10)
#> [1] TRUE
cpp_identical(list(1, 2, 3), list(3, 2, 1))
#> [1] FALSE
cpp_identical(iris, iris)
#> [1] TRUE
```

## Scalar operators

cppally also defines arithmetic and relational comparison operators for
its scalar types. Like the logical and equality operators seen earlier,
they are all `NA`-aware.

### Scalar arithmetic operators

**Addition**

``` cpp
r_int(0) + r_dbl(2.5)
```

    #> [1] 2.5

**Subtraction**

``` cpp
r_int(0) - r_int(1)
```

    #> [1] -1

**Multiplication**

``` cpp
r_int(2) * r_int(3)
```

    #> [1] 6

**Division**

``` cpp
r_dbl(9) / 3
```

    #> [1] 3

**Addition with `NA`**

``` cpp
na<r_int>() + r_dbl(2.5)
```

    #> [1] NA

**Subtraction with `NA`**

``` cpp
na<r_int>() - r_int(1)
```

    #> [1] NA

**Multiplication with `NA`**

``` cpp
na<r_int>() * r_int(3)
```

    #> [1] NA

**Division with `NA`**

``` cpp
na<r_dbl>() / 3
```

    #> [1] NA

### Scalar relational operators

**Less than**

``` cpp
r_int(1) < r_int(2)
```

    #> [1] TRUE

**Less than or equal to**

``` cpp
r_dbl(2) <= r_dbl(2)
```

    #> [1] TRUE

**Greater than**

``` cpp
r_int(3) > r_int(2)
```

    #> [1] TRUE

**Greater than or equal to**

``` cpp
r_dbl(2) >= r_dbl(3)
```

    #> [1] FALSE

**Less than with `NA`**

``` cpp
na<r_int>() < r_int(2)
```

    #> [1] NA

**Less than or equal to with `NA`**

``` cpp
na<r_dbl>() <= r_dbl(2)
```

    #> [1] NA

**Greater than with `NA`**

``` cpp
na<r_int>() > r_int(2)
```

    #> [1] NA

**Greater than or equal to with `NA`**

``` cpp
na<r_dbl>() >= r_dbl(3)
```

    #> [1] NA

Other defined operators not showcased: `++`, `--`, `+=`, `-=`, `*=`,
`/=`, `%=`, `-`, `%`, `|`, `&`, `!`

## Vectors

cppally vectors are templated and can be thought of as containers of
scalar elements like `r_int`, `r_dbl`, etc.

We can create vectors like so

``` cpp

// Integer vector of size n
[[cppally::register]]
r_vector<r_int> new_integer_vector(int n){
  r_vector<r_int> int_vctr(n, /*fill = */ r_int(0));
  return int_vctr;
}
```

``` r

new_integer_vector(3)
#> [1] 0 0 0
```

### inline vectors

To create inline vectors, use `make_vec<>`

``` cpp
make_vec<r_dbl>(1, 1.5, 2, na<r_dbl>())
```

    #> [1] 1.0 1.5 2.0  NA

We can add names on the fly with `arg()`

``` cpp

make_vec<r_dbl>(
    arg("first") = 1,
    arg("second") = 1.5,
    arg("third") = 2,
    arg("last") = na<r_dbl>()
  )
```

    #>  first second  third   last 
    #>    1.0    1.5    2.0     NA

In R a list is a generic vector, so cppally defines lists as
`r_vector<r_sexp>`, a vector of the generic type `r_sexp`.

``` cpp
make_vec<r_sexp>(1, 2, 3)
```

    #> [[1]]
    #> [1] 1
    #> 
    #> [[2]]
    #> [1] 2
    #> 
    #> [[3]]
    #> [1] 3

A list of all cppally vectors of length 0

``` cpp

[[cppally::register]]
r_vector<r_sexp> all_vectors(){
  return make_vec<r_sexp>(
    arg("logical") = r_vector<r_lgl>(),
    arg("integer") = r_vector<r_int>(),
    arg("integer64") = r_vector<r_int64>(), // Requires bit64
    arg("double") = r_vector<r_dbl>(),
    arg("character") = r_vector<r_str>(),
    arg("character") = r_vector<r_str_view>(),
    arg("raw") = r_vector<r_raw>(),
    arg("date") = r_vector<r_date>(),
    arg("date-time") = r_vector<r_psxct>(),
    arg("list") = r_vector<r_sexp>()
  );
}
```

``` r

all_vectors()
#> $logical
#> logical(0)
#> 
#> $integer
#> integer(0)
#> 
#> $integer64
#> integer64(0)
#> 
#> $double
#> numeric(0)
#> 
#> $character
#> character(0)
#> 
#> $character
#> character(0)
#> 
#> $raw
#> raw(0)
#> 
#> $date
#> Date of length 0
#> 
#> $`date-time`
#> POSIXct of length 0
#> 
#> $list
#> list()
```

## Scalar math

There is a rich suite of math functions that accept cppally types.

``` cpp

[[cppally::register]]
r_vector<r_dbl> cppally_math(r_dbl x){
    return make_vec<r_dbl>(
        arg("abs")          = abs(x),
        arg("floor")        = floor(x),
        arg("ceiling")      = ceiling(x),
        arg("trunc")        = trunc(x),
        arg("round")        = round(x),
        arg("signif")       = signif(x, 3),
        arg("sign")         = sign(x),
        arg("min")          = min(0, x),
        arg("max")          = max(0, x),
        arg("sqrt")         = sqrt(x),
        arg("pow")          = pow(x, 2),
        arg("exp")          = exp(x),
        arg("log")          = log(x),
        arg("log_base")     = log(x, 2),
        arg("log10")        = log10(x)
    );
}
```

``` r

cppally_math(2.5)
#>        abs      floor    ceiling      trunc      round     signif       sign 
#>  2.5000000  2.0000000  3.0000000  2.0000000  2.0000000  2.5000000  1.0000000 
#>        min        max       sqrt        pow        exp        log   log_base 
#>  0.0000000  2.5000000  1.5811388  6.2500000 12.1824940  0.9162907  1.3219281 
#>      log10 
#>  0.3979400
cppally_math(NA)
#>      abs    floor  ceiling    trunc    round   signif     sign      min 
#>       NA       NA       NA       NA       NA       NA       NA       NA 
#>      max     sqrt      pow      exp      log log_base    log10 
#>       NA       NA       NA       NA       NA       NA       NA
```

## Coercion

To coerce from one scalar to another we can use `as<T>`

``` cpp

[[cppally::register]]
r_int double_to_int(r_dbl x){
  return as<r_int>(x);
}
```

``` r

double_to_int(pi)
#> [1] 3
double_to_int(NA_real_)
#> [1] NA
```

We can also coerce from one vector type to another

``` cpp

[[cppally::register]]
r_vector<r_int> to_int_vec(r_vector<r_dbl> x){
  return as<r_vector<r_int>>(x);
}
```

``` r

to_int_vec(c(0, 1.5, NA))
#> [1]  0  1 NA
```

Since `as<T>` is extremely flexible, we can also coerce from a scalar to
a vector or vice versa

``` cpp

[[cppally::register]]
r_vector<r_sexp> coercions(){
    r_dbl a(4.2);
    r_vector<r_dbl> b = make_vec<r_dbl>(2.5);
    return make_vec<r_sexp>(
        as<r_vector<r_int>>(a),
        as<r_int>(a),
        as<r_int>(b),
        as<r_dbl>(b)
    );
}
```

``` r

coercions()
#> [[1]]
#> [1] 4
#> 
#> [[2]]
#> [1] 4
#> 
#> [[3]]
#> [1] 2
#> 
#> [[4]]
#> [1] 2.5
```

We can even coerce to and from C++ vectors

``` cpp

[[cppally::register]]
r_vector<r_int> cpp_vectors_example(r_vector<r_int> x){
  std::vector x_cpp = as<std::vector<r_int>>(x);
  x_cpp.push_back(r_int(42));
  return as<r_vector<r_int>>(x_cpp);
}
```

``` r

cpp_vectors_example(41L)
#> [1] 41 42
```

While coercing to a `std::vector` just to push back an element before
coercing back might not be the most efficient, it does showcase how easy
it is to work with cppally vectors and C++ vectors.

## Strings

cppally provides the useful string type `r_str`

We can create R strings easily

``` cpp
r_str("hello")
```

    #> [1] "hello"

To get a C or C++ string, use the members `c_str()` and `cpp_str()`
respectively

C string via `c_str()`

``` cpp
r_str("hello").c_str()
```

    #> [1] "hello"

C++ string_view via `cpp_str()`

This can be converted into a std::string via its constructor

``` cpp

[[cppally::register]]
r_str str_concatenate(r_str x, r_str y, r_str sep){
  std::string left = std::string(x.cpp_str());
  std::string right = std::string(y.cpp_str());
  std::string middle = std::string(sep.cpp_str());
  std::string combined = left + middle + right;
  return r_str(combined.c_str());
}
```

``` r

str_concatenate("hello", "how are you?", sep = ", ")
#> [1] "hello, how are you?"
```

## Dates and date-times

cppally offers two classes: `r_date` for dates and `r_psxct` for
date-times (based on POSIX time). These classes offer a rich set of
member functions to perform date arithmetic and manipulation.

To quickly re-familiarise ourselves - R Dates and Date-times are
essentially double-valued offsets from Unix time. Unix time is defined
as the number of seconds since 1 Jan 1970, and so `r_date` is
effectively an offset representing the number of days since epoch, and
similarly `r_psxct` is an offset representing the number of seconds
since epoch.

To create a new `r_date`, use the `r_date(year, month, day)`
constructor.

``` cpp

[[cppally::register]]
r_date create_date(int year, int month, int day){
  return r_date(year, month, day);
}
[[cppally::register]]
r_date date_from_epoch_offset(double n){
  return r_date(n);
}

[[cppally::register]]
r_psxct create_datetime(int year, int month, int day, int hour, int minute, double second){
  return r_psxct(year, month, day, hour, minute, second);
}
[[cppally::register]]
r_psxct datetime_from_epoch_offset(double n){
  return r_psxct(n);
}
```

``` r

create_date(1970, 1, 1); # This builds the Unix epoch date
#> [1] "1970-01-01"
```

You can also build an `r_date` from an epoch offset.

``` r

date_from_epoch_offset(0) # Also builds the Unix epoch
#> [1] "1970-01-01"
```

Similarly, to create a new `r_psxct`, use the
`r_psxct(year, month, day, hour, minute, second)` constructor.

``` r

create_datetime(1970, 1, 1, 0, 0, 0); # Unix epoch date-time
#> [1] "1970-01-01 UTC"
datetime_from_epoch_offset(0)         # Unix epoch from an offset
#> [1] "1970-01-01 UTC"
```

To get the current date and date-time, use `r_date::today()` and
`r_psxct::now()` respectively.

``` cpp

[[cppally::register]]
r_date get_today(){
  return r_date::today();
}
[[cppally::register]]
r_psxct get_now(){
  return r_psxct::now();
}
```

``` r

get_today()
#> [1] "2026-09-13"
get_now()
#> [1] "2026-09-13 08:25:22 UTC"
```

**Note:** `r_psxct` currently only supports UTC and no other time-zones.

#### Adding days, months, and years

`r_date` and `r_psxct` both have the template function `add<>`, which
allows us to add years, months, weeks, days, hours, minutes and seconds
to our dates and date-times.

``` cpp

[[cppally::register]]
r_date add_days(r_date x, double n){
  return x.add<"days">(n);
}
[[cppally::register]]
r_date add_months(r_date x, double n){
  return x.add<"months">(n);
}
```

``` r

y2k <- create_date(2000, 1, 1)

# Day after y2k
y2k |>
  add_days(1)
#> [1] "2000-01-02"

# Week after y2k
y2k |>
  add_days(7)
#> [1] "2000-01-08"

# Month after y2k
y2k |>
  add_months(1)
#> [1] "2000-02-01"

# Year after y2k
y2k |>
  add_months(12)
#> [1] "2001-01-01"
```

### Accessing date components

Those familiar with lubridate will instantly be familiar with the naming
of these functions, something that was an intentional design choice to
reduce confusion for those who are familiar with tidyverse.

``` cpp

[[cppally::register]]
r_int get_year(r_date x){
  return x.year();
}
[[cppally::register]]
r_int get_month(r_date x){
  return x.month();
}
[[cppally::register]]
r_int get_week(r_date x){
  return x.week();
}
[[cppally::register]]
r_int get_day(r_date x){
  return x.day();
}
[[cppally::register]]
r_int get_wday(r_date x, int week_start){
  return x.wday(week_start);
}
[[cppally::register]]
r_int get_yday(r_date x){
  return x.yday();
}
[[cppally::register]]
r_int get_iso_week(r_date x){
  return x.iso_week();
}
[[cppally::register]]
r_int get_iso_year(r_date x){
  return x.iso_year();
}
```

``` r

get_year(y2k)
#> [1] 2000
get_month(y2k)
#> [1] 1
get_week(y2k)
#> [1] 1
get_day(y2k) # Day of the month
#> [1] 1
get_wday(y2k, week_start = 1) # week_start = 1 = Mon
#> [1] 6
get_yday(y2k)
#> [1] 1
```

We can even retrieve the ISO week and ISO year.

``` r

get_iso_week(y2k) # Last (52nd) ISO week of the ISO year
#> [1] 52
get_iso_year(y2k) # ISO year is still 1999
#> [1] 1999
```

### Date rounding

We can also round dates using the member functions
[`floor()`](https://rdrr.io/r/base/Round.html),
[`ceiling()`](https://rdrr.io/r/base/Round.html), and
[`round()`](https://rdrr.io/r/base/Round.html).

``` cpp

[[cppally::register]]
r_date floor_month(r_date x){
  return x.floor<"months">();
}
[[cppally::register]]
r_date ceiling_year(r_date x){
  return x.ceiling<"years">();
}
[[cppally::register]]
r_date round_week(r_date x, int week_start){
  return x.round<"weeks">(week_start);
}
```

``` r

# Start of the month after advancing 100 days
y2k |> 
  add_days(100) |> 
  floor_month()
#> [1] "2000-04-01"

# ceiling() does not switch on boundary
y2k |> 
  ceiling_year()
#> [1] "2000-01-01"

y2k |> 
  add_days(1) |> 
  ceiling_year()
#> [1] "2001-01-01"

y2k |> 
  round_week(week_start = 1)
#> [1] "2000-01-03"
```

### Difference between two dates

To calculate the time difference between two dates or date-times, use
`time_diff<>`.

``` cpp

[[cppally::register]]
r_dbl diff_days(r_date x, r_date y){
  return time_diff<"days">(x, y);
}
[[cppally::register]]
r_dbl diff_months(r_date x, r_date y){
  return time_diff<"months">(x, y);
}
```

``` r

x <- y2k
y <- x |> add_months(3)

diff_days(x, y)
#> [1] 91
diff_months(x, y)
#> [1] 3
```

### Fractional months

Both `add<"months">` and `time_diff<"months">` can deal with fractional
months.

The previous sections dealt with dates, so we need to define functions
that work with `r_psxct` (date-times).

``` cpp

[[cppally::register]]
r_psxct as_dt(r_date x){
  return x.as_datetime();
}
[[cppally::register]]
r_psxct dt_add_months(r_psxct x, double n){
  return x.add<"months">(n);
}
[[cppally::register]]
r_dbl dt_diff_months(r_psxct x, r_psxct y){
  return time_diff<"months">(x, y);
}
```

``` r

y2k_datetime <- as_dt(y2k)

mid_month <- y2k_datetime |> 
  dt_add_months(0.5)

# Half-way through the month 
mid_month
#> [1] "2000-01-16 12:00:00 UTC"
```

The difference between the start of the month and halfway through the
month should return 0.5, confirming that fractional month arithmetic is
working.

``` r

dt_diff_months(y2k_datetime, mid_month)
#> [1] 0.5
```

## Symbols

Symbols have class `r_sym` and can be created directly from a string
literal

``` cpp
r_sym("new_symbol")
```

    #> new_symbol

Or from a cppally string

``` cpp
r_sym(r_str("symbol_from_string"))
```

    #> symbol_from_string

## Cached strings & symbols

cppally provides an efficient caching strategy for constructing cppally
strings/symbols from string literals

`cached_str<>`

``` cpp
cached_str<"cached_string">()
```

    #> [1] "cached_string"

This initialises the string once, caches it (to R’s CHARSXP pool), and
efficiently re-uses the cached string for each subsequent call.

We can cache symbols in a similar way

``` cpp
cached_sym<"cached_symbol">()
```

    #> cached_symbol

## Lists

`r_sexp` is generally interpreted as an “element of a list” since lists
are defined as `r_vector<r_sexp>`, a vector that holds generic `r_sexp`
elements.

``` cpp

using list = r_vector<r_sexp>;

[[cppally::register]]
list new_list(int n){
  return list(n);
}
```

``` r

new_list(0)
#> list()
new_list(3)
#> [[1]]
#> NULL
#> 
#> [[2]]
#> NULL
#> 
#> [[3]]
#> NULL
```

The problem with a class like `r_sexp` is that it is by design generic
and therefore difficult to work with in C++. To disambiguate the actual
type we can use `r_sexp_visit()` via a C++ lambda.

**Example:** using `r_sexp_visit()` to resize every vector to length n
in-place

``` cpp

[[cppally::register]]
r_vector<r_sexp> resize_all(r_vector<r_sexp> x, r_size_t n){
    r_size_t list_length = x.length();
    for (r_size_t i = 0; i < list_length; ++i){
        r_sexp_visit(x.view(i), [&]<RVector T>(T vec) {
            x.set(i, vec.resize(n));
        });
    }
    return x;
}
```

## Factors

We can create a factor via `r_factors()`

``` cpp

[[cppally::register]]
r_factors new_factor(r_vector<r_str> x){
    return r_factors(x);
}
```

``` r

new_factor(letters)
#>  [1] a b c d e f g h i j k l m n o p q r s t u v w x y z
#> Levels: a b c d e f g h i j k l m n o p q r s t u v w x y z
```

In cppally, like R, factors are not vectors and therefore do not satisfy
the RVector concept. To access the underlying integer codes vector, use
the public `codes()` member function

``` cpp

static_assert(!RVector<r_factors>);

[[cppally::register]]
r_vector<r_int> factor_codes(r_factors x){
    return x.codes();
}
```

``` r

letter_fct <- new_factor(letters)

letter_fct |>
    factor_codes()
#>  [1]  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25
#> [26] 26
```

## Calling R functions

cppally provides the `r_function` class, which can be used to easily
call any R function.

``` cpp

[[cppally::register]]
r_date r_get_today(){
  r_function sys_date("Sys.Date");
  return as<r_date>(sys_date());
}
```

``` r

r_get_today()
#> [1] "2026-09-13"
```

To get a function from a specific package, use `pkg_env`, a helper that
returns the namespace of a package. The result of `pkg_env` can be
passed to the `env` (or second) arg of `r_function`’s symbol and string
constructors.

``` cpp

[[cppally::register]]
r_function base_sum_fn(){
  return r_function(cached_sym<"sum">(), pkg_env<"base">());
}
```

``` r

base_sum_fn()
#> function (..., na.rm = FALSE)  .Primitive("sum")
```

`r_function` also allows named arguments to be passed.

``` cpp

[[cppally::register]]
r_dbl base_sum(r_vector<r_dbl> x, bool na_rm){
  return as<r_dbl>(base_sum_fn()(x, arg("na.rm") = na_rm));
}
```

``` r

base_sum(c(NA, 1:3, NA), na_rm = TRUE)
#> [1] 6
```

## Value matching

Use `r_vector` member [`find()`](https://rdrr.io/r/utils/apropos.html)
to find the **0-indexed** locations of a scalar value.

``` cpp

[[cppally::register]]
r_vector<r_int> find_empty_string(r_vector<r_str> x){
    return x.find(r_str(""));
}
```

``` r

x <- c("zero", "one", "two", "three", "four")

find_empty_string(x)
#> integer(0)

# Add empty strings
x[c(1, 3)] <- ""

find_empty_string(x)
#> [1] 0 2
```

To find the locations of multiple values (first match), use
[`match()`](https://bit64.r-lib.org/reference/bit64S3.html). It works
like R’s [`base::match()`](https://rdrr.io/r/base/match.html) but is
0-indexed.

``` cpp

[[cppally::register]]
r_vector<r_int> match_strs(r_vector<r_str> x, r_vector<r_str> table){
    return match(x, table);
}
```

``` r

letters
#>  [1] "a" "b" "c" "d" "e" "f" "g" "h" "i" "j" "k" "l" "m" "n" "o" "p" "q" "r" "s"
#> [20] "t" "u" "v" "w" "x" "y" "z"

vowels <- c("a", "e", "i", "o", "u")

# cppally::match is 0-indexed
match_strs(vowels, letters)
#> [1]  0  4  8 14 20
```

cppally provides the `IS_IN` infix operator, identical to R’s `%in%`

``` cpp

[[cppally::register]]
r_vector<r_lgl> cpp_in(r_vector<r_str> x, r_vector<r_str> table){
    return x IS_IN table;
}
```

``` r

cpp_in(c("a", "A", NA), letters)
#> [1]  TRUE FALSE FALSE
```

To mimic R’s new `%notin%` operator, simply use the logical negation
operator.

``` cpp

[[cppally::register]]
r_vector<r_lgl> cpp_not_in(r_vector<r_str> x, r_vector<r_str> table){
    return !(x IS_IN table);
}
```

**Technical note:** cppally internally negates the intermediate result
of `x IS_IN table` in-place in this particular case because it satisfies
specific properties of exclusivity which while not covered here, may be
covered in a later vignette. This in-place negation is naturally
efficient as it avoids allocating a new vector.

``` r

cpp_not_in(c("a", "A", NA), letters)
#> [1] FALSE  TRUE  TRUE
```

## Subsetting

Subsetting is 0-indexed in cppally, as is all other indexing.

``` cpp

template <RVector T, typename subscript_t>
requires any<subscript_t, r_lgl, r_int, r_str>
[[cppally::register]]
T cpp_subset(T x, r_vector<subscript_t> y){
    return subset(x, y);
}
```

Subsetting with **integer** indices

``` r

x <- 1:10
cpp_subset(x, 0L) # index 0 is 1st value
#> [1] 1
cpp_subset(x, 9L) # index 9 is last value here
#> [1] 10
cpp_subset(x, 10L) # index 10 is out-of-bounds so NA is returned
#> [1] NA
cpp_subset(x, NA_integer_) # NA is returned with integer NA index
#> [1] NA
```

Subsetting with **logical** indices

``` r

cpp_subset(x, x > 5)
#> [1]  6  7  8  9 10
cpp_subset(x, x > 100)
#> integer(0)

# It differs to base subsetting (via `[`)
cpp_subset(x, rep(NA, 10)) # cppally only returns values associated with TRUE
#> integer(0)
x[rep(NA, 10)] # base R returns NA values when subsetting with NA logicals
#>  [1] NA NA NA NA NA NA NA NA NA NA
```

base R performs negative subsetting by using negative numbers. This is
not possible with our 0-indexed subset as we would have to also
represent negative zero to subset everything except the first element at
index 0. Instead cppally has an `invert` argument.

``` cpp

template <RVector T, typename subscript_t>
requires any<subscript_t, r_lgl, r_int, r_str>
[[cppally::register]]
T cpp_negative_subset(T x, r_vector<subscript_t> y){
    return subset(x, y, /*invert=*/ true);
}
```

``` r

cpp_negative_subset(x, 0L) # Everything but 1st
#> [1]  2  3  4  5  6  7  8  9 10
cpp_negative_subset(x, x > 5) # Everything except where x > 5
#> [1] 1 2 3 4 5
cpp_negative_subset(x, integer()) # Everything
#>  [1]  1  2  3  4  5  6  7  8  9 10
```

Named subsetting is also supported. cppally internally hashes the vector
names and performs hash lookups. For more info, see the ‘Vector Names
Hashing’ vignette.

``` r

names(x) <- LETTERS[seq_along(x)]

cpp_subset(x, c("A", "J", "Z"))
#>    A    J <NA> 
#>    1   10   NA
cpp_negative_subset(x, "A")
#>  B  C  D  E  F  G  H  I  J 
#>  2  3  4  5  6  7  8  9 10
```

## Concepts and Templates

One of the most powerful features of C++20 are concepts. These allow
users to write human-readable templates and constraints.

When writing your own templates, it is necessary to place them in
headers for cppally registration to work correctly.

Let’s practice by creating the
[`abs()`](https://rdrr.io/r/base/MathFun.html) function in C++ using
templates and the `RNumber` concept.

``` cpp

template <RNumber T>
[[cppally::register]]
T cpp_abs(T x){
  if (is_na(x)){
    return na<T>();
  } else if (x < 0){
    return -x;
  } else {
    return x;
  }
}
```

What’s nice is that it works correctly for integers and doubles while
simultaneously preserving their type

``` r

cpp_abs(-4.2)
#> [1] 4.2
cpp_abs(-3L)
#> [1] 3

class(cpp_abs(-4.2)) # Double preserved
#> [1] "numeric"
class(cpp_abs(-3L)) # Integer preserved
#> [1] "integer"
```

This type of programming is historically tricky within the R C API and
typically necessitates a switch statement that switches on the object’s
type, handling each type separately. With our
[`abs()`](https://rdrr.io/r/base/MathFun.html) template, the logic is
correctly handled with one set of operations.

### How it works

The top-line `template <RNumber T>` declares a template that
encapsulates `T`, an `RNumber` - a concept that contains `r_int`,
`r_int64` and `r_dbl`

If x is NA then we immediately also return NA via `na<T>()` which is a
templated function that returns NA of the input type `T`.

To correctly register templates, the ‘\[\[cppally::register\]\]’ tag
must always go above the function name.

``` cpp
template <typename T>
[[cppally::register]] // <--- Here
T foo(T x){
  return x;
}
```

### Templates without function arguments

Explicit instantiation (from R) is unfortunately not possible and
template types must be deduced from supplied arguments.

``` cpp
template <typename T>
[[cppally::register]]
T foo(){
    return T();
}
```

Here `foo()` will not be compiled because the function has no arguments
that let the compiler automatically deduce what `T` is. In C++ you would
always call this function like so: `foo<T>()`. Unfortunately we can’t do
that from R directly.

You may get a cryptic compiler error like this

``` cpp
error: no matching function for call to 'foo()'
[]<typename T>() -> decltype(cpp_to_r(::foo())) {
```

along with an equally cryptic note

``` cpp
note:   couldn't deduce template parameter 'T'
[]<typename T>() -> decltype(cpp_to_r(::foo())) {
```

Even though these kinds of templates can be written with cppally in C++,
they cannot be exported to R.

An obvious and somewhat ugly workaround is to include a prototype
argument that allows the template parameter to be deduced from.

``` cpp

// Return the default constructor result of RScalar types

template <RScalar T>
[[cppally::register]]
T scalar_default(T ptype){
    return T();
}
```

``` r

scalar_default(integer(1)) # Default is 0L
#> [1] 0
scalar_default(numeric(1)) # Default is 0.0
#> [1] 0
scalar_default(character(1)) # Default is ""
#> [1] ""
```

Exporting variadic templates are also not supported. The best
alternative is to use lists (`r_vector<r_sexp>`).

In the above example we used the `RScalar` concept which includes all
cppally scalar types (excluding `r_sexp`). For a list of all cppally
concepts, please see the **Annex**

## Attributes

Attributes can be manipulated via functions defined in the attr
namespace.

**Example:** Adding names to a list

``` cpp

[[cppally::register]]
r_vector<r_sexp> set_list_names(r_vector<r_sexp> x, r_vector<r_str> names){
  x.set_names(names);
  return x;
}
```

``` r

set.seed(42)
norm_samples <- lapply(1:5, \(x) rnorm(10, mean = x))
set_list_names(norm_samples, paste0("sample_", 1:5))
#> $sample_1
#>  [1] 2.3709584 0.4353018 1.3631284 1.6328626 1.4042683 0.8938755 2.5115220
#>  [8] 0.9053410 3.0184237 0.9372859
#> 
#> $sample_2
#>  [1]  3.3048697  4.2866454  0.6111393  1.7212112  1.8666787  2.6359504
#>  [7]  1.7157471 -0.6564554 -0.4404669  3.3201133
#> 
#> $sample_3
#>  [1] 2.693361 1.218692 2.828083 4.214675 4.895193 2.569531 2.742731 1.236837
#>  [9] 3.460097 2.360005
#> 
#> $sample_4
#>  [1] 4.455450 4.704837 5.035104 3.391074 4.504955 2.282991 3.215541 3.149092
#>  [9] 1.585792 4.036123
#> 
#> $sample_5
#>  [1] 5.205999 4.638943 5.758163 4.273295 3.631719 5.432818 4.188607 6.444101
#>  [9] 4.568554 5.655648
```

More useful attribute helpers

- `get_attrs()` - Returns a list of attributes (possibly
  `r_vector<r_sexp>(r_null)`)
- `set_attrs()` - Sets attributes to ones specified. Note: replaces any
  current attributes
- `clear_attrs()` - Removes all attributes
- `set_attr()` - Set a single attribute
- `get_attr()` - Get a single attribute
- `inherits1()` - Does object inherit class?
- `inherits_any()` - Does object inherit at least one of the specified
  classes?
- `inherits_all()` - Does object inherit all of the specified classes?
- `modify_attrs()` - Modifies current attributes but doesn’t remove any
  existing ones

## Regular sequences

There are two core functions for generating regular sequences -
[`sequence()`](https://rdrr.io/r/base/sequence.html) and
[`seq()`](https://rdrr.io/r/base/seq.html).
[`seq()`](https://rdrr.io/r/base/seq.html) behaves exactly like the R
equivalent [`base::seq()`](https://rdrr.io/r/base/seq.html), and
[`sequence()`](https://rdrr.io/r/base/sequence.html) behaves like the R
equivalent [`base::sequence()`](https://rdrr.io/r/base/sequence.html),
with the exception that it accepts scalar arguments instead of vector
ones, and also works with decimal increments.

``` cpp
seq(r_dbl(1), r_dbl(5), r_dbl(0.5))
```

    #> [1] 1.0 1.5 2.0 2.5 3.0 3.5 4.0 4.5 5.0

``` cpp
sequence(5, /*from = */ r_int(0), /*by = */ r_int(-1))
```

    #> [1]  0 -1 -2 -3 -4

We can also use [`sequence()`](https://rdrr.io/r/base/sequence.html) to
easily replicate [`base::seq_len()`](https://rdrr.io/r/base/seq.html)

``` cpp

[[cppally::register]]
r_vector<r_int> cpp_seq_len(r_size_t n){
  return sequence(n, /* from = */ r_int(1), /* by = */ r_int(1));
}
```

``` r

cpp_seq_len(5)
#> [1] 1 2 3 4 5
```

It is also straightforward to replicate
[`base::sequence()`](https://rdrr.io/r/base/sequence.html) with `pmap()`

``` cpp

template <typename T>
requires (any<T, r_int, r_int64, r_dbl>)
[[cppally::register]]
r_vector<r_sexp> cpp_sequences(r_vector<r_int> size, r_vector<T> from, r_vector<T> by){
    return pmap([](auto a, auto b, auto c){
        return as<r_sexp>(sequence(a, b, c));
    }, size, from, by);
}
```

``` r

cpp_sequences(1:3, from = 0L, by = 1L) |> 
  unlist()
#> [1] 0 0 1 0 1 2

# Same as base R
sequence(1:3, from = 0L, by = 1L)
#> [1] 0 0 1 0 1 2
```

## Using multiple OpenMP threads

To set the number of global OpenMP threads, use
`cppally::set_threads()`.

``` cpp
cppally::set_threads(4); // Sets the number of threads to 4
```

To see the number of currently available threads, use
`cppally::get_threads()`.

From R we can then register these C++ function and use them.

``` cpp

[[cppally::register]]
void cpp_set_threads(int n){
  set_threads(n);
}

[[cppally::register]]
int cpp_get_threads(){
  return get_threads();
}
```

``` r


cpp_set_threads(2) # Set to 2 threads
cpp_get_threads()  # Number of threads now available
#> [1] 2

cpp_set_threads(1) # Reset back to 1
cpp_get_threads()  # Should return 1
#> [1] 1
```

Because this is unique to each dll file, setting threads in one R
package doesn’t affect another. Once threads have been set, all cppally
code that can make use of them, will use them. This means you should
speed improvements across many cppally functions.

## Stats functions

cppally provides some stats functions which are both common and
difficult to implement efficiently and safely.

- [`sum()`](https://rdrr.io/r/base/sum.html) - Sum of values
- [`mean()`](https://rdrr.io/r/base/mean.html) - Average of values
- [`range()`](https://rdrr.io/r/base/range.html) - Min and max range of
  values
- [`var()`](https://rdrr.io/r/stats/cor.html) - Variance

``` cpp

template <RNumber T>
[[cppally::register]]
r_dbl cpp_sum(r_vector<T> x, bool na_rm){
  return as<r_dbl>(sum(x, na_rm));
}
template <RNumber T>
[[cppally::register]]
r_vector<T> cpp_range(r_vector<T> x, bool na_rm){
  return range(x, na_rm);
}
template <RNumber T>
[[cppally::register]]
r_dbl cpp_var(r_vector<T> x, bool na_rm){
  return var(x, na_rm);
}
```

``` r

library(bench)

set.seed(1)
x <- sample.int(10, 5e05, replace = TRUE)
x[sample.int(10^5, 10^4)] <- NA # Add many NAs

# Sum (ignoring NAs)
mark(
  cppally_sum = cpp_sum(x, na_rm = TRUE),
  base_sum = sum(x, na.rm = TRUE)
)
#> # A tibble: 2 × 6
#>   expression       min   median `itr/sec` mem_alloc `gc/sec`
#>   <bch:expr>  <bch:tm> <bch:tm>     <dbl> <bch:byt>    <dbl>
#> 1 cppally_sum    124µs    125µs     7814.        0B        0
#> 2 base_sum       374µs    375µs     2628.        0B        0

# Sum (not ignoring NAs)
mark(
  cppally_sum = cpp_sum(x, na_rm = FALSE),
  base_sum = sum(x, na.rm = FALSE)
)
#> # A tibble: 2 × 6
#>   expression       min   median `itr/sec` mem_alloc `gc/sec`
#>   <bch:expr>  <bch:tm> <bch:tm>     <dbl> <bch:byt>    <dbl>
#> 1 cppally_sum   1.18µs   1.29µs   739545.        0B        0
#> 2 base_sum    230.04ns 251.11ns  3469902.        0B        0

# Range (ignoring NAs)
mark(
  cppally_range = cpp_range(x, na_rm = TRUE),
  base_range = range(x, na.rm = TRUE)
)
#> # A tibble: 2 × 6
#>   expression         min   median `itr/sec` mem_alloc `gc/sec`
#>   <bch:expr>    <bch:tm> <bch:tm>     <dbl> <bch:byt>    <dbl>
#> 1 cppally_range 156.56µs 157.25µs     6215.        0B       0 
#> 2 base_range      2.85ms   2.96ms      322.    11.4MB     211.

# Range (not ignoring NAs)
mark(
  cppally_range = cpp_range(x, na_rm = FALSE),
  base_range = range(x, na.rm = FALSE)
)
#> # A tibble: 2 × 6
#>   expression         min   median `itr/sec` mem_alloc `gc/sec`
#>   <bch:expr>    <bch:tm> <bch:tm>     <dbl> <bch:byt>    <dbl>
#> 1 cppally_range   1.16µs   1.22µs   760251.        0B      0  
#> 2 base_range    544.23µs   1.27ms     1063.    1.91MB     51.1

# Variance (ignoring NAs)
mark(
  cppally_var = cpp_var(x, na_rm = TRUE),
  base_var = var(x, na.rm = TRUE)
)
#> # A tibble: 2 × 6
#>   expression       min   median `itr/sec` mem_alloc `gc/sec`
#>   <bch:expr>  <bch:tm> <bch:tm>     <dbl> <bch:byt>    <dbl>
#> 1 cppally_var 738.12µs 754.29µs     1320.        0B      0  
#> 2 base_var      5.68ms   5.77ms      173.    5.74MB     24.8

# Variance (not ignoring NAs)
mark(
  cppally_var = cpp_var(x, na_rm = FALSE),
  base_var = var(x, na.rm = FALSE)
)
#> # A tibble: 2 × 6
#>   expression       min   median `itr/sec` mem_alloc `gc/sec`
#>   <bch:expr>  <bch:tm> <bch:tm>     <dbl> <bch:byt>    <dbl>
#> 1 cppally_var   1.18µs   1.23µs   766668.        0B      0  
#> 2 base_var      1.18ms   1.25ms      767.    3.81MB     72.5

# Using multiple threads

cpp_set_threads(4)

# Sum (ignoring NAs)
mark(
  cppally_sum = cpp_sum(x, na_rm = TRUE),
  base_sum = sum(x, na.rm = TRUE)
)
#> # A tibble: 2 × 6
#>   expression       min   median `itr/sec` mem_alloc `gc/sec`
#>   <bch:expr>  <bch:tm> <bch:tm>     <dbl> <bch:byt>    <dbl>
#> 1 cppally_sum   66.5µs   67.2µs    13541.        0B        0
#> 2 base_sum     374.3µs  375.3µs     2613.        0B        0

# Sum (not ignoring NAs)
mark(
  cppally_sum = cpp_sum(x, na_rm = FALSE),
  base_sum = sum(x, na.rm = FALSE)
)
#> # A tibble: 2 × 6
#>   expression       min   median `itr/sec` mem_alloc `gc/sec`
#>   <bch:expr>  <bch:tm> <bch:tm>     <dbl> <bch:byt>    <dbl>
#> 1 cppally_sum   1.18µs   1.23µs   750305.        0B        0
#> 2 base_sum    230.04ns 250.99ns  2550380.        0B        0

# Range (ignoring NAs)
mark(
  cppally_range = cpp_range(x, na_rm = TRUE),
  base_range = range(x, na.rm = TRUE)
)
#> # A tibble: 2 × 6
#>   expression         min   median `itr/sec` mem_alloc `gc/sec`
#>   <bch:expr>    <bch:tm> <bch:tm>     <dbl> <bch:byt>    <dbl>
#> 1 cppally_range 221.66µs 226.96µs     4104.        0B       0 
#> 2 base_range      2.89ms   2.97ms      250.    11.4MB     165.

# Range (not ignoring NAs)
mark(
  cppally_range = cpp_range(x, na_rm = FALSE),
  base_range = range(x, na.rm = FALSE)
)
#> # A tibble: 2 × 6
#>   expression         min   median `itr/sec` mem_alloc `gc/sec`
#>   <bch:expr>    <bch:tm> <bch:tm>     <dbl> <bch:byt>    <dbl>
#> 1 cppally_range   1.17µs   1.22µs   781918.        0B      0  
#> 2 base_range    546.95µs    1.3ms      898.    1.91MB     41.4

# Variance (ignoring NAs)
mark(
  cppally_var = cpp_var(x, na_rm = TRUE),
  base_var = var(x, na.rm = TRUE)
)
#> # A tibble: 2 × 6
#>   expression       min   median `itr/sec` mem_alloc `gc/sec`
#>   <bch:expr>  <bch:tm> <bch:tm>     <dbl> <bch:byt>    <dbl>
#> 1 cppally_var 372.11µs 379.88µs     2469.        0B      0  
#> 2 base_var      4.25ms   5.76ms      179.    5.72MB     23.9

# Variance (not ignoring NAs)
mark(
  cppally_var = cpp_var(x, na_rm = FALSE),
  base_var = var(x, na.rm = FALSE)
)
#> # A tibble: 2 × 6
#>   expression       min   median `itr/sec` mem_alloc `gc/sec`
#>   <bch:expr>  <bch:tm> <bch:tm>     <dbl> <bch:byt>    <dbl>
#> 1 cppally_var   1.18µs   1.24µs   728849.        0B      0  
#> 2 base_var    468.04µs   1.25ms      837.    3.81MB     76.6

# Reset to single-threaded
cpp_set_threads(1)
```

### A note on cppally::sum

[`cppally::sum()`](https://rdrr.io/r/base/sum.html) is written to use
SIMD instructions for performance where possible. It also uses higher
width integers for accuracy when dealing with integer vectors. For
example, if summing a 32-bit integer vector, it uses intermediate 64-bit
integers, and likewise if summing a 64-bit integer vector, it uses
intermediate 128-bit integers (if your compiler supports it). Doing it
this way avoids integer overflow entirely for vectors whose length is
less than 2^32. It also avoids intermediate integer overflow that would
have arisen if same-width integers were used. For example, for
`x <- .Machine$integer.max * c(-1L, -1L, 1L, 1L)`, the sum of x is
trivially 0, but the intermediate addition of `x[1]` and `x[2]` would
result in overflow if one were to use 32-bit integers.

## Unique values

`n_unique()` is a convenient helper to calculate the number of unique
values. [`unique()`](https://rdrr.io/r/base/unique.html) is also highly
performant.

``` cpp

template <RVector T>
[[cppally::register]]
r_int cpp_n_unique(T x){
  return as<r_int>(n_unique(x));
}

template <RVector T>
[[cppally::register]]
T cpp_unique(T x, bool sort){
  return unique(x, sort);
}
```

``` r

x <- sample(1:100, 10^5, replace = TRUE)

mark(
  base_n_unique = length(unique(x)),
  cppally_n_unique = cpp_n_unique(x)
)
#> # A tibble: 2 × 6
#>   expression            min   median `itr/sec` mem_alloc `gc/sec`
#>   <bch:expr>       <bch:tm> <bch:tm>     <dbl> <bch:byt>    <dbl>
#> 1 base_n_unique       706µs   1.24ms      935.    1.38MB     30.2
#> 2 cppally_n_unique    139µs 140.32µs     7010.        0B      0

mark(
  base_unique = unique(x),
  cppally_unique = cpp_unique(x, sort = FALSE)
)
#> # A tibble: 2 × 6
#>   expression          min   median `itr/sec` mem_alloc `gc/sec`
#>   <bch:expr>     <bch:tm> <bch:tm>     <dbl> <bch:byt>    <dbl>
#> 1 base_unique       713µs   1.22ms      982.    1.38MB     29.7
#> 2 cppally_unique    138µs 140.23µs     6995.      448B      0

mark(
  base_sorted_unique = sort(unique(x)),
  cppally_sorted_unique = cpp_unique(x, sort = TRUE)
)
#> # A tibble: 2 × 6
#>   expression                 min   median `itr/sec` mem_alloc `gc/sec`
#>   <bch:expr>            <bch:tm> <bch:tm>     <dbl> <bch:byt>    <dbl>
#> 1 base_sorted_unique       734µs    765µs     1166.    1.38MB     36.9
#> 2 cppally_sorted_unique    139µs    142µs     6919.      896B      0
```

## Sorting

cppally also offers
[`order()`](https://bit64.r-lib.org/reference/bit64S3.html) and
[`sort()`](https://rdrr.io/r/base/sort.html) for sorting purposes. A
hybrid approach is used, incorporating various sorting algorithms,
including ska sort (a variant on radix sort), counting sort, quick sort,
etc. These have been implemented with speed being a key focus.

``` cpp


template <RSortableVector T>
[[cppally::register]]
r_vector<r_int> cpp_order(T x){
  
  // Add 1 to return 1-indexed permutation to match base::order()
  // In general C++ code, always use 0-indexed as cppally indexing is 
  // always 0-indexed.
  
  return order(x) + 1; 
}

template <RSortableVector T>
[[cppally::register]]
T cpp_sort(T x){
  return sort(x);
}
```

[`cppally::sort`](https://rdrr.io/r/base/sort.html) always sorts `NA`
values last and so is equivalent to
`base::sort(..., method = "radix", na.last = TRUE)`.

``` r

r_radix_sort <- function(x){
  sort(x, method = "radix", na.last = TRUE)
}
r_radix_order <- function(x){
  order(x, method = "radix", na.last = TRUE)
}

# Small data
mark(cpp_sort(c(3, 2, 1)), r_radix_sort(c(3, 2, 1)))
#> # A tibble: 2 × 6
#>   expression                    min   median `itr/sec` mem_alloc `gc/sec`
#>   <bch:expr>               <bch:tm> <bch:tm>     <dbl> <bch:byt>    <dbl>
#> 1 cpp_sort(c(3, 2, 1))       1.34µs    1.4µs   626070.        0B      0  
#> 2 r_radix_sort(c(3, 2, 1))   28.6µs     31µs    29257.        0B     11.7
mark(cpp_order(c(3, 2, 1)), r_radix_order(c(3, 2, 1)))
#> # A tibble: 2 × 6
#>   expression                     min   median `itr/sec` mem_alloc `gc/sec`
#>   <bch:expr>                <bch:tm> <bch:tm>     <dbl> <bch:byt>    <dbl>
#> 1 cpp_order(c(3, 2, 1))       1.32µs   1.42µs   634223.        0B      0  
#> 2 r_radix_order(c(3, 2, 1))  12.91µs  14.14µs    67992.        0B     13.6

# Integer data
ints <- sample.int(20, 2e05, replace = TRUE)
mark(cpp_sort(ints), r_radix_sort(ints))
#> # A tibble: 2 × 6
#>   expression              min   median `itr/sec` mem_alloc `gc/sec`
#>   <bch:expr>         <bch:tm> <bch:tm>     <dbl> <bch:byt>    <dbl>
#> 1 cpp_sort(ints)     959.41µs      1ms      802.    1.53MB     27.2
#> 2 r_radix_sort(ints)   2.07ms   2.14ms      467.    1.53MB     18.1
mark(cpp_order(ints), r_radix_order(ints))
#> # A tibble: 2 × 6
#>   expression               min   median `itr/sec` mem_alloc `gc/sec`
#>   <bch:expr>          <bch:tm> <bch:tm>     <dbl> <bch:byt>    <dbl>
#> 1 cpp_order(ints)     833.64µs   1.14ms      889.     781KB     15.4
#> 2 r_radix_order(ints)   1.01ms    1.3ms      772.     781KB     13.1

# Double-floating point precision data
dbls <- as.double(ints)
mark(cpp_sort(dbls), r_radix_sort(dbls))
#> # A tibble: 2 × 6
#>   expression              min   median `itr/sec` mem_alloc `gc/sec`
#>   <bch:expr>         <bch:tm> <bch:tm>     <dbl> <bch:byt>    <dbl>
#> 1 cpp_sort(dbls)       2.28ms   2.35ms      404.    2.29MB     27.5
#> 2 r_radix_sort(dbls)   4.69ms   4.76ms      210.    2.29MB     15.6
mark(cpp_order(dbls), r_radix_order(dbls))
#> # A tibble: 2 × 6
#>   expression               min   median `itr/sec` mem_alloc `gc/sec`
#>   <bch:expr>          <bch:tm> <bch:tm>     <dbl> <bch:byt>    <dbl>
#> 1 cpp_order(dbls)       2.02ms   2.13ms      468.     781KB    10.5 
#> 2 r_radix_order(dbls)   4.06ms   4.14ms      238.     781KB     4.11

# Floating point data with decimals
dcmls <- rnorm(length(dbls))
mark(cpp_sort(dcmls), r_radix_sort(dcmls))
#> # A tibble: 2 × 6
#>   expression               min   median `itr/sec` mem_alloc `gc/sec`
#>   <bch:expr>          <bch:tm> <bch:tm>     <dbl> <bch:byt>    <dbl>
#> 1 cpp_sort(dcmls)       6.08ms   6.17ms      162.    2.29MB     13.3
#> 2 r_radix_sort(dcmls)    7.5ms   7.73ms      129.    2.29MB     11.1
mark(cpp_order(dcmls), r_radix_order(dcmls))
#> # A tibble: 2 × 6
#>   expression                min   median `itr/sec` mem_alloc `gc/sec`
#>   <bch:expr>           <bch:tm> <bch:tm>     <dbl> <bch:byt>    <dbl>
#> 1 cpp_order(dcmls)       5.87ms   5.93ms      167.     781KB     4.13
#> 2 r_radix_order(dcmls)   7.14ms   7.38ms      136.     781KB     4.18

# String data
strs <- round(dcmls, 2) |> paste0()
mark(cpp_sort(strs), r_radix_sort(strs))
#> # A tibble: 2 × 6
#>   expression              min   median `itr/sec` mem_alloc `gc/sec`
#>   <bch:expr>         <bch:tm> <bch:tm>     <dbl> <bch:byt>    <dbl>
#> 1 cpp_sort(strs)       3.55ms   4.37ms      233.    2.29MB     23.3
#> 2 r_radix_sort(strs)   2.27ms   2.71ms      377.    2.29MB     36.6
mark(cpp_order(strs), r_radix_order(strs))
#> # A tibble: 2 × 6
#>   expression               min   median `itr/sec` mem_alloc `gc/sec`
#>   <bch:expr>          <bch:tm> <bch:tm>     <dbl> <bch:byt>    <dbl>
#> 1 cpp_order(strs)       1.65ms      2ms      502.     781KB     10.5
#> 2 r_radix_order(strs)  942.9µs    975µs      981.     781KB     21.8
```

## Grouping

Some areas of analysis require working with groups, with sometimes very
large numbers of them. cppally provides a powerful class `groups`, along
with `make_groups()`, a function that creates `groups` from vectors.

The structure of `groups` is:

- `r_vector<r_int> ids` - The group IDs
- `int n_groups` - Number of unique groups
- `bool ordered` - Do the group IDs specify a sorting order, or are they
  by order-of-first-appearance?
- `bool sorted` - Are the group IDs sorted? (This can also be true for
  order-of-first-appearance IDs)
- `r_vector<r_int> starts()` - Returns a vector of start locations of
  each unique group, signifying the location in the data at which each
  group initially appeared
- `r_vector<r_int> counts()` - Returns a vector of frequency counts of
  each unique group
- `r_vector<r_int> order()` - Returns a 0-indexed permutation vector
  that can be used to sort group IDs

**Note:** When the groups are in sorted-order (where `sorted` is true),
member functions like `starts()` and `counts()` use a hybrid search
strategy combining linear, gallop, and binary searching. This strategy
ensures that search time complexity scales with the number of groups and
not the size of the data.

``` cpp


template <RVector T>
[[cppally::register]]
r_vector<r_sexp> group_info(T x){
  groups g = make_groups(x, /*ordered=*/ false);
  return make_vec<r_sexp>(
    arg("group_id") = g.ids,
    arg("n_groups") = g.n_groups,
    arg("ordered") = g.ordered,
    arg("sorted") = g.sorted,
    arg("starts") = g.starts(),
    arg("counts") = g.counts(),
    arg("order") = g.order(),
    arg("group_names") = group_names(x, g)
  );
}
```

``` r

set.seed(1)
x <- sample(c("A", "B", "C"), 20, replace = TRUE)

# Convert into group info

letter_groups <- group_info(x)

letter_groups$group_id # Group IDs
#>  [1] 0 1 0 2 0 1 1 2 2 1 1 0 0 0 2 2 2 2 1 0
letter_groups$n_groups # Number of groups
#> [1] 3
letter_groups$ordered  # Groups were collected using order-of-first-appearance
#> [1] FALSE
letter_groups$sorted   # Are group IDs in sorted order?
#> [1] FALSE
letter_groups$starts   # Data start locations (0-indexed)
#> [1] 0 1 3
letter_groups$counts   # Group counts
#> [1] 7 6 7
letter_groups$order    # Order permutation (0-indexed)
#>  [1]  0  2  4 11 12 13 19  1  5  6  9 10 18  3  7  8 14 15 16 17
letter_groups$group_names # Names of unique groups
#> [1] "A" "C" "B"
```

In real analysis, you normally wouldn’t need all of this metadata. For
example, to return a data frame of unique groups (keys) and their
associated counts, we only need `starts()` and `counts()`.

``` cpp

template <RVector T>
[[cppally::register]]
r_df group_counts(T x){
  groups g = make_groups(x, /*ordered=*/ false);
  
  return make_df(
    arg("key") = subset(x, g.starts()),
    arg("count") = g.counts()
  );
}
```

``` r

group_counts(x)
#>   key count
#> 1   A     7
#> 2   C     6
#> 3   B     7
```

To illustrate how fast `make_groups()` is, let’s compare cppally-derived
group counts of a relatively large sample to base-R derived group
counts.

``` r

# 10k groups, 100k data points
x <- sample.int(10^4, 10^5, replace = TRUE)
mark(
  cppally_group_counts = group_counts(x)$count,
  base_table = table(factor(x, levels = unique(x))) |> as.integer()
)
#> # A tibble: 2 × 6
#>   expression                min   median `itr/sec` mem_alloc `gc/sec`
#>   <bch:expr>           <bch:tm> <bch:tm>     <dbl> <bch:byt>    <dbl>
#> 1 cppally_group_counts 326.85µs  335.3µs     2941.   514.1KB     41.2
#> 2 base_table             7.35ms   7.46ms      134.     6.2MB     28.3
```

## Annex

### Unsupported classes in R-registered templates

`r_sym` and `r_function` are both unsupported in R-registered template
functions. This only applies when used as template arguments. They will
work as normal where function arguments are non-templated.

Good:

``` cpp
template <RStringType T>
[[cppally::register]]
r_lgl foo(r_sym x, T y){
    return as<r_str>(x) == y;
}
```

Bad:

``` cpp
template <RStringType T, typename Symbol>
requires (is<Symbol, r_sym>)
[[cppally::register]]
r_lgl foo(Symbol x, T y){
    return as<r_str>(x) == y;
}
```

### All core cppally concepts

- RNumber - Includes `r_int`, `r_int64` and `r_dbl`

- RIntegerType - Includes `r_lgl`, `r_int`, `r_int64`

- RIntegerNumber - Includes `r_int` and `r_int64`

- RMathType - Includes `r_lgl`, `r_int`, `r_int64` and `r_dbl`

- RStringType - Includes `r_str` and `r_str_view`

- RScalar - Includes `r_lgl`, `r_int`, `r_int64`, `r_dbl`, `r_str`,
  `r_str_view`, `r_cplx`, `r_raw`, `r_date` and `r_psxct`

- RVal - Includes anything a cppally vector (`r_vector<>`) can contain:
  RScalar +`r_sexp`

- RVector - Includes `r_vector<T>` where `T` is an RVal

- RFactor - Factors

- RDataFrame - Data frames

- RComposite - Includes vectors, factors and data frames

- RTimeType - Includes `r_date` and `r_psxct`

- RNumericType - Numeric types, including RMathType and RTimeType

- RSortableType - Includes RNumericType and RStringType (strings can
  also be sorted)

- RAtomicVector - A vector that contains RScalar elements

- CppallyType - Any R type defined by R, including RVal, RVector,
  RFactor, RDataFrame, RSymbol

- CppType - Anything that is not an CppallyType

- CastableToRScalar - Anything that can be constructed or cast into an
  RScalar (which also includes RScalar)

Other useful type traits

- `unwrap_t` - Returns the underlying unwrapped type
- `as_r_scalar_t` - Returns the equivalent RScalar type
- `as_r_composite_t` - Returns the equivalent RComposite type
- `common_r_t` - Returns the common cppally type between 2 types

### Accessing the underlying types and values

While it is generally not recommended to access the underlying values,
you can do so with `unwrap()` which returns the underlying C/C++ value.
For example, `unwrap(r_int(5))` will return an `int` of value `5`.

To access the underlying type, use `unwrap_t<>` which always aligns with
`unwrap()`

``` cpp
int one = unwrap(r_int(1));
double two = unwrap(r_dbl(2));

// Confirming that unwrap_t produces the required types as well
static_assert(std::is_same_v<int, unwrap_t<r_int>>);
static_assert(std::is_same_v<double, unwrap_t<r_dbl>>);

// unwrap_t<r_str> = SEXP
// cppally recommends against using BOTH the cppally API and the R C API together.
unwrap_t<r_str> r_c_api_blank_str = unwrap(r_str(""));
```

[^1]: `r_sexp` represents a generic R object which can include cppally
    vectors. We will explain how to disambiguate `r_sexp` later which is
    most useful when working with lists and data frames
