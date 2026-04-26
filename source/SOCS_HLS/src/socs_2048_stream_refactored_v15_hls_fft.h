/**
 * SOCS 2048 Stream Refactored v15 - HLS FFT IP with Higher Precision
 */

#ifndef SOCS_2048_STREAM_REFACTORED_V15_HLS_FFT_H
#define SOCS_2048_STREAM_REFACTORED_V15_HLS_FFT_H

#include <complex>
#include <ap_fixed.h>

// Type definitions
typedef std::complex<float> cmpx_2048_t;

// Architecture constants (golden_1024 config)
const int MAX_MASK_X = 1024;
const int MAX_MASK_Y = 1024;
const int MAX_FFT_X = 128;
const int MAX_FFT_Y = 128;
const int MAX_KERNEL_SIZE = 17;
const int MAX_NK = 10;

// Forward declarations of v12 functions (from socs_2048.cpp)
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
void calc_socs_2048_hls_stream_refactored_v15(
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

#endif // SOCS_2048_STREAM_REFACTORED_V15_HLS_FFT_H
