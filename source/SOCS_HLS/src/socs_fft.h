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

// ============================================================================
// FFT Parameters (Matching reference implementation)
// ============================================================================
// CRITICAL: Match vitis_hls_fft/interface_stream/fft_top.h exactly!
// Reference uses MINIMAL parameter override (only what's needed)
const char FFT_INPUT_WIDTH = 32;        // 32-bit input (Q1.31 format)
const char FFT_OUTPUT_WIDTH = 32;       // Same as input
const char FFT_TWIDDLE_WIDTH = 16;      // Standard twiddle width
const char FFT_STATUS_WIDTH = 8;        // CRITICAL: Must be 8 (reference value)
const char FFT_CHANNELS = 1;            // Single channel
const int  FFT_LENGTH = 32;             // 32-point FFT
const char FFT_NFFT_MAX = 5;            // log2(32) = 5
const bool FFT_HAS_NFFT = 0;            // Fixed length (not runtime configurable)

// ============================================================================
// Data Types (FIXED-POINT for numerical stability)
// ============================================================================
// CRITICAL: FFT IP uses TYPE DERIVATION FORMULA!
// Vitis HLS FFT derives types as: ap_fixed<((input_width+7)/8)*8, 1>
// 
// For input_width=32: ((32+7)/8)*8 = (39/8)*8 = 4*8 = 32
// Result: ap_fixed<32, 1> (Q1.31 format)
//
// CRITICAL: Must define types USING THE DERIVATION FORMULA!
// If types don't match derivation, FFT template will reject compilation
#define FFT_INPUT_WIDTH_PADDED (((FFT_INPUT_WIDTH+7)/8)*8)  // 32 for input_width=32
#define FFT_OUTPUT_WIDTH_PADDED (((FFT_OUTPUT_WIDTH+7)/8)*8)  // 32 for output_width=32

typedef ap_fixed<FFT_INPUT_WIDTH_PADDED, 1> fixed_fft_t;    // FFT IP native type: Q1.31
typedef ap_fixed<FFT_OUTPUT_WIDTH_PADDED, FFT_OUTPUT_WIDTH_PADDED-FFT_INPUT_WIDTH+1> fixed_fft_out_t;  // Output type
typedef std::complex<fixed_fft_t> cmpxFixedFFT_t;
typedef std::complex<fixed_fft_out_t> cmpxFixedFFTOut_t;

// ============================================================================
// Input Scaling Constants
// ============================================================================
// CRITICAL SCALING FIX: Data is ALREADY within [-1, 1) range!
// Analysis:
//   - Mask spectrum magnitude: [9.84e-9, 6.16e-2] (max 0.06)
//   - Kernel magnitude: [0, 0.41] (max 0.41)
//   - Product max ≈ 0.06 × 0.41 ≈ 0.025 → ALREADY within Q1.31 range!
//
// Previous error: INPUT_SCALE = 2^20 caused saturation!
//   - 0.025 × 2^20 ≈ 26,214 → saturated to ±1 in Q1.31
//   - Effective scaling only ~40, not 2^20
//
// CORRECTED Strategy:
//   - NO input scaling (INPUT_SCALE = 1)
//   - Data enters FFT directly (already in [-1, 1) range)
//   - Output compensation: scaled mode divides by N² = 1024
//   - intensity = scale × |HLS_output|^2 × 1024² = scale × |original_IFFT|^2
#define FFT_INPUT_SCALE_FACTOR 1.0f          // NO scaling - data already in range
#define FFT_OUTPUT_SCALE_FACTOR 1048576.0f  // N^4 = 32^4 = 1024^2 = 2^20 (output compensation)

