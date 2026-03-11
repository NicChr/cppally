#include <cpp20.hpp>
using namespace cpp20;

[[cpp20::register]]
r_str dummy(){
  return r_str("hello"); 
}
