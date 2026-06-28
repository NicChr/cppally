# Functional Programming with cppally

This vignette is mainly a gallery of examples, serving to provide
intuition behind the usage of cppally functionals, as well as showcasing
their utility.

Let’s load cppally in R and include cppally in our cpp or header file to
get started

``` r

library(cppally)
```

``` cpp
#include <cppally.hpp>
using namespace cppally;
```

### reduce

`r_vec::reduce()` is a left-fold reduction functional that successively
applies a binary function along the elements of the vector (from
left-to-right). It allows for returning early and explicitly continuing
by calling `done()` and `keep()`. The input function is typically a
lambda, but can also be a callable (struct with `operator()`).

Example of summing a vector with `reduce`

``` cpp

[[cppally::register]]
r_dbl cpp_sum(r_vector<r_dbl> x){
    return x.reduce([](auto a, auto b){ return a + b; });
}
```

``` r

cpp_sum(1:10)
#> [1] 55
```

We could have also passed the callable `std::plus<>{}`, which makes it
even more readable

``` cpp

[[cppally::register]]
r_dbl cpp_sum2(r_vector<r_dbl> x){
    return x.reduce(std::plus<>{});
}
```

``` r

cpp_sum2(1:10)
#> [1] 55
```

To perform a reduction until a condition is met, use the helpers
`done()` and `keep()`

**Example:** Check vector has any `NA` or all `NA`

``` cpp

[[cppally::register]]
bool cpp_any_na(r_vector<r_dbl> x){
    return x.reduce([](auto, auto curr){ return is_na(curr) ? done(true) : keep(false); }, /*init = */ false);
}

[[cppally::register]]
bool cpp_all_na(r_vector<r_dbl> x){
    return x.reduce([](auto, auto curr){ return is_na(curr) ? keep(true) : done(false); }, /*init = */ true);
}
  
```

``` r

x <- c(1, 2, NA, 4, 5)
cpp_any_na(x)
#> [1] TRUE
cpp_all_na(x)
#> [1] FALSE
```

Notice that the two folds are identical - only `done`/`keep` and `init`
are swapped. This is the general pattern for any/all-style predicates.

**Example:** greatest-common-divisor across integer vector. The trick
here is to break when the result is 1 as `gcd(1, x) = 1` for any x

``` cpp

[[cppally::register]]
r_int cpp_gcd(r_vector<r_int> x){
    return x.reduce([](auto acc, auto curr){
        auto res = cppally::gcd(acc, curr); // cppally has its own NA-aware gcd
        if ( (res == 1).is_true() ){
            return done(res);
        } else {
            return keep(res);
        }
    });
}
  
```

``` r

cpp_gcd(c(5L, 25L, 125L))
#> [1] 5
cpp_gcd(c(5L, 25L, 1L, 125L))
#> [1] 1
```

Use `cumulative_reduce` to return a vector of all the intermediate
results of the reduction

``` cpp

[[cppally::register]]
r_vector<r_dbl> cpp_cumsum(r_vector<r_dbl> x){
    return x.cumulative_reduce(std::plus<>{});
}
  
```

``` r

cpp_cumsum(1:10)
#>  [1]  1  3  6 10 15 21 28 36 45 55
```

### pmap

`pmap` is a C++ variadic function that allows one to apply a function
across corresponding elements of multiple vectors.

**Example:** vectorised binary max

``` cpp

[[cppally::register]]
r_vector<r_dbl> cpp_pmax(r_vector<r_dbl> x, r_vector<r_dbl> y){
    return pmap([](auto a, auto b){
        return max(a, b);
    }, x, y);
}
  
```

``` r

x <- c(10, 20, 30)
y <- c(10, 50, 0)
cpp_pmax(x, y)
#> [1] 10 50 30

# pmap also recycles vectors

cpp_pmax(x, 15)
#> [1] 15 20 30
```

**Example:** vectorised if else

``` cpp

template <RVector T>
[[cppally::register]]
T cpp_if_else(r_vec<r_lgl> condition, T if_true, T if_false, T if_na){
    return pmap([](r_lgl condition_, auto yes, auto no, auto missing) {
        if (condition_.is_true()){
            return yes;
        } else if (condition_.is_false()){
            return no;
        } else {
            return missing;
        }
    }, condition, if_true, if_false, if_na);
}
```

``` r

cpp_if_else(c(TRUE, FALSE, NA), "yes", "no", "missing")
#> [1] "yes"     "no"      "missing"
```

`pmap_with_index()` is a variant that allows one to capture the index as
we iterate along the vector

**Example:** Integer sequence along vector

``` cpp

template <RVector T>
[[cppally::register]]
r_vector<r_int> cpp_seq_along(T x){
    return pmap_with_index([](int i, auto){ // 2nd arg included so function can compile
        return r_int(i + 1); // R is 1-indexed
    }, x);
}
```

``` r

cpp_seq_along(letters)
#>  [1]  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25
#> [26] 26
```

### Lagged operations

Use `pmap_with_shift` to perform lagged operations. It checks for
out-of-bounds access and therefore is safer than hand-writing it via
`pmap_with_index`

``` cpp

[[cppally::register]]
r_vector<r_int> cpp_lag(r_vector<r_int> x, int k){
    return pmap_with_shift([&](auto a){
        return lag(a, k);
    }, x);
}
```

