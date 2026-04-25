/**
 * @file socs_2048_stream_optimized.h
 * @brief Header for optimized stream-based SOCS implementation
 */

#ifndef SOCS_2048_STREAM_OPTIMIZED_H
#define SOCS_2048_STREAM_OPTIMIZED_H

#include <complex>

// FFT size constants
#define MAX_FFT_X 128
#define MAX_FFT_Y 128

// Type definitions
typedef std::complex<float> cmpx_2048_t;

// Main function declaration
void calc_socs_2048_hls_stream_optimized(
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

#endif // SOCS_2048_STREAM_OPTIMIZED_H
