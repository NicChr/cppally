#include <cppally.hpp>
#include <vector>
using namespace cppally;

[[cppally::register]]
void test_valgrind(){
    std::vector<int> int_vec(1000);
    abort("Error"); // This should run C++ destructors and release vector memory
}
