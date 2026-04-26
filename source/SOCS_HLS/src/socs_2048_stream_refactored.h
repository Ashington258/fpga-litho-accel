/**
 * SOCS HLS 2048 Stream Refactored Architecture Header
 * FPGA-Litho Project - Phase 3 Stream Interface Refactoring
 * 
 * Key Features:
 *   - TRUE DATAFLOW using hls::stream interfaces
 *   - Pipeline overlap between kernels
 *   - Fixed 128×128 FFT
 *   - Ping-pong buffering for tmpImg accumulation
 */

#ifndef SOCS_2048_STREAM_REFACTORED_H
#define SOCS_2048_STREAM_REFACTORED_H

// Conditional compilation for HLS vs standard C++
#ifdef __SYNTHESIS__
#include <hls_stream.h>
#include <ap_fixed.h>
#else
// Standard C++ headers for simulation
#include <iostream>
#include <fstream>
#include <vector>
#endif

#include <cmath>
#include <complex>

// ============================================================================
// 2048 Architecture Constants
// ============================================================================

// Maximum FFT size (fixed for all Nx configurations)
#define MAX_FFT_X  128
#define MAX_FFT_Y  128
#define MAX_FFT_LOG2  7  // log2(128) = 7

// Maximum kernel size (Nx=24 → kerX=49)
#define MAX_KER_X  49
#define MAX_KER_Y  49

// ============================================================================
// Type Definitions
// ============================================================================

typedef std::complex<float> cmpx_2048_t;

// ============================================================================
// Top-Level Function Declaration
// ============================================================================

/**
 * @brief SOCS calculation with stream interface refactoring
 * 
 * @param mskf_r Mask spectrum real part (Lx×Ly)
 * @param mskf_i Mask spectrum imaginary part (Lx×Ly)
 * @param krn_r Kernel real parts (nk×kerX×kerY)
 * @param krn_i Kernel imaginary parts (nk×kerX×kerY)
 * @param scales Eigenvalues (nk)
 * @param output Output image (MAX_FFT_X×MAX_FFT_Y)
 * @param nk Number of kernels
 * @param nx_actual Actual Nx value (runtime parameter)
 * @param ny_actual Actual Ny value (runtime parameter)
 * @param Lx Mask width
 * @param Ly Mask height
 */
void calc_socs_2048_hls_stream_refactored(
    float* mskf_r, float* mskf_i,
    float* krn_r, float* krn_i,
    float* scales,
    float* output,
    int nk,
    int nx_actual,
    int ny_actual,
    int Lx,
    int Ly
);

#endif // SOCS_2048_STREAM_REFACTORED_H
