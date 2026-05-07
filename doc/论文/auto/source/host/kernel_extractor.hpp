#ifndef HOST_KERNEL_EXTRACTOR_HPP
#define HOST_KERNEL_EXTRACTOR_HPP

#include <complex>
#include <vector>
#include "json_parser.hpp"

// Kernel Extractor - Eigenvalue decomposition of TCC matrix
// Extracts SOCS kernels (eigenvectors) and scales (eigenvalues)

/**
 * Extract SOCS kernels and scales from TCC matrix
 * 
 * Uses Eigen library's SelfAdjointEigenSolver for complex Hermitian matrices.
 * Extracts the top nk eigenvalues/eigenvectors (largest eigenvalues).
 * 
 * Parameters:
 *   krns     : Output kernels (nk vectors, each of size tccSize)
 *   scales   : Output scales/eigenvalues (nk values)
 *   nk       : Number of kernels to extract
 *   tcc      : Input TCC matrix (tccSize x tccSize, Hermitian)
 *   tccSize  : TCC matrix dimension = (2*Nx+1)*(2*Ny+1)
 *   Nx/Ny    : Frequency sampling range (for ordering output kernels)
 *   verbose  : Verbosity level
 * 
 * Returns:
 *   0 on success, non-zero on failure
 */
int extractKernels(std::vector<std::vector<std::complex<double>>>& krns,
                   std::vector<float>& scales, int nk,
                   const std::vector<std::complex<double>>& tcc,
                   int tccSize, int Nx, int Ny, int verbose = 0);

/**
 * Reconstruct kernels in spatial frequency domain
 * Converts 1D eigenvectors to 2D kernel format (2*Nx+1 x 2*Ny+1)
 * 
 * Parameters:
 *   krns2D   : Output 2D kernels (nk vectors, each of (2*Nx+1)*(2*Ny+1) size)
 *   krns     : Input 1D eigenvectors
 *   nk       : Number of kernels
 *   tccSize  : TCC matrix dimension
 *   Nx/Ny    : Frequency sampling range
 */
void reconstructKernels2D(std::vector<std::vector<std::complex<double>>>& krns2D,
                          const std::vector<std::vector<std::complex<double>>>& krns,
                          int nk, int tccSize, int Nx, int Ny);

#endif // HOST_KERNEL_EXTRACTOR_HPP