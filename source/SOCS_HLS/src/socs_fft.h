/**
 * SOCS HLS Implementation with Real FFT Integration
 * FPGA-Litho Project
 * 
 * Configuration: Nx=4, IFFT 32×32, Output 17×17
 * 
 * This file defines FFT configuration and data types for SOCS algorithm
 */

#ifndef SOCS_FFT_H
#define SOCS_FFT_H

#include <hls_stream.h>
#include <hls_fft.h>
#include <ap_fixed.h>
#include <cmath>
#include <complex>

// ============================================================================
// Configuration Parameters (Nx=4, based on current config)
// ============================================================================
#define Lx 512
#define Ly 512
#define Nx 4
#define Ny 4
#define convX (4*Nx + 1)  // = 17
#define convY (4*Ny + 1)  // = 17
#define fftConvX 32       // nextPowerOfTwo(17)
#define fftConvY 32
#define kerX (2*Nx + 1)   // = 9
#define kerY (2*Ny + 1)   // = 9
#define nk 10

// FFT Parameters
#define FFT_LENGTH 32
#define FFT_NFFT_MAX 5    // log2(32) = 5
#define FFT_INPUT_WIDTH 32
#define FFT_TWIDDLE_WIDTH 24  // For floating point, must be 24 or 25

// ============================================================================
// Data Types (using float for better precision)
// ============================================================================
typedef float data_t;
typedef std::complex<data_t> cmpxData_t;

// ============================================================================
// FFT Configuration for 32-point FFT
// ============================================================================
// IMPORTANT: For floating-point FFT (32-bit), Vitis HLS has specific requirements
// - phase_factor_width must be 24 or 25
// - implementation_options affects resource usage
// ============================================================================

// FFT Configuration for 32-point (simplified, matching reference implementation)
// CRITICAL: For float FFT, minimal config to avoid IP generation errors
// Only specify essential parameters for 32-point float FFT
struct fft_config_32 : hls::ip_fft::params_t {
    static const unsigned phase_factor_width = 24;  // REQUIRED for float FFT (24 or 25)
    static const unsigned output_ordering = hls::ip_fft::natural_order;
    
    // CRITICAL FIX: Use UNSCALED mode to avoid internal NaN/Inf issues
    // HLS FFT scaled mode internally divides by 2 at each stage, causing numerical
    // instability with denormal numbers. Unscaled mode avoids this problem.
    // Compensation strategy:
    //   - Input scaling: INPUT_SCALE (1,000,000)
    //   - FFT scaling: NONE (unscaled mode)
    //   - Final compensation: (INPUT_SCALE)^2 = 1e12
    static const unsigned scaling_options = hls::ip_fft::unscaled;  // CRITICAL FIX
    
    // Implementation options: pipelined streaming for better throughput
    static const unsigned implementation_options = hls::ip_fft::pipelined_streaming_io;
};

// ============================================================================
// FFT Helper Functions
// ============================================================================

/**
 * Perform 32-point 1D FFT/IFFT
 * 
 * @param dir: Direction (0=FFT, 1=IFFT)
 * @param in_stream: Input stream (32 complex samples)
 * @param out_stream: Output stream (32 complex samples)
 */
void fft_1d_32(
    ap_uint<1> dir,                  // 0=forward, 1=inverse
    hls::stream<cmpxData_t>& in_stream,
    hls::stream<cmpxData_t>& out_stream
);

/**
 * Perform 32×32 2D IFFT using row-column decomposition
 * 
 * Process:
 * 1. Row-wise IFFT on each row
 * 2. Transpose matrix
 * 3. Column-wise IFFT (now rows after transpose)
 * 4. Transpose again to restore original layout
 * 
 * @param in_matrix: Input 32×32 complex matrix (frequency domain)
 * @param out_matrix: Output 32×32 complex matrix (spatial domain)
 */
void ifft_2d_32x32(
    cmpxData_t in_matrix[fftConvY][fftConvX],
    cmpxData_t out_matrix[fftConvY][fftConvX]
);

/**
 * Transpose a 32×32 complex matrix
 * 
 * @param in: Input matrix
 * @param out: Transposed output matrix
 */
void transpose_32x32(
    cmpxData_t in[fftConvY][fftConvX],
    cmpxData_t out[fftConvX][fftConvY]
);

/**
 * Build padded IFFT input from kernel×mask product
 * 
 * Embedding strategy (centered padding):
 * - The 9×9 product is placed at center of 32×32 array
 * - Offset: (32-9)/2 = 11 (both x and y)
 * 
 * @param mskf: Mask spectrum (512×512, via AXI-MM)
 * @param krn_r/krn_i: Single SOCS kernel (9×9)
 * @param padded: Output padded array (32×32)
 * @param Lxh/Lyh: Mask spectrum center (256, 256)
 */
void build_padded_ifft_input_32(
    float* mskf_r,       // Mask spectrum real part
    float* mskf_i,       // Mask spectrum imaginary part
    float* krn_r,        // Kernel real part (9×9)
    float* krn_i,        // Kernel imaginary part (9×9)
    cmpxData_t padded[fftConvY][fftConvX],
    int Lxh, int Lyh
);