``` r

# Lags
cpp_lag(1:10, k = 1)
#>  [1] NA  1  2  3  4  5  6  7  8  9
cpp_lag(1:10, k = 2)
#>  [1] NA NA  1  2  3  4  5  6  7  8
cpp_lag(1:10, k = 3)
#>  [1] NA NA NA  1  2  3  4  5  6  7

# Leads
cpp_lag(1:10, k = -1)
#>  [1]  2  3  4  5  6  7  8  9 10 NA
cpp_lag(1:10, k = -2)
#>  [1]  3  4  5  6  7  8  9 10 NA NA
cpp_lag(1:10, k = -3)
#>  [1]  4  5  6  7  8  9 10 NA NA NA
```

`pmap_with_shift` has three helpers:
[`lag()`](https://rdrr.io/r/stats/lag.html), `lead()` and `curr()`.
These are designed to assist in performing efficient lagged operations
in a vectorised context, while maintaining readability

**Example:** Lagged differencing

``` cpp

[[cppally::register]]
r_vector<r_dbl> cpp_diff(r_vector<r_dbl> x){
    return pmap_with_shift([&](auto a){
        return curr(a) - lag(a);
    }, x);
}
```

``` r

cpp_diff(1:10)
#>  [1] NA  1  1  1  1  1  1  1  1  1
cpp_diff(seq(10, 100, by = 5))
#>  [1] NA  5  5  5  5  5  5  5  5  5  5  5  5  5  5  5  5  5  5
```

### In-place functionals

To perform in-place transformations, use `r_vec::apply` as `pmap` always
allocates a fresh vector and therefore cannot do in-place modification.

``` cpp

[[cppally::register]]
r_vector<r_dbl> cpp_in_place_abs(r_vector<r_dbl>& x){
    x.apply([](auto a){ return abs(a);});
    return x;
}
```

``` r

x <- c(-20, -10)
cpp_in_place_abs(x)
#> [1] 20 10

x # Modified in-place
#> [1] 20 10
```

`r_vec::shift` is a helper which can shift an entire vector in-place

``` cpp

[[cppally::register]]
r_vector<r_dbl> cpp_in_place_lag(r_vector<r_dbl>& x, int k){
    x.shift(k);
    return x;
}
```

``` r

x <- c(1, 2, 3, 4, 5)
cpp_in_place_lag(x, k = 1)
#> [1] NA  1  2  3  4
x # lagged in-place
#> [1] NA  1  2  3  4

# keep lagging until we run out of elements to lag
cpp_in_place_lag(x, k = 1)
#> [1] NA NA  1  2  3
cpp_in_place_lag(x, k = 1)
#> [1] NA NA NA  1  2
cpp_in_place_lag(x, k = 1)
#> [1] NA NA NA NA  1
cpp_in_place_lag(x, k = 1)
#> [1] NA NA NA NA NA
```

### Vectorised math

`pmap` makes it easy to write vectorised math functions

**Example:** Binary addition

Here we are using `pmap_simd`, a variant of `pmap` that applies the
supplied transformation under an omp simd directive. SIMD
(single-instruction-multiple-data) is when the machine performs the same
operation on multiple data points instead of one data point at time. In
this case we are performing addition across multiple `r_int` values
simultaneously. To safely use `pmap_simd`, iterations must be
independent, not throw any errors, and the body must not have any side
effects, which for math operations on `RMathType` classes is true.

``` cpp

[[cppally::register]]
r_vector<r_int> cpp_add2(r_vector<r_int> x, r_vector<r_int> y){
    return pmap_simd([](auto a, auto b){ return a + b; }, x, y);
}
```

``` r

cpp_add2(1:5, 10)
#> [1] 11 12 13 14 15
```

While we could go ahead and write vectorised versions of all the core
operators, cppally has already done this with particular focus on
efficiency. All operators are parallelised via simd and avoid allocating
new vectors (like R does) where possible.

The vectorised operators currently defined:

binary: `+,-,*,/,+=,-=,*=,/=,==,<=,<,>=,>,|,&`

unary: `!,-`

**Example:** Mixed math operations

``` cpp

[[cppally::register]]
r_vector<r_dbl> cpp_pythagorean_theorem(r_vector<r_dbl> a, r_vector<r_dbl> b){
    return (a * a) + (b * b); // Pythagorean theorem - a^2 + b^2 = c^2
}
```

``` r

cpp_pythagorean_theorem(1:10, 10:1)
#>  [1] 101  85  73  65  61  61  65  73  85 101
```

cppally provides a rich set of scalar math functions (defined in
r_math.h) which can be trivially vectorised.

**Example:** vectorising `round`, `floor` and `ceiling`

``` cpp

[[cppally::register]]
r_vector<r_dbl> cpp_round(r_vector<r_dbl> x, r_vector<r_dbl> digits){
    return pmap([](auto a, auto b){ return round(a, b); }, x, digits);
}
[[cppally::register]]
r_vector<r_dbl> cpp_floor(r_vector<r_dbl> x){
    return pmap([](auto a){ return floor(a); }, x);
}
[[cppally::register]]
r_vector<r_dbl> cpp_ceiling(r_vector<r_dbl> x){
    return pmap([](auto a){ return ceiling(a); }, x);
}
```

``` r

x <- seq(-2, 2, by = 0.5)
cpp_round(x, digits = 0)
#> [1] -2 -2 -1  0  0  0  1  2  2
cpp_floor(x)
#> [1] -2 -2 -1 -1  0  0  1  1  2
cpp_ceiling(x)
#> [1] -2 -1 -1  0  0  1  1  2  2
```
