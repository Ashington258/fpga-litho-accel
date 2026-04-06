/*
 * FFT Top Level Implementation
 * SOCS HLS Project
 */

#include "fft_top.h"

void fft_inverse_top(
    hls::stream<fft_complex_t>& xn,
    hls::stream<fft_complex_t>& xk,
    bool* ovflo
) {
    // 完全参考vitis_hls_fft的接口设计
#pragma HLS INTERFACE ap_fifo depth=1024 port=xn
#pragma HLS INTERFACE ap_fifo depth=1024 port=xk
#pragma HLS INTERFACE ap_fifo depth=1 port=ovflo
#pragma HLS STREAM variable=xn depth=1024
#pragma HLS STREAM variable=xk depth=1024
#pragma HLS DATAFLOW

    // 调用FFT IP
    hls::fft<fft_config_32>(xn, xk, 1, 0, -1, ovflo);
}