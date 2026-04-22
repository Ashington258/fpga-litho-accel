/**
 * Standalone 128-point FFT Test Module (Corrected Pattern)
 * Follows reference implementation structure exactly
 */

#include "fft_config_128_corrected.h"
#include <hls_stream.h>

void fft_top_128(
    ap_uint<1> dir,
    ap_uint<15> scaling,
    hls::stream<cmpx_in_128_t>& xn,
    hls::stream<cmpx_out_128_t>& xk,
    bool* status
) {
#pragma HLS interface ap_fifo depth=128 port=xn,xk
#pragma HLS interface ap_fifo depth=1 port=status
#pragma HLS stream variable=xn
#pragma HLS stream variable=xk
#pragma HLS dataflow

    hls::fft<config_128>(xn, xk, dir, scaling, -1, status);
}