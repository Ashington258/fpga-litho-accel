#ifndef HOST_SOURCE_PROCESSOR_HPP
#define HOST_SOURCE_PROCESSOR_HPP

#include <string>
#include <vector>
#include <sstream>
#include "json_parser.hpp"

// ============================================================================
// Source Matrix Creation Functions
// ============================================================================

/**
 * Create Annular source pattern
 * @param matrix Source matrix (srcSize x srcSize)
 * @param matrixSize Size of the matrix
 * @param annular Annular parameters (innerRadius, outerRadius)
 */
void createAnnular(std::vector<float>& matrix, int matrixSize, const Annular& annular);

/**
 * Create Dipole source pattern
 * @param matrix Source matrix (srcSize x srcSize)
 * @param matrixSize Size of the matrix
 * @param dipole Dipole parameters (radius, offset, onXAxis)
 */
void createDipole(std::vector<float>& matrix, int matrixSize, const Dipole& dipole);

/**
 * Create CrossQuadrupole source pattern
 * @param matrix Source matrix (srcSize x srcSize)
 * @param matrixSize Size of the matrix
 * @param crossQuadrupole CrossQuadrupole parameters (radius, offset)
 */
void createCrossQuadrupole(std::vector<float>& matrix, int matrixSize, const CrossQuadrupole& crossQuadrupole);

/**
 * Create Point source pattern
 * @param matrix Source matrix (srcSize x srcSize)
 * @param matrixSize Size of the matrix
 * @param point Point parameters (x, y)
 */
void createPoint(std::vector<float>& matrix, int matrixSize, const Point& point);

/**
 * Normalize source matrix to have total energy = 1
 * @param matrix Source matrix to normalize
 * @param matrixSize Size of the matrix
 */
void normalizeSource(std::vector<float>& matrix, int matrixSize);

// ============================================================================
// Source Processing Functions
// ============================================================================

/**
 * Create source matrix based on configuration
 * This is the main entry point matching litho.cpp createSource()
 * @param src Output source matrix (srcSize x srcSize)
 * @param config SOCS configuration with source parameters (will be updated with outerSigma)
 * @param ss_log Log stream for verbose output
 * @return true if successful, false otherwise
 */
bool createSource(std::vector<float>& src, SOCSConfig& config, std::stringstream& ss_log);

/**
 * Compute Nx/Ny dynamically from optical parameters
 * Formula: Nx = floor(NA * Lx * (1 + outerSigma) / wavelength)
 * @param config SOCS configuration (NA, Lx, wavelength, outerSigma)
 * @param Nx Output Nx value
 * @param Ny Output Ny value
 */
void computeNxNy(SOCSConfig& config);

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * Print source matrix statistics
 * @param src Source matrix
 * @param srcSize Size of the matrix
 * @param ss_log Log stream
 */
void printSourceStats(const std::vector<float>& src, int srcSize, std::stringstream& ss_log);

#endif // HOST_SOURCE_PROCESSOR_HPP