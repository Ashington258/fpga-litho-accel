/**
 * HLS FFT Configuration for SOCS 2048 - v15 High Precision
 * 
 * PURPOSE: Fix precision issue in v14 (ap_fixed<24,1> insufficient)
 * 
 * CHANGES from v14:
 * - Increase precision: ap_fixed<24,1> → ap_fixed<32,8>
 * - Integer part: 1 bit → 8 bits (dynamic range: -128 to +127)
 * - Fractional part: 23 bits → 24 bits (precision: 2^-24 ≈ 6e-8)
 * 
 * EXPECTED RESULT: RMSE < 1e-5 (matching v13 Direct DFT)
 */

#ifndef HLS_FFT_CONFIG_2048_V15_H
#define HLS_FFT_CONFIG_2048_V15_H

#include <ap_fixed.h>
#include <hls_fft.h>
#include <hls_stream.h>
#include <complex>

// ============================================================================
// FFT Constants (golden_1024 config)
// ============================================================================
const int FFT_NFFT_MAX_128 = 7;      // log2(128) = 7
const int FFT_INPUT_WIDTH_32 = 32;   // v15: Increased from 24 to 32
const int FFT_OUTPUT_WIDTH_32 = 32;  // v15: Increased from 24 to 32

// ============================================================================
// Type Definitions (v15: Higher Precision)
// ============================================================================
// v14: ap_fixed<24, 1> - INSUFFICIENT (integer overflow for values > 1.0)
// v15: ap_fixed<32, 8> - SUFFICIENT (8-bit integer, 24-bit fractional)
typedef ap_fixed<32, 8, AP_TRN, AP_WRAP> data_fft_in_t;   // v15: 8-bit integer, 24-bit fractional
typedef ap_fixed<32, 8, AP_TRN, AP_WRAP> data_fft_out_t;  // v15: 8-bit integer, 24-bit fractional
typedef std::complex<data_fft_in_t> cmpx_fft_in_t;
typedef std::complex<data_fft_out_t> cmpx_fft_out_t;

// ============================================================================
// FFT Config Struct (v15: Higher Precision)
// ============================================================================
struct config_socs_fft_v15 : hls::ip_fft::params_t {
    static const unsigned log2_transform_length = FFT_NFFT_MAX_128;  // 7 for 128-point
    static const unsigned input_width = FFT_INPUT_WIDTH_32;          // v15: 32-bit
    static const unsigned output_width = FFT_OUTPUT_WIDTH_32;        // v15: 32-bit
    static const unsigned output_ordering = hls::ip_fft::natural_order;
    static const unsigned implementation_options = hls::ip_fft::pipelined_streaming_io;
    static const unsigned scaling_options = hls::ip_fft::scaled;
};

// ============================================================================
// FFT Helper Functions (v15: Higher Precision)
// ============================================================================

/**
 * 1D HLS FFT IP call (128-point, v15: Higher Precision)
 */
inline void fft_1d_hls_128_v15(
    hls::stream<cmpx_fft_in_t>& in_stream,
    hls::stream<cmpx_fft_out_t>& out_stream,
    bool is_inverse
) {
    #pragma HLS interface ap_fifo depth=128 port=in_stream,out_stream
    #pragma HLS stream variable=in_stream
    #pragma HLS stream variable=out_stream
    #pragma HLS dataflow
    
    ap_uint<1> dir = is_inverse ? 1 : 0;
    
    // CRITICAL: For nfft=7 (odd), final stage scaling must be 0 or 1
    // Use 0x055 = 0b001010101 (4 stages of 1/2 shift = 1/16 total)
    ap_uint<15> scaling = 0x055;
    
    bool ovflo;
    unsigned blk_exp;
    
    hls::fft<config_socs_fft_v15>(in_stream, out_stream, dir, scaling, -1, &ovflo, &blk_exp);
}

/**
 * 2D HLS FFT using stream interface (128×128, v15: Higher Precision)
 */
inline void fft_2d_hls_128_v15(
    cmpx_fft_in_t input[128][128],
    cmpx_fft_out_t output[128][128],
    bool is_inverse
) {
    #pragma HLS DATAFLOW
    
    hls::stream<cmpx_fft_in_t> row_stream("row_stream");
    hls::stream<cmpx_fft_out_t> row_fft_stream("row_fft_stream");
    hls::stream<cmpx_fft_in_t> col_stream("col_stream");
    hls::stream<cmpx_fft_out_t> col_fft_stream("col_fft_stream");
    
    #pragma HLS stream variable=row_stream depth=128
    #pragma HLS stream variable=row_fft_stream depth=128
    #pragma HLS stream variable=col_stream depth=128
    #pragma HLS stream variable=col_fft_stream depth=128
    
    cmpx_fft_out_t temp[128][128];
    #pragma HLS RESOURCE variable=temp core=RAM_2P_BRAM
    
    // Stage 1: Row FFT
    for (int row = 0; row < 128; row++) {
        for (int col = 0; col < 128; col++) {
            #pragma HLS PIPELINE II=1
            row_stream.write(input[row][col]);
        }
        
        fft_1d_hls_128_v15(row_stream, row_fft_stream, is_inverse);
        
        for (int col = 0; col < 128; col++) {
            #pragma HLS PIPELINE II=1
            temp[row][col] = row_fft_stream.read();
        }
    }
    
    // Stage 2: Column FFT (with transpose)
    for (int col = 0; col < 128; col++) {
        for (int row = 0; row < 128; row++) {
            #pragma HLS PIPELINE II=1
            col_stream.write(temp[row][col]);
        }
        
        fft_1d_hls_128_v15(col_stream, col_fft_stream, is_inverse);
        
        for (int row = 0; row < 128; row++) {
            #pragma HLS PIPELINE II=1
            output[row][col] = col_fft_stream.read();
        }
    }
}

#endif // HLS_FFT_CONFIG_2048_V15_H
