/**
 * SOCS 2048 Stream Refactored v14 - HLS FFT IP Integration
 * 
 * PURPOSE: Replace Direct DFT with HLS FFT IP to resolve RTL simulation timeout
 * 
 * KEY CHANGES from v13:
 * - Uses HLS FFT IP (hls::fft) for BOTH C Simulation and Co-Simulation
 * - No conditional compilation (#ifdef __SYNTHESIS__) - single code path
 * - O(N log N) complexity instead of O(N²) → ~18× faster
 * - Expected RTL simulation time: ~70ms per kernel (vs 1.262s with Direct DFT)
 * 
 * SCALING BEHAVIOR:
 * - HLS FFT scaled mode (scaling=0x155): divides by N per dimension automatically
 * - 2D IFFT total scaling: 1/(128×128) = 1/16384
 * - Matches FFTW BACKWARD behavior → NO manual N² compensation needed
 * 
 * REUSED FUNCTIONS (from socs_2048.cpp):
 * - embed_kernel_mask_padded_2048()
 * - accumulate_intensity_2048()
 * - fftshift_2d_2048()
 */

#ifndef SOCS_2048_STREAM_REFACTORED_V14_HLS_FFT_H
#define SOCS_2048_STREAM_REFACTORED_V14_HLS_FFT_H

#include <complex>
#include <ap_fixed.h>

// Type definitions
typedef std::complex<float> cmpx_2048_t;
typedef ap_fixed<24, 1, AP_TRN, AP_WRAP> fixed_24_1_t;
typedef std::complex<fixed_24_1_t> cmpx_fft_in_t;

// Architecture constants (golden_1024 config)
const int MAX_MASK_X = 1024;
const int MAX_MASK_Y = 1024;
const int MAX_FFT_X = 128;
const int MAX_FFT_Y = 128;
const int MAX_KERNEL_SIZE = 17;
const int MAX_NK = 10;

// Forward declarations of functions from socs_2048.cpp
extern void embed_kernel_mask_padded_2048(
    float* mskf_r, float* mskf_i,
    float* krn_r, float* krn_i,
    cmpx_2048_t fft_input[MAX_FFT_Y][MAX_FFT_X],
    int nx_actual, int ny_actual,
    int kernel_idx,
    int Lx, int Ly
);

extern void accumulate_intensity_2048(
    cmpx_2048_t ifft_output[MAX_FFT_Y][MAX_FFT_X],
    float tmpImg[MAX_FFT_Y][MAX_FFT_X],
    float scale
);

extern void fftshift_2d_2048(
    float input[MAX_FFT_Y][MAX_FFT_X],
    float output[MAX_FFT_Y][MAX_FFT_X]
);

// Main function
void calc_socs_2048_hls_stream_refactored_v14(
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

#endif // SOCS_2048_STREAM_REFACTORED_V14_HLS_FFT_H
