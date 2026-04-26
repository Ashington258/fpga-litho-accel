/**
 * HLS FFT Configuration for SOCS 2048 - v16 Block Floating Point Mode
 * 
 * PURPOSE: Achieve RMSE < 1e-5 matching v13 Direct DFT
 * 
 * EVOLUTION:
 * - v14: ap_fixed<24,1> scaled mode → RMSE=0.133 (insufficient precision)
 * - v16a: ap_fixed<32,1> scaled mode (0x1F) → RMSE=2.99e-05 (direction fix + scaling OK, but per-stage truncation)
 * - v16b: ap_fixed<32,1> block_float mode → target RMSE < 1e-5 (no intermediate truncation)
 * 
 * KEY INSIGHT: Scaled mode truncates bits at EVERY butterfly stage (7 stages × 2D = 14 truncations)
 * Block floating point only adjusts exponent at stage boundaries, preserving mantissa precision
 * 
 * CRITICAL: HLS FFT direction convention:
 *   fwd_inv=1 → Forward FFT (time→freq)
 *   fwd_inv=0 → Inverse FFT (freq→time)
 *   ⚠️ This is OPPOSITE to intuitive "is_inverse=1 means IFFT"!
 *   When is_inverse=true, must pass fwd_inv=0
 * 
 * EXPECTED RESULT: RMSE < 1e-5 (matching v13 Direct DFT = 8.324e-07)
 */

#ifndef HLS_FFT_CONFIG_2048_V16_H
#define HLS_FFT_CONFIG_2048_V16_H

#include <ap_fixed.h>
#include <hls_fft.h>
#include <hls_stream.h>
#include <complex>

// ============================================================================
// FFT Constants (golden_1024 config)
// ============================================================================
const int FFT_NFFT_MAX_128 = 7;      // log2(128) = 7
const int FFT_INPUT_WIDTH_40 = 40;   // v16b: Aligned to 40 bits (HLS requirement: ((32+7)/8)*8)
const int FFT_OUTPUT_WIDTH_48 = 48;  // v16b: Aligned to 48 bits (HLS requirement: ((40+7)/8)*8)

// ============================================================================
// Type Definitions (v16b: Block Floating Point Mode)
// ============================================================================
// v14: ap_fixed<24,1> scaled mode → RMSE=0.133 (insufficient)
// v16a: ap_fixed<32,1> scaled mode → RMSE=2.99e-05 (per-stage truncation)
// v16b: ap_fixed<32,1> block_float mode → target < 1e-5 (no intermediate truncation)
//
// Block floating point mode:
// - Output width = input_width (NOT input_width + max_nfft + 1)
// - blk_exp parameter tracks total scaling applied
// - Compensation: output * 2^(blk_exp) to restore correct magnitude
//
// CRITICAL: FFT IP input_width must be in range [8, 34]
// - Use 32-bit (within valid range)
// - HLS FFT will NOT perform additional alignment (32 is already 8-byte aligned)
//
// CRITICAL: Do NOT use AP_TRN, AP_WRAP parameters - HLS FFT expects simple ap_fixed<N, I>
typedef ap_fixed<32, 1> data_fft_in_t;    // v16b: 32-bit (within FFT IP valid range)
typedef ap_fixed<32, 1> data_fft_out_t;   // v16b: Same as input for block_float mode
typedef std::complex<data_fft_in_t> cmpx_fft_in_t;
typedef std::complex<data_fft_out_t> cmpx_fft_out_t;

// ============================================================================
// FFT Config Struct (v16b: Block Floating Point Mode)
// ============================================================================
struct config_socs_fft_v16 : hls::ip_fft::params_t {
    // Transform length (NEW API only)
    static const unsigned log2_transform_length = FFT_NFFT_MAX_128;  // 7 for 128-point
    static const bool run_time_configurable_transform_length = false;
    
    // Data widths (CRITICAL: FFT IP requires input_width in [8, 34])
    static const unsigned input_width = 32;                          // v16b: 32-bit (within valid range)
    static const unsigned output_width = 32;                         // v16b: Same as input for block_float
    static const unsigned phase_factor_width = 24;                   // Twiddle factor width
    
    // Architecture (NEW API only)
    static const unsigned implementation_options = hls::ip_fft::pipelined_streaming_io;
    
    // Output ordering (NEW API only)
    static const unsigned output_ordering = hls::ip_fft::natural_order;
    
    // Channels
    static const unsigned channels = 1;
    
    // CRITICAL: Use block floating point mode for maximum precision (NEW API only)
    // - NO per-stage truncation (unlike scaled mode)
    // - blk_exp parameter tracks total scaling
    // - Compensation: output * 2^(blk_exp)
    static const unsigned scaling_options = hls::ip_fft::block_floating_point;
    
    // Rounding (NEW API only)
    static const unsigned rounding_modes = hls::ip_fft::truncation;
    
    // Overflow
    static const bool ovflo = true;
    
    // Memory options (NEW API only)
    static const unsigned memory_options_data = hls::ip_fft::block_ram;
    static const unsigned memory_options_phase_factors = hls::ip_fft::block_ram;
    static const unsigned memory_options_reorder = hls::ip_fft::block_ram;
    
    // Implementation options
    static const unsigned complex_mult_type = hls::ip_fft::use_luts;
    static const unsigned butterfly_type = hls::ip_fft::use_luts;
    
