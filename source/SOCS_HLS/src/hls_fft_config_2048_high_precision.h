/**
 * FFT Configuration for SOCS HLS (Higher Precision Version)
 * 
 * Purpose: Test if higher precision (ap_fixed<32,8>) resolves NaN issue
 * 
 * Changes from corrected version:
 *   - Increased precision from 24-bit to 32-bit
 *   - Increased integer part from 1-bit to 8-bit (range: -128 to +127)
 */

#ifndef HLS_FFT_CONFIG_2048_HIGH_PRECISION_H
#define HLS_FFT_CONFIG_2048_HIGH_PRECISION_H

#include <ap_fixed.h>
#include <complex>
#include <hls_fft.h>

// ============================================================================
// FFT Parameters (Higher Precision) - Global Constants
// NOTE: HLS FFT library requires these specific global constant names
// ============================================================================

const char FFT_INPUT_WIDTH                     = 32;
const char FFT_OUTPUT_WIDTH                    = FFT_INPUT_WIDTH;
const char FFT_STATUS_WIDTH                    = 8;
const char FFT_CHANNELS                        = 1;
const int  FFT_LENGTH                          = 128; 
const char FFT_NFFT_MAX                        = 7;  // log2(128) = 7
const bool FFT_HAS_NFFT                        = 0;
const hls::ip_fft::arch FFT_ARCH               = hls::ip_fft::pipelined_streaming_io;
const char FFT_TWIDDLE_WIDTH                   = 32;
const hls::ip_fft::ordering FFT_OUTPUT_ORDER   = hls::ip_fft::natural_order;
const bool FFT_OVFLO                           = 1;
const bool FFT_CYCLIC_PREFIX_INSERTION         = 0;
const bool FFT_XK_INDEX                        = 0;
const hls::ip_fft::scaling FFT_SCALING         = hls::ip_fft::scaled;
const hls::ip_fft::rounding FFT_ROUNDING       = hls::ip_fft::truncation;

// Not configurable yet
const hls::ip_fft::mem FFT_MEM_DATA            = hls::ip_fft::block_ram;
const hls::ip_fft::mem FFT_MEM_PHASE_FACTORS   = hls::ip_fft::block_ram;
const hls::ip_fft::mem FFT_MEM_REORDER         = hls::ip_fft::block_ram;
const char FFT_STAGES_BLOCK_RAM                = 4;
const bool FFT_MEM_OPTIONS_HYBRID              = 0;
const hls::ip_fft::opt FFT_COMPLEX_MULT_TYPE   = hls::ip_fft::use_luts;
const hls::ip_fft::opt FFT_BUTTERLY_TYPE       = hls::ip_fft::use_luts;

// ============================================================================
// Type Definitions (Higher Precision)
// ============================================================================

// Input type: ap_fixed<32, 8> (Higher precision)
// Integer part: 8 bits (range: -128 to +127)
// Fractional part: 24 bits
typedef ap_fixed<FFT_INPUT_WIDTH, 8> data_in_hp_t;
typedef std::complex<data_in_hp_t> cmpxDataIn_hp_t;

// Output type: ap_fixed<32, 8> (same as input for simplicity)
typedef ap_fixed<FFT_OUTPUT_WIDTH, 8> data_out_hp_t;
typedef std::complex<data_out_hp_t> cmpxDataOut_hp_t;

// ============================================================================
// FFT Config Struct (Higher Precision)
// ============================================================================

struct config_hp : hls::ip_fft::params_t {
    static const unsigned output_ordering = hls::ip_fft::natural_order;
};

typedef hls::ip_fft::config_t<config_hp> config_t_hp;
typedef hls::ip_fft::status_t<config_hp> status_t_hp;

// ============================================================================
// FFT Helper Functions (Higher Precision)
// ============================================================================

/**
 * 1D HLS FFT IP call (128-point, higher precision)
 */
inline void fft_1d_hls_128_hp(
    hls::stream<cmpxDataIn_hp_t>& in_stream,
    hls::stream<cmpxDataOut_hp_t>& out_stream,
    ap_uint<1> dir,
    ap_uint<15> scaling
) {
    #pragma HLS interface ap_fifo depth=128 port=in_stream,out_stream
    #pragma HLS stream variable=in_stream
    #pragma HLS stream variable=out_stream
    #pragma HLS dataflow
    
    bool status;
    hls::fft<config_hp>(in_stream, out_stream, dir, scaling, -1, &status);
}

/**
 * 2D HLS FFT (128×128, higher precision)
 */
inline void fft_2d_hls_128_hp(
    cmpxDataIn_hp_t input[128][128],
    cmpxDataOut_hp_t output[128][128],
    bool is_inverse
) {
    #pragma HLS DATAFLOW
    
    hls::stream<cmpxDataIn_hp_t> row_stream("row_stream");
    hls::stream<cmpxDataOut_hp_t> row_fft_stream("row_fft_stream");
    hls::stream<cmpxDataIn_hp_t> col_stream("col_stream");
    hls::stream<cmpxDataOut_hp_t> col_fft_stream("col_fft_stream");
    
    #pragma HLS stream variable=row_stream depth=128
    #pragma HLS stream variable=row_fft_stream depth=128
    #pragma HLS stream variable=col_stream depth=128
    #pragma HLS stream variable=col_fft_stream depth=128
    
    cmpxDataIn_hp_t temp[128][128];
    #pragma HLS RESOURCE variable=temp core=RAM_2P_BRAM
    
    ap_uint<1> dir = is_inverse ? 1 : 0;
    ap_uint<15> scaling = 0x7F;  // Scaled mode for all stages
    
    // Stage 1: Row FFT
    for (int row = 0; row < 128; row++) {
        #pragma HLS UNROLL factor=4
        
        for (int col = 0; col < 128; col++) {
            #pragma HLS PIPELINE II=1
            row_stream.write(input[row][col]);
        }
        
        fft_1d_hls_128_hp(row_stream, row_fft_stream, dir, scaling);
        
        for (int col = 0; col < 128; col++) {
            #pragma HLS PIPELINE II=1
            temp[row][col] = row_fft_stream.read();
        }
    }
    
    // Stage 2: Column FFT
    for (int col = 0; col < 128; col++) {
        #pragma HLS UNROLL factor=4
        
        for (int row = 0; row < 128; row++) {
            #pragma HLS PIPELINE II=1
            col_stream.write(temp[row][col]);
        }
        
        fft_1d_hls_128_hp(col_stream, col_fft_stream, dir, scaling);
        
        for (int row = 0; row < 128; row++) {
            #pragma HLS PIPELINE II=1
            output[row][col] = col_fft_stream.read();
        }
    }
}

#endif // HLS_FFT_CONFIG_2048_HIGH_PRECISION_H
