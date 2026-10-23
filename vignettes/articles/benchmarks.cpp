// ----- For benchmarks.Rmd ----- //

#include <cppally.hpp>
using namespace cppally;

// ---- set_cppally_threads ----
[[cppally::register]]
void set_cppally_threads(int n){
  set_threads(n);
}

// ---- cpp_order ----
template <typename T>
requires (any<T, r_int, r_int64, r_dbl, r_str>)
[[cppally::register]]
r_vector<r_int> cpp_order(r_vector<T> x){
  return order(x);
}

// ---- cpp_unique ----
template <typename T>
requires (any<T, r_int, r_int64, r_dbl, r_str>)
[[cppally::register]]
r_vector<T> cpp_unique(r_vector<T> x, bool sort){
  return unique(x, sort);
}

// ---- cpp_match ----
template <typename T>
requires (any<T, r_int, r_int64, r_dbl, r_str>)
[[cppally::register]]
r_vector<r_int> cpp_match(r_vector<T> x, r_vector<T> y){
  return match(x, y);
}

// ---- cpp_group_id ----
template <RVector T>
[[cppally::register]]
r_vector<r_int> cpp_group_id(T x, bool ordered){
  return make_groups(x, ordered).ids;
}

// ---- cpp_sampling ----
[[cppally::register]]
r_vector<r_int> cpp_sample_int_with_replacement(int n, int size){

  r_vector<r_int> out(size);

  random_stream r; // Setup random_stream - it respects `base::set.seed()` so it is reproducible from an R seed

  for (int i = 0; i < size; ++i){
    out.set(i, r.index(1, n));
  }

  return out;
}

// ---- cpp_distributions ----
[[cppally::register]]
r_vector<r_dbl> cpp_unif(int n, double lo, double hi){

  r_vector<r_dbl> out(n);

  random_stream r; // Setup random_stream - it respects `base::set.seed()` so it is reproducible from an R seed

  for (int i = 0; i < n; ++i){
    out.set(i, r.uniform(lo, hi));
  }

  return out;
}

[[cppally::register]]
r_vector<r_dbl> cpp_exponential(int n){
  r_vector<r_dbl> out(n);
  random_stream r;
  for (int i = 0; i < n; ++i){
    out.set(i, r.exponential());
  }
  return out;
}

[[cppally::register]]
r_vector<r_dbl> cpp_normal(int n){
  r_vector<r_dbl> out(n);
  random_stream r;
  for (int i = 0; i < n; ++i){
    out.set(i, r.normal());
  }
  return out;
}

[[cppally::register]]
r_vector<r_int64> cpp_binomial(int n, int size, double prob){
  r_vector<r_int64> out(n);
  random_stream r;
  for (int i = 0; i < n; ++i){
    out.set(i, r.binomial(size, prob));
  }
  return out;
}

[[cppally::register]]
r_vector<r_dbl> cpp_gamma(int n, double shape){
  r_vector<r_dbl> out(n);
  random_stream r;
  for (int i = 0; i < n; ++i){
    out.set(i, r.gamma(shape));
  }
  return out;
}

[[cppally::register]]
r_vector<r_int64> cpp_poisson(int n, double lambda){
  r_vector<r_int64> out(n);
  random_stream r;
  for (int i = 0; i < n; ++i){
    out.set(i, r.poisson(lambda));
  }
  return out;
}

[[cppally::register]]
r_vector<r_dbl> cpp_lognormal(int n){
  r_vector<r_dbl> out(n);
  random_stream r;
  for (int i = 0; i < n; ++i){
    out.set(i, r.lognormal());
  }
  return out;
}

[[cppally::register]]
r_vector<r_dbl> cpp_beta(int n, double shape1, double shape2){
  r_vector<r_dbl> out(n);
  random_stream r;
  for (int i = 0; i < n; ++i){
    out.set(i, r.beta(shape1, shape2)); 
  }
  return out;
}


// ---- cpp_date_field_accessors ----
[[cppally::register]]
r_vector<r_int> cpp_year(r_vector<r_date> x){
  return pmap_parallel_simd([](r_date v){ return v.year(); }, x);
}

[[cppally::register]]
r_vector<r_int> cpp_month(r_vector<r_date> x){
  return pmap_parallel_simd([](r_date v){ return v.month(); }, x);
}

[[cppally::register]]
r_vector<r_int> cpp_week(r_vector<r_date> x){
  return pmap_parallel_simd([](r_date v){ return v.week(); }, x);
}

[[cppally::register]]
r_vector<r_int> cpp_iso_week(r_vector<r_date> x){
  return pmap_parallel_simd([](r_date v){ return v.iso_week(); }, x);
}

