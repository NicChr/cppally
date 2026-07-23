#pragma once

#include <cppally_light.hpp>
using namespace cppally;

template <RNumber T>
[[cppally::register]]
auto reduce_sum(r_vec<T> x, bool na_rm){
    return x.reduce(std::plus<>{}, T(0), na_rm);
}
template <RMathType T>
[[cppally::register]]
auto reduce_max(r_vec<T> x, bool na_rm){
    if (x.length() == 0){
        return na<T>();
    } else {
        return x.reduce([](auto acc, auto curr) { return max(acc, curr); }, r_limits<T>::min(), na_rm);
    }
}
template <RNumber T>
[[cppally::register]]
auto reduce_cumulative_sum(r_vec<T> x, bool na_rm){
    return x.cumulative_reduce(std::plus<>{}, na_rm);
}

template <RNumber T>
[[cppally::register]]
T reduce_gcd(r_vec<T> x){
    return x.reduce([](auto a, auto b){
        auto g = gcd(a, b);
        if constexpr (RIntegerType<T>){
            return (g == 1).is_true() ? done(g) : g;
        } else {
            return g;
        }
    });
}

template <RMathType T, RMathType U>
[[cppally::register]]
auto pmap2_add(r_vec<T> x, r_vec<U> y){
    return pmap_simd([](auto a, auto b){ return a + b; }, x, y);
}

[[cppally::register]]
r_sexp pmap_add(r_vec<r_sexp> x){
    return list_pmap(x, []<RNumber elem_t>(std::span<elem_t> r){
        elem_t s{0};
        for (std::size_t i = 0; i < r.size(); ++i){
            s = s + r[i];
        }
        return s; 
    });
}