// ============================================================================
// FFT Configuration for 32-point FFT (MINIMAL OVERRIDE)
// ============================================================================
// FFT Configuration for 32-point FFT (EXPLICIT OVERRIDE)
// ============================================================================
// CRITICAL LESSON: config_width MUST match actual config_bits requirement!
// 
// Problem discovered:
// - params_t default: config_width = 16 (hls_fft.h:137)
// - Our overrides change config_bits calculation → requires width=8
// - Runtime check (hls_fft.h:208-211): ((config_bits+7)>>3)<<3 must match config_width
// - Mismatch causes "Config channel width = 16 is illegal" error
//
// Reference implementation (fft_top.h):
// - Uses default input_width=16 → config_bits small → config_width=16 matches
// - Only overrides output_ordering → minimal config_bits change
//
// Our implementation:
// - Overrides input_width=32, output_width=32, log2_nfft=5 → config_bits needs width=8
// - MUST override config_width=8 to match actual requirement
//
// MUST override fields:
// 1. input_width = 32 (default=16 doesn't match our data type)
// 2. output_width = 32 (must match input_width)
// 3. config_width = 8 (CRITICAL: default=16, actual need=8)
// 4. status_width = 8 (explicit, matches default)
// 5. log2_transform_length = 5 (for 32-point FFT)
// 6. scaling_options = scaled (for fixed-point FFT)
// 7. output_ordering = natural_order
struct fft_config_32_fixed : hls::ip_fft::params_t {
    // MUST override: input/output width (default=16 doesn't match our 32)
    static const unsigned input_width = FFT_INPUT_WIDTH;        // 32-bit
    static const unsigned output_width = FFT_OUTPUT_WIDTH;      // 32-bit
    
    // MUST override: config_width
    // For pipelined_streaming_io with custom input_width: need 8
    // For radix_2_lite_burst_io: need 16 (default)
    static const unsigned config_width = 8;
    
    // MUST override: status_width (explicit for clarity)
    static const unsigned status_width = 8;                     // Default value
    
    // MUST override: transform length
    static const unsigned log2_transform_length = FFT_NFFT_MAX;  // log2(32) = 5
    
    // MUST override: scaling mode (fixed-point requires scaled mode)
    static const unsigned scaling_options = hls::ip_fft::scaled;
    
    // MUST override: output ordering
    static const unsigned output_ordering = hls::ip_fft::natural_order;
    
    // CRITICAL FIX for IP Packaging: Force BRAM stages = 0
    static const unsigned number_of_stages_using_block_ram_for_data_and_phase_factors = 0;
    
    // DO NOT override implementation_options - use default (auto)
    // Previous attempts:
    // - pipelined_streaming_io: CoSim FAIL (port mismatch)
    // - radix_2_lite_burst_io: CSim FAIL (config_width requirement)
    // Let HLS choose automatically
    
    // Leave as default: phase_factor_width, implementation_options
};

// Backward compatibility alias (use fixed config)
typedef fft_config_32_fixed fft_config_32;

// ============================================================================
// Legacy Compatibility Types
// ============================================================================
// For existing code that uses cmpxData_t, map to fixed-point types
// IMPORTANT: These types now use Q1.31 format (integer bits = 1)
typedef cmpxFixedFFT_t cmpxData_t;  // Map to FFT native type

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

// ============================================================================
// FIXED-POINT FFT Scaling Strategy
// ============================================================================
// FIXED-POINT FFT uses SCALED mode, which automatically handles scaling:
// - Scaled mode divides by 2 at each FFT stage
// - For 32-point FFT: 5 stages, output is divided by 32
// - For 2D IFFT: output is divided by 32×32 = 1024
// 
// CRITICAL: Fixed-point FFT is inherently numerically stable:
// - No NaN/Inf issues (fixed-point arithmetic is bounded)
// - No denormal numbers (fixed-point has no subnormal range)
// - No INPUT_SCALE compensation needed (scaled mode handles range)
// 
// Input range: any fixed-point value (bounded by ap_fixed<32,16> range)
// Output range: input_range / 1024 (automatically scaled)
// 
// REMOVED: INPUT_SCALE and FFT_SCALING_COMPENSATION (no longer needed for fixed-point)

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