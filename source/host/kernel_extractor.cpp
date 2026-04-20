#include "kernel_extractor.hpp"
#include <iostream>
#include <algorithm>
#include <Eigen/Dense>
#include <Eigen/Eigenvalues>

using ComplexD = std::complex<double>;

// Eigen typedefs for convenience
using MatrixXcd = Eigen::MatrixXcd;
using VectorXd = Eigen::VectorXd;

/**
 * Extract SOCS kernels and scales from TCC matrix
 * 
 * The TCC matrix is Hermitian (TCC[i,j] = conj(TCC[j,i]))
 * We extract the largest nk eigenvalues and their corresponding eigenvectors.
 * 
 * Implementation using Eigen::SelfAdjointEigenSolver:
 *   1. Copy TCC to Eigen matrix (column-major, compatible with std::vector)
 *   2. Run SelfAdjointEigenSolver for Hermitian eigenvalue decomposition
 *   3. Extract top nk eigenvalues and eigenvectors
 *   4. Reverse order (Eigen returns smallest to largest)
 *   5. Format output kernels and scales
 */
int extractKernels(std::vector<std::vector<ComplexD>>& krns,
                   std::vector<float>& scales, int nk,
                   const std::vector<ComplexD>& tcc,
                   int tccSize, int Nx, int Ny, int verbose) {
    
    // Validate input
    if (tccSize * tccSize != static_cast<int>(tcc.size())) {
        std::cerr << "[extractKernels] ERROR: TCC size mismatch!" << std::endl;
        return -1;
    }
    
    if (nk > tccSize) {
        std::cerr << "[extractKernels] ERROR: nk > tccSize!" << std::endl;
        return -2;
    }
    
    if (verbose >= 1) {
        std::cout << "[extractKernels] Solving eigenvalue problem for TCC matrix..." << std::endl;
        std::cout << "  TCC size: " << tccSize << " x " << tccSize << std::endl;
        std::cout << "  Requested kernels: " << nk << std::endl;
    }
    
    // Step 1: Copy TCC to Eigen matrix
    // Eigen uses column-major storage, compatible with our row-major TCC layout
    // We need to transpose during copy to ensure correct matrix representation
    MatrixXcd eigenTcc(tccSize, tccSize);
    
    for (int i = 0; i < tccSize; i++) {
        for (int j = 0; j < tccSize; j++) {
            // Original TCC: tcc[i * tccSize + j] (row-major)
            // Eigen matrix: eigenTcc(i, j) (column-major, but direct indexing works)
            eigenTcc(i, j) = tcc[j * tccSize + i];  // Transpose for correct orientation
        }
    }
    
    // Step 2: Solve eigenvalue problem
    // SelfAdjointEigenSolver assumes the matrix is Hermitian
    // It returns eigenvalues sorted from smallest to largest
    Eigen::SelfAdjointEigenSolver<MatrixXcd> solver(eigenTcc);
    
    if (solver.info() != Eigen::Success) {
        std::cerr << "[extractKernels] ERROR: Eigenvalue decomposition failed!" << std::endl;
        return -3;
    }
    
    // Step 3: Extract eigenvalues (sorted smallest to largest)
    VectorXd eigenvalues = solver.eigenvalues();
    
    // Step 4: Extract eigenvectors (columns of eigenvector matrix)
    // Eigenvectors are sorted corresponding to eigenvalues (smallest first)
    MatrixXcd eigenvectors = solver.eigenvectors();
    
    if (verbose >= 1) {
        std::cout << "[extractKernels] Eigenvalue decomposition successful." << std::endl;
        std::cout << "  Top " << nk << " eigenvalues (largest):" << std::endl;
        for (int i = tccSize - nk; i < tccSize; i++) {
            std::cout << "    lambda[" << i << "] = " << eigenvalues[i] << std::endl;
        }
    }
    
    // Step 5: Extract top nk eigenvalues and eigenvectors (reverse order)
    // Eigen returns smallest to largest, we want largest to smallest
    scales.resize(nk);
    krns.resize(nk);
    for (int k = 0; k < nk; k++) {
        krns[k].resize(tccSize);
    }
    
    for (int j = 0; j < nk; j++) {
        // Eigenvalue index (reverse order: largest first)
        int eigenIdx = tccSize - 1 - j;
        
        // Scale (eigenvalue)
        scales[j] = static_cast<float>(eigenvalues[eigenIdx]);
        
        // Kernel (eigenvector, reverse order within vector)
        // This matches litho.cpp behavior: krns[i][j] = z[(m-1-i)*n + (n-1-j)]
        for (int i = 0; i < tccSize; i++) {
            krns[j][i] = eigenvectors.col(eigenIdx)(tccSize - 1 - i);
        }
    }
    
    if (verbose >= 1) {
        std::cout << "[extractKernels] Kernels extracted successfully." << std::endl;
        std::cout << "  Total kernel energy: sum(scales) = ";
        double total = 0.0;
        for (int k = 0; k < nk; k++) {
            total += scales[k];
        }
        std::cout << total << std::endl;
    }
    
    return 0;  // Success
}

/**
 * Reconstruct kernels in 2D spatial frequency format
 * 
 * Each kernel (1D eigenvector) is rearranged to 2D format:
 *   Kernel[k][y * (2*Nx+1) + x] where y = -Ny to Ny, x = -Nx to Nx
 * 
 * This function is already implicitly done by the extraction,
 * but provided for explicit 2D access if needed.
 */
void reconstructKernels2D(std::vector<std::vector<ComplexD>>& krns2D,
                          const std::vector<std::vector<ComplexD>>& krns,
                          int nk, int tccSize, int Nx, int Ny) {
    
    int kerX = 2 * Nx + 1;
    int kerY = 2 * Ny + 1;
    
    krns2D.resize(nk);
    for (int k = 0; k < nk; k++) {
        krns2D[k].resize(kerX * kerY);
    }
    
    // The 1D kernel already has the correct ordering:
    // Index = (y + Ny) * (2*Nx+1) + (x + Nx)
    // So we just copy (no rearrangement needed)
    for (int k = 0; k < nk; k++) {
        krns2D[k] = krns[k];
    }
}