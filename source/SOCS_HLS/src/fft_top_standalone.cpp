/**
 * Standalone HLS FFT Test Module (Exact Reference Pattern)
 * Purpose: Verify FFT configuration works before integrating into SOCS
 */

#ifdef __SYNTHESIS__

#include "hls_fft_config_2048_final.h"
#include <hls_stream.h>

// Standalone FFT top function (matching reference exactly)
void fft_top_standalone(
    ap_uint<1> dir,
    ap_uint<15> scaling,
    hls::stream<cmpx_hls_2048_t>& xn,
    hls::stream<cmpx_hls_out_2048_t>& xk,
    bool* status
) {
#pragma HLS interface ap_fifo depth=128 port=xn,xk
#pragma HLS interface ap_fifo depth=1 port=status
#pragma HLS stream variable=xn
#pragma HLS stream variable=xk
#pragma HLS dataflow

    hls::fft<config_fft_2048>(xn, xk, dir, scaling, -1, status);
}

#endif // __SYNTHESIS__