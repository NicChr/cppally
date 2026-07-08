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
    constexpr r_cplx() noexcept : value{0.0, 0.0} {}
    constexpr r_cplx(r_dbl r, r_dbl i) noexcept : value{r, i} {}
    
    // Conversion handling
    explicit constexpr r_cplx(std::complex<double> x) noexcept : value{x} {}
    constexpr operator std::complex<double>() const noexcept { return value; }

    // Get real and imaginary parts
    constexpr r_dbl re() const noexcept { return r_dbl{value.real()}; }
    constexpr r_dbl im() const noexcept { return r_dbl{value.imag()}; }

    static constexpr r_cplx na() noexcept {
        return r_cplx(r_dbl::na(), r_dbl::na());
    }

    constexpr bool is_na() const noexcept {
        return re().is_na() || im().is_na();
    }
};

namespace internal {
inline constexpr r_cplx na_cplx = r_cplx::na();
}

}

#endif
