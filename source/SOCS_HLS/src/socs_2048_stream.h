/**
 * @file socs_2048_stream.h
 * @brief Header file for SOCS HLS stream-based implementation
 */

#ifndef SOCS_2048_STREAM_H
#define SOCS_2048_STREAM_H

#include <hls_stream.h>
#include <complex>

using namespace std;

// Type definitions
typedef complex<float> cmpx_2048_t;

// FFT size constants
#define MAX_FFT_X 128
#define MAX_FFT_Y 128

// Function declarations

/**
 * @brief Embed kernel × mask product (Stream-based)
 */
void embed_kernel_stream(
    float* mskf_r, float* mskf_i,
    float* krn_r, float* krn_i,
    hls::stream<cmpx_2048_t>& fft_in_stream,
    int nx_actual, int ny_actual,
    int kernel_idx,
    int Lx, int Ly
);

/**
 * @brief 2D FFT with stream interface
 */
void fft_2d_stream(
    hls::stream<cmpx_2048_t>& in_stream,
    hls::stream<cmpx_2048_t>& out_stream,
    bool is_inverse
);

/**
 * @brief Accumulate intensity from stream
 */
void accumulate_stream(
    hls::stream<cmpx_2048_t>& ifft_stream,
    float tmpImg[MAX_FFT_Y][MAX_FFT_X],
    float scale
);

/**
 * @brief FFTshift and output extraction
 */
void fftshift_and_output(
    float tmpImg[MAX_FFT_Y][MAX_FFT_X],
    float* output,
    int nx_actual,
    int ny_actual
);

/**
 * @brief Main SOCS function with DATAFLOW optimization
 */
void calc_socs_2048_hls_stream(
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

#endif // SOCS_2048_STREAM_H
