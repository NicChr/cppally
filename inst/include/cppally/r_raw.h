#ifndef CPPALLY_R_RAW_H
#define CPPALLY_R_RAW_H

#include <cppally/r_concepts.h>

namespace cppally {

// In the future we will use std::byte
struct r_raw {
    unsigned char value;
    using value_type = unsigned char;

    // Constructors
    constexpr r_raw() : value{static_cast<unsigned char>(0)} {}

    // Conversion handling
    explicit constexpr r_raw(unsigned char x) : value{x} {}
    constexpr operator unsigned char() const { return value; }
};

}

#endif
