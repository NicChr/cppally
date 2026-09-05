# Random number generation

In many analyses, reproducibility is just as important as the quality of
the analytical methods used. The ability to reproduce the same set of
numbers is a core mechanism of R.

`random_stream` exists to allow that reproducibility from R, but
entirely in C++.

``` r

library(cppally)
```

``` cpp
#include <cppally.hpp>
using namespace cppally;
```

## Creating a stream

The default constructor seeds a new stream from R’s *current* seed
state:

``` cpp
random_stream r; // Initialised with a new seed using R's current seed state
```

The consequence is the one that matters for users: a function that
builds a stream is reproducible from R.

``` cpp

[[cppally::register]]
r_dbl one_unif(){
  return r_dbl(random_stream().unif());
}
```

``` r

set.seed(1)
one_unif()
#> [1] 0.8642478

set.seed(1)
one_unif() # Reproduces the same number under the same R seed
#> [1] 0.8642478
```

Without setting a seed, we get random numbers as expected.

``` r

replicate(10, one_unif())
#>  [1] 0.35822373 0.24848048 0.07650453 0.86536336 0.28446347 0.09335021
#>  [7] 0.11783802 0.77373274 0.63778202 0.97516822
```

Because constructing a stream *consumes* R draws, two fresh streams
under one [`set.seed()`](https://rdrr.io/r/base/Random.html) are
different - exactly as you would want, and exactly like calling
[`runif()`](https://rdrr.io/r/stats/Uniform.html) twice:

``` r

set.seed(1)
c(one_unif(), one_unif())
#> [1] 0.8642478 0.3582237
```

## Reusing one stream

When doing repeated sampling, it’s more efficient to generate the
`random_stream` once and draw from it many times, rather than
constructing a new one per draw.

``` cpp

[[cppally::register]]
r_vector<r_dbl> unif_deviates(int n){

  random_stream r; // <-- Generated once here

  r_vector<r_dbl> out(n);

  for (r_size_t i = 0; i < n; ++i){
    out.set(i, r.unif());
  }
  return out;
}
```

``` r

set.seed(1)
unif_deviates(10)
#>  [1] 0.8642478 0.3523587 0.2399782 0.3767446 0.8174655 0.9434300 0.9112013
#>  [8] 0.4336280 0.2818099 0.0432162
```

`unif()` returns a `double` in `[0, 1)` by default, but can return any
range in `[a, b)`.

## Explicit seeds

A stream can also be built from a seed you choose, in which case R’s
state is irrelevant and never touched:

``` cpp

[[cppally::register]]
r_vector<r_dbl> unif_seeded(int n, uint64_t seed){

  random_stream r(seed);

  r_vector<r_dbl> out(n);

  for (r_size_t i = 0; i < n; ++i){
    out.set(i, r.unif());
  }
  return out;
}
```

``` r

set.seed(1)
unif_seeded(5, seed = 42)
#> [1] 0.8143051 0.3188210 0.9838942 0.7011356 0.7935045

set.seed(99)
unif_seeded(5, seed = 42) # Same numbers, different R seed
#> [1] 0.8143051 0.3188210 0.9838942 0.7011356 0.7935045
```

Every stream can return the seed it was created with, so a run seeded
from R can still be replayed later:

``` cpp
random_stream r;
uint64_t s = r.seed();   // Log this
// ... later ...
random_stream replay(s); // Identical sequence
```

## `index()`

`random_stream::index()` samples a random integer between the specified
bounds (**inclusive** at both ends).

``` cpp

[[cppally::register]]
int random_index(int a, int b){
  return random_stream().index(a, b);
}
```

``` r

set.seed(1)
random_index(0, 1000)
#> [1] 865
```

Unlike the `<random>` distributions discussed below, `index()` is
platform-independent. It uses Lemire’s nearly divisionless method over
pure integer arithmetic, so the same seed gives the same integers
everywhere.

### Example: efficient sample with replacement

``` cpp

[[cppally::register]]
r_vector<r_int> cpp_sample_int_with_replacement(int n, int size){

  random_stream r;
  r_vector<r_int> out(size);

  for (r_size_t i = 0; i < size; ++i){
    auto idx = r.index(0, n - 1);
    out.set(i, idx);
  }
  return out;
}
```

Benchmark comparison against `sample.int(..., replace = TRUE)`:

``` r

library(bench)
mark(
  cppally_sample_int_with_replacement = cpp_sample_int_with_replacement(10^6, 10^5),
  base_sample_int_with_replacement = sample.int(10^6, 10^5, replace = TRUE),
  check = FALSE
)
#> # A tibble: 2 × 6
#>   expression                           min   median `itr/sec` mem_alloc `gc/sec`
#>   <bch:expr>                      <bch:tm> <bch:tm>     <dbl> <bch:byt>    <dbl>
#> 1 cppally_sample_int_with_replac… 224.24µs 369.12µs     2690.     391KB    19.0 
#> 2 base_sample_int_with_replaceme…   2.17ms   2.33ms      429.     391KB     4.14
```

In this simple benchmark we achieve a large speed improvement over base
R. The whole loop runs without re-entering R’s RNG once - only the
`random_stream r;` line does that.

## Using `<random>` distributions

`random_stream` satisfies `std::uniform_random_bit_generator`, so it can
be handed straight to any C++ distribution or algorithm -
`std::shuffle`, `std::normal_distribution` and so on.

``` cpp

#include <random>

[[cppally::register]]
r_vector<r_dbl> normal_deviates(int n){

  random_stream r;
  r_vector<r_dbl> out(n);

  std::normal_distribution<> dist;

  for (r_size_t i = 0; i < n; ++i){
    out.set(i, dist(r));
  }
  return out;
}
```

``` r

set.seed(1)
normal_deviates(10)
#>  [1] -0.36860578  0.90939231 -0.63676191 -1.34332485 -0.13620834  0.84386574
#>  [7]  0.05412269  0.61586757  1.17028316 -1.23532499
```

Two caveats are worth knowing before you rely on this.

The C++ standard specifies the *interface* of the `<random>`
distributions but not their algorithms, so results are not guaranteed to
be identical across platforms, compilers, or standard library versions.
The same seed reproduces the same numbers on one machine, but not
necessarily on somebody else’s. Relatedly, *Writing R Extensions* asks
compiled code not to depend on the C++11 random number library - which
is why `random_stream`’s own methods (`unif()`, `index()`) are
implemented directly rather than delegating to `<random>`.

If cross-platform reproducibility matters, prefer `unif()`/`index()`, or
R’s own distribution functions via `draw_from_r()`.

## Calling R C API RNG functions

⚠️ To call R’s C API RNG functions - `unif_rand()`, `norm_rand()`,
`R_unif_index()`, or anything from `Rmath.h` - wrap them in
`draw_from_r()`.

``` cpp

[[cppally::register]]
r_vector<r_dbl> r_normal_deviates(int n){

  r_vector<r_dbl> out(n);

  draw_from_r([&]{
    for (r_size_t i = 0; i < n; ++i){
      double v = norm_rand();
      out.set(i, v);
    }
  });

  return out;
}
```

``` r

set.seed(1)
r_normal_deviates(5)
#> [1] -0.6264538  0.1836433 -0.8356286  1.5952808  0.3295078

set.seed(1)
rnorm(5) # Identical - these are R's own draws
#> [1] -0.6264538  0.1836433 -0.8356286  1.5952808  0.3295078
```

`draw_from_r()` loads R’s RNG state before running your callable and
writes it back after, i.e. it is an RAII wrapper around `GetRNGstate()`
and `PutRNGstate()`. Without it, draws never reach `.Random.seed`, so R
hands the same values out again on its next call.

It returns whatever the callable returns, so it wraps expressions as
well as statements:

``` cpp
uint64_t seed = draw_from_r([]{ return internal::draw_seed(); });
```

Wrap the *whole loop*, as above, rather than each individual draw - the
state round trip is the expensive part.

## Bootstrapping

Given that we can generate random indices very efficiently, it stands to
reason that we can also generate bootstrap samples efficiently.

``` cpp

[[cppally::register]]
r_vector<r_dbl> boot_mean(r_vector<r_dbl> x, int n_boot){

  r_size_t n = x.length();

  random_stream r;
  r_vector<r_dbl> out(n_boot);

  for (int b = 0; b < n_boot; ++b){
    
    r_dbl total(0);
    
    // calculate mean
    for (r_size_t i = 0; i < n; ++i){
      total += x.get(r.index(r_size_t(0), n - 1));
    }
    out.set(b, total / n);
    
  }
  return out;
}
```

`boot_mean` resamples `x` (with replacement), generating `n_boot` mean
estimates. We can then use `quantile` to create a percentile confidence
interval.

``` r

set.seed(42)
x <- rnorm(1000)

set.seed(1)
replicates <- boot_mean(x, 2000)

quantile(replicates, c(0.025, 0.975)) # Percentile confidence interval
#>        2.5%       97.5% 
#> -0.08765848  0.03722926
```

### Comparison to R - benchmark

We could compare `boot_mean` to R’s equivalent `mean` + `sample`, but
that wouldn’t be a fair comparison, given that `boot_mean` is a
hand-tuned C++ function which doesn’t generate intermediate vectors.

Instead let’s compare apples to apples and write our own C++
sample-with-replacement.

``` cpp

[[cppally::register]]
r_vector<r_dbl> resample(r_vector<r_dbl> x){

    r_size_t n = x.length();
    r_vector<r_dbl> out(n);
    
    random_stream r;
    
    for (r_size_t i = 0; i < n; ++i){
        out.set(i, x.get(r.index(r_size_t(0), n - 1)));
    }
    
    return out;
}
```

Now let’s test it against R.

``` r

boot_mean_r <- function(x, n_boot){
  vapply(
    seq_len(n_boot),
    \(i) mean(sample(x, replace = TRUE)), 
    0
  )
}

boot_mean_cppally <- function(x, n_boot){
  vapply(
    seq_len(n_boot),
    \(i) mean(resample(x)),
    0
  )
}

mark(
  cppally = boot_mean_cppally(x, 2000),
  base_r  = boot_mean_r(x, 2000),
  check = FALSE
)
#> # A tibble: 2 × 6
#>   expression      min   median `itr/sec` mem_alloc `gc/sec`
#>   <bch:expr> <bch:tm> <bch:tm>     <dbl> <bch:byt>    <dbl>
#> 1 cppally      22.7ms   23.2ms      41.9    15.4MB     28.0
#> 2 base_r       60.6ms   60.6ms      16.5    23.1MB    115.
```

The hand-tuned bootstrap mean naturally is faster.

``` r

mark(
  boot_mean_cppally(x, 2000),
  boot_mean(x, 2000),
  check = FALSE
)
#> # A tibble: 2 × 6
#>   expression                      min   median `itr/sec` mem_alloc `gc/sec`
#>   <bch:expr>                 <bch:tm> <bch:tm>     <dbl> <bch:byt>    <dbl>
#> 1 boot_mean_cppally(x, 2000)  22.93ms  23.21ms      43.0    15.4MB     28.7
#> 2 boot_mean(x, 2000)           4.26ms   4.37ms     229.     21.8KB      0
```

### A flexible and fast bootstrapper

The gap between `boot_mean_cppally` and `boot_mean` is worth pulling
apart. Three costs are paid on every one of those 2,000 iterations:
`resample()` builds a fresh `random_stream`, so R’s `.Random.seed` is
loaded and written back each time - exactly what *Reusing one stream*
set out to avoid; a new `n`-length vector is allocated only to have a
single number taken from it; and each replicate makes a round trip out
to R and back.

`boot_mean` pays none of them per replicate. It constructs one stream,
allocates nothing inside the loop, and never leaves C++.

All three go away together if we hold one stream for the whole run,
re-use one intermediate vector, and keep the replicate loop in C++. If
we can manage that without fixing the statistic in advance, we get
something generic that still performs like the hand-tuned version.

**Bootstrap functional:**

``` cpp

template <RScalar T, typename FUN>
requires RScalar<std::invoke_result_t<FUN&, const r_vector<T>&>> // Accept only fns that return scalars
auto apply_boot(const r_vector<T>& data, int n_boot, FUN fn){

  r_size_t n = data.length();

  // Function return type
  using stat_t = std::invoke_result_t<FUN&, const r_vector<T>&>;

  random_stream r;

  r_vector<stat_t> out(n_boot);
  
  // Reused across every replicate
  r_vector<T> samples(n);

  for (int b = 0; b < n_boot; ++b){

    // Sample x using a random index [0, n - 1]
    // Set the samples directly into our existing samples vector
    for (r_size_t i = 0; i < n; ++i){
        
        auto idx = r.index(r_size_t(0), n - 1);
        
        // data.view() is safe here because we are immediately protecting the result from gc
        // by placing it into samples, which is itself protected
        
        samples.set(i, data.view(idx));
    }

    // Apply function
    out.set(b, fn(samples));

  }

  return out;
}
```

Most of the complexity sits in the signature. The three arguments are:

- `data` - the vector being bootstrapped
- `n_boot` - the number of replicates
- `fn` - a C++ lambda returning the test-statistic of your choice

The `requires` clause rejects any `fn` that does not return a scalar,
and `stat_t` picks up whatever type it *does* return.

Using cppally’s [`cppally::mean()`](https://rdrr.io/r/base/mean.html),
we can easily re-write a new mean bootstrap function.

``` cpp

[[cppally::register]]
r_vector<r_dbl> boot_mean2(r_vector<r_dbl> x, int n_boot){
    return apply_boot(x, n_boot, [](const auto& vec){
        return mean(vec);
    });
}
```

Comparing our new bootstrap mean function against the hand-tuned
version.

``` r

mark(
  boot_mean(x, 2000),
  boot_mean2(x, 2000),
  check = FALSE
)
#> # A tibble: 2 × 6
#>   expression               min   median `itr/sec` mem_alloc `gc/sec`
#>   <bch:expr>          <bch:tm> <bch:tm>     <dbl> <bch:byt>    <dbl>
#> 1 boot_mean(x, 2000)    4.02ms   4.36ms      230.    15.7KB        0
#> 2 boot_mean2(x, 2000)    4.4ms   4.43ms      225.    23.5KB        0
```

Almost as fast as the hand-tuned version, which is a nice result given
that `boot_mean2` was written using a functional.

One benefit of a generic bootstrap functional like `apply_boot` is the
ability to quickly write new bootstrap methods.

``` cpp


// Bootstrapped sum

[[cppally::register]]
r_vector<r_dbl> boot_sum(r_vector<r_dbl> x, int n_boot){
    return apply_boot(x, n_boot, [](const auto& vec){
    
      return as<r_dbl>(sum(vec));
        
    });
}

// Bootstrapped count of positive values

[[cppally::register]]
r_vector<r_int> boot_n_positive(r_vector<r_dbl> x, int n_boot){
    return apply_boot(x, n_boot, [](const auto& vec){
    
      return vec.reduce([](auto acc, auto curr){
        return as<r_int>(acc + (curr > 0));
      }, 
      /*init = */ r_int(0));
        
    });
}
```

Note that these two return different types. `boot_sum` produces doubles,
whereas `boot_n_positive` counts, and so produces integers. Neither had
to declare that - `stat_t` follows whatever the lambda returns, and the
output vector is built from it.

``` r

set.seed(42)
boot_sum(x, 5)
#> [1] -71.1128232  18.2339024   0.1102194 -41.3509977 -55.1178594
boot_n_positive(x, 5)
#> [1] 479 487 454 486 479
```

## Engine

`random_stream` drives xoshiro256++ (Blackman & Vigna, public domain)
via Xoshiro-cpp v1.0 by Ryo Suzuki (MIT), bundled in `inst/include/` and
reduced to xoshiro256++ and its SplitMix64 seeder.
