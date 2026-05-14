/**
 * SOCS 2048 Stream Refactored v16 - HLS FFT IP with Block Floating Point
 * 
 * PURPOSE: Fix precision issue in v14 (ap_fixed<24,1> insufficient)
 * 
 * CHANGES from v14:
 * - Use block floating point mode (allows arbitrary integer width)
 * - Increase precision: ap_fixed<24,1> → ap_fixed<32,8>
 * - Track blk_exp for compensation
 * 
 * EXPECTED RESULT: RMSE < 1e-5 (matching v13 Direct DFT)
 */

#ifndef SOCS_2048_STREAM_REFACTORED_V16_HLS_FFT_H
#define SOCS_2048_STREAM_REFACTORED_V16_HLS_FFT_H

#include <complex>
#include <ap_fixed.h>

// ============================================================================
// Constants (golden_1024 config)
// ============================================================================
const int MAX_FFT_X = 128;
const int MAX_FFT_Y = 128;
const int MAX_KERNEL_SIZE = 17;  // 2*Nx+1 for Nx=8

// ============================================================================
// Type Definitions
// ============================================================================
typedef std::complex<float> cmpx_2048_t;

// ============================================================================
// Helper Functions (from v13)
// ============================================================================

/**
 * Embed kernel into mask spectrum with zero-padding
 */
void embed_kernel_mask_padded_2048(
    float* mskf_r, float* mskf_i,
    float* krn_r, float* krn_i,
    cmpx_2048_t fft_input[MAX_FFT_Y][MAX_FFT_X],
    int nx_actual, int ny_actual,
    int kernel_idx,
    int Lx, int Ly
);

/**
 * Accumulate intensity from IFFT output
 */
void accumulate_intensity_2048(
    cmpx_2048_t fft_output[MAX_FFT_Y][MAX_FFT_X],
    float tmpImg[MAX_FFT_Y][MAX_FFT_X],
    float scale
);

/**
 * 2D FFTshift
 */
void fftshift_2d_2048(
    float input[MAX_FFT_Y][MAX_FFT_X],
    float output[MAX_FFT_Y][MAX_FFT_X]
);

// ============================================================================
// Main Function (v16: HLS FFT IP with Block Floating Point)
// ============================================================================

void calc_socs_2048_hls_stream_refactored_v16(
    float* mskf_r, float* mskf_i,
    float* krn_r, float* krn_i,
    float* scales,
    float* tmpImg_ddr,
    float* output,
    int nk,
    int nx_actual,
    int ny_actual,
    int Lx,
    int Ly
);

#endif // SOCS_2048_STREAM_REFACTORED_V16_HLS_FFT_H