/**
 * Extract valid 17×17 region from 32×32 IFFT output (OLD - for reference)
 * 
 * Extraction strategy (center crop):
 * - Extract center 17×17 region from 32×32 output
 * - Offset: (32-17)/2 = 7 (both x and y)
 * 
 * @param ifft_out: Full 32×32 IFFT output
 * @param valid: Extracted 17×17 region
 */
void extract_valid_17x17(
    cmpxData_t ifft_out[fftConvY][fftConvX],
    cmpxData_t valid[convY][convX]
);

/**
 * Accumulate intensity from IFFT result (OLD - for reference)
 * 
 * Formula: accum[y][x] += scale * (real^2 + imag^2)
 * 
 * @param ifft_valid: Valid IFFT output (17×17 complex)
 * @param accum: Accumulated intensity (17×17 real)
 * @param scale: Eigenvalue weight for this kernel
 */
void accumulate_intensity_17x17(
    cmpxData_t ifft_valid[convY][convX],
    float accum[convY][convX],
    float scale
);

/**
 * Apply fftshift to move zero-frequency to center (OLD - for reference)
 * 
 * For 17×17 array: swap quadrants
 * 
 * @param in: Input intensity array
 * @param out: Shifted output array
 */
void fftshift_17x17(
    float in[convY][convX],
    float out[convY][convX]
);

// ============================================================================
// NEW FUNCTIONS: Matching litho.cpp processing flow exactly
// ============================================================================

/**
 * Accumulate intensity on FULL 32×32 array with FFT scaling compensation
 * 
 * HLS FFT Scaled Mode Compensation:
 * - 32-point 1D IFFT divides by 32 (5 stages, each divides by 2)
 * - 2D IFFT (row-column) divides by 32 × 32 = 1024 total
 * - Intensity = |IFFT|^2, so need to multiply by 1024^2 = 1048576
 * 
 * Formula: accum[y][x] += scale * (re^2 + im^2) * FFT_SCALING_COMPENSATION
 * 
 * @param ifft_out: Full 32×32 IFFT output (scaled by HLS FFT)
 * @param accum: Accumulated intensity on 32×32 array
 * @param scale: Eigenvalue weight for this kernel
 */
void accumulate_intensity_32x32(
    cmpxData_t ifft_out[fftConvY][fftConvX],
    float accum[fftConvY][fftConvX],
    float scale
);

// FFT Input Scaling Strategy (to avoid numerical instability with small floats)
// Problem: HLS FFT may produce NaN when processing very small values (< 1e-35)
// Solution: Pre-scale input by INPUT_SCALE, then adjust compensation factor
// 
// CRITICAL FIX: Use UNSCALED FFT mode to avoid internal NaN/Inf issues
// Previous attempts with scaled mode failed due to internal FFT scaling operations
// 
// Strategy with UNSCALED FFT:
//   1. Input scaling: multiply kernel×mask product by INPUT_SCALE (1,000,000)
//   2. FFT processing: UNSCALED mode (no internal division)
//   3. Intensity calculation: |output|^2, then divide by INPUT_SCALE^2
// 
// Compensation factor: 1 / (INPUT_SCALE^2) = 1 / 1e12 = 1e-12
// 
// Maximum product value: ~2.37e-02 * 1e6 = 23700 (safe within float range)
// Denormal threshold: 1e-30 (flush extremely small values to zero)
#define INPUT_SCALE 1000000.0f
#define FFT_SCALING_COMPENSATION (1.0f / (INPUT_SCALE * INPUT_SCALE))  // For UNSCALED FFT mode
// Denormal threshold is defined locally in socs_fft.cpp (const float denormal_thresh = 1e-30f)

/**
 * Apply fftshift on 32×32 array (matching litho.cpp myShift)
 * 
 * litho.cpp: myShift(tmpImg, tmpImgp, fftConvX, fftConvY, true, true)
 * - xh = sizeX / 2 = 16, yh = sizeY / 2 = 16
 * - Swap quadrants: each pixel moves to opposite quadrant
 * 
 * @param in: Input 32×32 intensity array
 * @param out: Shifted 32×32 output array
 */
void fftshift_32x32(
    float in[fftConvY][fftConvX],
    float out[fftConvY][fftConvX]
);

/**
 * Extract center 17×17 from 32×32 array after fftshift
 * 
 * litho.cpp golden output extraction:
 * - Extract center region from tmpImgp (32×32) -> tmpImgp_center (17×17)
 * - Offset: (32-17)/2 = 7 (both x and y)
 * 
 * @param in_32: Full 32×32 intensity array (after fftshift)
 * @param out_17: Extracted center 17×17 region
 */
void extract_center_17x17_from_32(
    float in_32[fftConvY][fftConvX],
    float out_17[convY][convX]
);

#endif // SOCS_FFT_H