    // SSR (NEW API only)
    static const unsigned super_sample_rates = hls::ip_fft::ssr_1;
    
    // Config width calculation for block_float mode:
    // - log2_transform_length = 7, run_time_configurable_transform_length = false → nfft_bits = 0
    // - cyclic_prefix_insertion = false → cp_len_bits = 0
    // - channels = 1 → ch_bits = 1
    // - implementation_options = pipelined_streaming_io → tmp_bits = ((7+1)>>1) * 2 = 8
    // - scaling_options = block_float → sch_bits = 0 (no scaling schedule)
    // - config_bits = (0 + 1) * 1 + 0 + 0 = 1
    // - config_width = ((1 + 7) >> 3) << 3 = 8
    static const unsigned config_width = 8;
    
    // Status width calculation for block_float mode:
    // - has_ovflo = ovflo && (scaling_opt == scaled) = true && false = false
    // - blk_exp_bits = 8 (padding to 8 bits)
    // - ovflo_bits = 0 (has_ovflo = false)
    // - status_bits = (8 + 0) * 1 = 8
    // - status_width = 8
    static const unsigned status_width = 8;
    
    // Not supported
    static const bool xk_index = false;
    static const bool cyclic_prefix_insertion = false;
    static const bool memory_options_hybrid = false;
    static const bool use_native_float = false;
};

// ============================================================================
// FFT Helper Functions (v16b: Block Floating Point Mode)
// ============================================================================

/**
 * 1D HLS FFT IP call (128-point, v16b: Block Floating Point Mode)
 * 
 * Block floating point mode:
 * - NO per-stage truncation (unlike scaled mode)
 * - blk_exp tracks total scaling applied
 * - Output needs compensation: output * 2^(blk_exp)
 * 
 * For IFFT (is_inverse=true):
 * - HLS FFT performs unnormalized IDFT
 * - blk_exp indicates how many bits were shifted right to prevent overflow
 * - Compensation: output * 2^(blk_exp) to restore correct magnitude
 * 
 * CRITICAL: HLS FFT library bug in stream-based wrapper
 * - Stream-based FFT calls setSch() even when scale_sch = -1
 * - setSch() checks scaling_opt == scaled, fails for block_floating_point
 * - Solution: Use array-based FFT interface with manual config setup
 */
inline void fft_1d_hls_128_v16(
    hls::stream<cmpx_fft_in_t>& in_stream,
    hls::stream<cmpx_fft_out_t>& out_stream,
    bool is_inverse,
    unsigned* blk_exp
) {
    #pragma HLS interface ap_fifo depth=128 port=in_stream,out_stream
    #pragma HLS stream variable=in_stream
    #pragma HLS stream variable=out_stream
    #pragma HLS dataflow
    
    // Internal arrays for array-based FFT interface
    cmpx_fft_in_t xn[128];
    cmpx_fft_out_t xk[128];
    #pragma HLS stream variable=xn depth=8
    #pragma HLS stream variable=xk depth=8
    
    // Read input stream to array
    for (int i = 0; i < 128; i++) {
        #pragma HLS pipeline II=1
        xn[i] = in_stream.read();
    }
    
    // Setup config manually (avoid setSch for block_float mode)
    hls::ip_fft::config_t<config_socs_fft_v16> config;
    hls::ip_fft::status_t<config_socs_fft_v16> status;
    config.setDir(is_inverse ? 0 : 1);  // 0=IFFT, 1=FFT
    
    // Call array-based FFT (no setSch called)
    hls::fft<config_socs_fft_v16>(xn, xk, &status, &config);
    
    // Extract blk_exp from status
    *blk_exp = status.getBlkExp();
    
    // Write output array to stream
    for (int i = 0; i < 128; i++) {
        #pragma HLS pipeline II=1
        out_stream.write(xk[i]);
    }
}

/**
 * 2D HLS FFT using stream interface (128×128, v16b: Block Floating Point Mode)
 * 
 * Block floating point mode:
 * - Each 1D FFT returns blk_exp (number of bits shifted right)
 * - Total blk_exp = sum of all 1D FFT blk_exp values
 * - Compensation: output * 2^(total_blk_exp)
 */
inline void fft_2d_hls_128_v16(
    cmpx_fft_in_t input[128][128],
    cmpx_fft_out_t output[128][128],
    bool is_inverse,
    unsigned* total_blk_exp
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
    
    // Initialize total_blk_exp
    *total_blk_exp = 0;
    
    // Stage 1: Row FFT
    for (int row = 0; row < 128; row++) {
        for (int col = 0; col < 128; col++) {
            #pragma HLS PIPELINE II=1
            row_stream.write(input[row][col]);
        }
        
        unsigned row_blk_exp;
        fft_1d_hls_128_v16(row_stream, row_fft_stream, is_inverse, &row_blk_exp);
        *total_blk_exp += row_blk_exp;
        
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
        
        unsigned col_blk_exp;
        fft_1d_hls_128_v16(col_stream, col_fft_stream, is_inverse, &col_blk_exp);
        *total_blk_exp += col_blk_exp;
        
        for (int row = 0; row < 128; row++) {
            #pragma HLS PIPELINE II=1
            output[row][col] = col_fft_stream.read();
        }
    }
}

#endif // HLS_FFT_CONFIG_2048_V16_H