// ---- cpp_date_arithmetic ----
[[cppally::register]]
r_vector<r_date> cpp_add_months(r_vector<r_date> x, r_vector<r_int> n, int roll_choice){
  // roll = 0 (none), 1 (backwards), 2 (forwards), 3 (away), 4 (nearest)
  return pmap_parallel_simd([choice = roll_choice == 0 ? roll::none : roll::backward](r_date v, r_int n){
    return v.add<"months">(n, choice); 
  }, x, n);
}

[[cppally::register]]
r_vector<r_date> cpp_add_days(r_vector<r_date> x, r_vector<r_int> n){
  return pmap_parallel_simd([](r_date v, r_int n){ return v.add<"days">(n); }, x, n);
}

// ---- cpp_floor_dates ----
[[cppally::register]]
r_vector<r_date> cpp_floor_month(r_vector<r_date> x){
  return pmap_parallel_simd([](r_date v){ return v.floor<"month">(); }, x);
}

[[cppally::register]]
r_vector<r_date> cpp_floor_week(r_vector<r_date> x){
  return pmap_parallel_simd([](r_date v){ return v.floor<"week">(); }, x);
}

// ---- cpp_time_diff ----
[[cppally::register]]
r_vector<r_dbl> cpp_time_diff_days(r_vector<r_date> x, r_vector<r_date> y){
  return pmap_parallel_simd([](r_date a, r_date b){ return time_diff<"days">(a, b); }, x, y);
}

[[cppally::register]]
r_vector<r_dbl> cpp_time_diff_weeks(r_vector<r_date> x, r_vector<r_date> y){
  return pmap_parallel_simd([](r_date a, r_date b){ return time_diff<"weeks">(a, b); }, x, y);
}

[[cppally::register]]
r_vector<r_dbl> cpp_time_diff_months(r_vector<r_date> x, r_vector<r_date> y, int roll_choice){
  return pmap_parallel_simd([roll_choice](r_date a, r_date b){
    return time_diff<"months">(a, b, /*width = */ r_dbl(1.0), static_cast<roll>(roll_choice));
  }, x, y);
}

// ---- cpp_stats ----
template <RNumber T>
[[cppally::register]]
r_dbl cpp_sum(r_vector<T> x){
  return as<r_dbl>(sum(x, /*na_rm = */ true));
}

template <RNumber T>
[[cppally::register]]
T cpp_min(r_vector<T> x){
  return range(x, /*na_rm = */ true).get(0);
}

template <RNumber T>
[[cppally::register]]
r_dbl cpp_var(r_vector<T> x){
  return var(x, /*na_rm = */ true);
}

template <RNumber T>
[[cppally::register]]
r_vector<r_dbl> cpp_quantile(r_vector<T> x, r_vector<r_dbl> probs){
  return quantile(x, probs, /*na_rm = */ true, /*names = */ false);
}

// ---- cpp_grouped_stats ----
[[cppally::register]]
r_vector<r_dbl> grouped_sum(r_vector<r_dbl> x, r_vector<r_int> g){
  groups grps = make_groups(g, /*ordered = */ true);
  return apply_by_group(x, grps, [](const r_vector<r_dbl>& chunk){
    return sum(chunk, /*na_rm = */ true);
  });
}

[[cppally::register]]
r_vector<r_dbl> grouped_min(r_vector<r_dbl> x, r_vector<r_int> g){
  groups grps = make_groups(g, /*ordered = */ true);
  return apply_by_group(x, grps, [](const r_vector<r_dbl>& chunk){
    return range(chunk, /*na_rm = */ true).get(0);
  });
}

[[cppally::register]]
r_vector<r_dbl> grouped_var(r_vector<r_dbl> x, r_vector<r_int> g){
  groups grps = make_groups(g, /*ordered = */ true);
  return apply_by_group(x, grps, [](const r_vector<r_dbl>& chunk){
    return var(chunk, /*na_rm = */ true);
  });
}

[[cppally::register]]
r_vector<r_dbl> grouped_quantile(r_vector<r_dbl> x, r_vector<r_int> g, double prob){
  groups grps = make_groups(g, /*ordered = */ true);
  return apply_by_group(x, grps, [prob](const r_vector<r_dbl>& chunk){
    return quantile(chunk, prob, /*na_rm = */ true);
  });
}

// ---- cpp_grouped_binary_stats ----
[[cppally::register]]
r_vector<r_dbl> reduce_based_grouped_sum(r_vector<r_dbl> x, r_vector<r_int> g){
  groups grps = make_groups(g, /*ordered = */ true);
  return reduce_by_group(x, grps, std::plus<>{}, /*init = */ r_dbl(0));
}
