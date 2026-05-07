#ifndef HOST_TCC_PROCESSOR_HPP
#define HOST_TCC_PROCESSOR_HPP

#include <complex>
#include <vector>
#include "json_parser.hpp"

// TCC (Transmission Cross Coefficient) Calculator
// Computes the TCC matrix from source, optics, and geometry parameters

/**
 * Calculate TCC matrix
 * 
 * Parameters:
 *   tcc       : Output TCC matrix (tccSize x tccSize complex)
 *   tccSize   : TCC matrix dimension = (2*Nx+1)*(2*Ny+1)
 *   src       : Source matrix (srcSize x srcSize)
 *   srcSize   : Source grid size (typically 101)
 *   outerSigma: Maximum normalized source radius
 *   config    : Configuration parameters (NA, wavelength, defocus, Lx, Ly, Nx, Ny)
 *   verbose   : Verbosity level (0=silent, 1=progress, 2+details)
 */
void calcTCC(std::vector<std::complex<double>>& tcc, int tccSize,
             const std::vector<float>& src, int srcSize, float outerSigma,
             const SOCSConfig& config, int verbose = 0);

/**
 * Compute outer sigma from source matrix
 * Returns the maximum normalized distance from center to any non-zero source point
 */
float computeOuterSigma(const std::vector<float>& src, int srcSize);

/**
 * Compute aerial image from TCC and mask spectrum (TCC direct method)
 * 
 * Parameters:
 *   imgf      : Output aerial image spectrum (fftLx x fftLy complex)
 *   mskf      : Input mask spectrum (fftLx x fftLy complex)
 *   fftLx/Ly  : FFT dimensions
 *   tcc       : TCC matrix
 *   tccSize   : TCC matrix dimension
 *   Nx/Ny     : Frequency sampling range
 *   verbose   : Verbosity level
 */
void calcImageFromTCC(std::vector<std::complex<double>>& imgf,
                      const std::vector<std::complex<double>>& mskf,
                      int fftLx, int fftLy,
                      const std::vector<std::complex<double>>& tcc,
                      int tccSize, int Nx, int Ny, int verbose = 0);

#endif // HOST_TCC_PROCESSOR_HPP