#ifndef CPPALLY_R_CPLX_H
#define CPPALLY_R_CPLX_H

#include <complex> // For complex<double>
#include <cppally/r_dbl.h>

namespace cppally {

// Complex number - uses std::complex<double> under the hood
struct r_cplx {
    std::complex<double> value;
    using value_type = std::complex<double>;

    // Constructors
    constexpr r_cplx() : value{0.0, 0.0} {}
    constexpr r_cplx(r_dbl r, r_dbl i) : value{r, i} {}
    
    // Conversion handling
    explicit constexpr r_cplx(std::complex<double> x) : value{x} {}
    constexpr operator std::complex<double>() const { return value; }

    // Get real and imaginary parts
    constexpr r_dbl re() const { return r_dbl{value.real()}; }
    constexpr r_dbl im() const { return r_dbl{value.imag()}; }
};

}

#endif
