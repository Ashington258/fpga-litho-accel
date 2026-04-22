/**
 * HLS FFT Implementation (Corrected for Vitis 2025.2)
 * FPGA-Litho Project
 * 
 * Purpose: Fixed-point FFT implementation matching Vitis HLS requirements
 * Reference: reference/vitis_hls_ftt的实现/interface_stream/fft_top.cpp
 */

#ifdef __SYNTHESIS__

#include "hls_fft_config_2048_final.h"  // Use exact reference pattern
#include "socs_2048.h"
#include <hls_stream.h>

// ============================================================================
// Type Conversion Functions (float ↔ ap_fixed)
// ============================================================================

/**
 * Convert float complex to ap_fixed complex (for HLS FFT input)
 */
inline cmpx_hls_2048_t float_to_fixed_2048(const cmpx_2048_t& input) {
    // Convert float to ap_fixed<27,1> (1 integer bit, 26 fractional bits)
    data_in_2048_t real_fixed = (data_in_2048_t)input.real();
    data_in_2048_t imag_fixed = (data_in_2048_t)input.imag();
    return cmpx_hls_2048_t(real_fixed, imag_fixed);
}

/**
 * Convert ap_fixed complex to float complex (for HLS FFT output)
 */
inline cmpx_2048_t fixed_to_float_2048(const cmpx_hls_out_2048_t& output) {
    // Convert ap_fixed<27,20> back to float
    float real_float = (float)output.real();
    float imag_float = (float)output.imag();
    return cmpx_2048_t(real_float, imag_float);
}

// ============================================================================
// 1D FFT Implementation (Corrected Vitis 2025.2 Interface)
// ============================================================================

/**
 * 1D FFT using HLS FFT IP (Stream-based, ap_fixed)
 * 
 * CRITICAL: Must use ap_fixed types, not float!
 * Interface: hls::fft<config>(in_stream, out_stream, dir, scaling, nfft, status)
 * 
 * Parameters:
 *   - dir: 0=Forward, 1=Inverse
 *   - scaling: 0=Scaled mode (matches FFTW BACKWARD 1/N scaling)
 *   - nfft: -1 for fixed-length FFT
 *   - status: Output status flag
 */
void fft_1d_hls_fixed_2048(
    hls::stream<cmpx_hls_2048_t>& in_stream,
    hls::stream<cmpx_hls_out_2048_t>& out_stream,
    bool is_inverse
) {
#pragma HLS interface ap_fifo depth=128 port=in_stream,out_stream
#pragma HLS stream variable=in_stream
#pragma HLS stream variable=out_stream
#pragma HLS dataflow
    
    // FFT parameters (Vitis 2025.2 format - matching reference example)
    ap_uint<1> dir = is_inverse ? 1 : 0;  // 0=Forward, 1=Inverse
    ap_uint<15> scaling = 0;  // 0=Scaled mode (BFP scaling)
    bool status;  // Output status flag
    
    // Call HLS FFT IP (corrected interface - matching reference)
    // Parameters: in_stream, out_stream, dir, scaling, nfft(-1), status
    hls::fft<config_fft_2048>(in_stream, out_stream, dir, scaling, -1, &status);
}

// ============================================================================
// Stream Adapter Functions (Array ↔ Stream with Type Conversion)
// ============================================================================

/**
 * Load float array row to fixed-point stream (adapter)
 */
void load_row_to_fixed_stream_2048(
    cmpx_2048_t array[MAX_FFT_Y][MAX_FFT_X],
    int row,
    hls::stream<cmpx_hls_2048_t>& stream
) {
    #pragma HLS PIPELINE II=1
    for (int x = 0; x < MAX_FFT_X; x++) {
        cmpx_hls_2048_t fixed_val = float_to_fixed_2048(array[row][x]);
        stream.write(fixed_val);
    }
}

/**
 * Store fixed-point stream to float array row (adapter)
 */
void store_fixed_stream_to_row_2048(
    hls::stream<cmpx_hls_out_2048_t>& stream,
    cmpx_2048_t array[MAX_FFT_Y][MAX_FFT_X],
    int row
) {
    #pragma HLS PIPELINE II=1
    for (int x = 0; x < MAX_FFT_X; x++) {
        cmpx_hls_out_2048_t fixed_val = stream.read();
        array[row][x] = fixed_to_float_2048(fixed_val);
    }
}

/**
 * Load float array column to fixed-point stream (adapter)
 */
void load_col_to_fixed_stream_2048(
    cmpx_2048_t array[MAX_FFT_Y][MAX_FFT_X],
    int col,
    hls::stream<cmpx_hls_2048_t>& stream
) {
    #pragma HLS PIPELINE II=1
    for (int y = 0; y < MAX_FFT_Y; y++) {
        cmpx_hls_2048_t fixed_val = float_to_fixed_2048(array[y][col]);
        stream.write(fixed_val);
    }
}

/**
 * Store fixed-point stream to float array column (adapter)
 */
void store_fixed_stream_to_col_2048(
    hls::stream<cmpx_hls_out_2048_t>& stream,
    cmpx_2048_t array[MAX_FFT_Y][MAX_FFT_X],
    int col
) {
    #pragma HLS PIPELINE II=1
    for (int y = 0; y < MAX_FFT_Y; y++) {
        cmpx_hls_out_2048_t fixed_val = stream.read();
        array[y][col] = fixed_to_float_2048(fixed_val);
    }
}

// ============================================================================
// 2D FFT Implementation (Corrected HLS FFT IP)
// ============================================================================

/**
 * 2D FFT using HLS FFT IP (Stream-based, ap_fixed)
 * 
 * Replaces direct DFT with HLS FFT IP:
 *   - DSP: ~24 (vs 8,064 direct DFT)
 *   - Latency: ~230k cycles (vs ~16M cycles)
 *   - Uses ap_fixed<27,1> for precision matching float
 */
void fft_2d_full_2048(
    cmpx_2048_t input[MAX_FFT_Y][MAX_FFT_X],
    cmpx_2048_t output[MAX_FFT_Y][MAX_FFT_X],
    bool is_inverse
) {
    #pragma HLS DATAFLOW
    
    // Intermediate buffer for transpose
    cmpx_2048_t temp[MAX_FFT_Y][MAX_FFT_X];
    #pragma HLS bind_storage variable=temp type=RAM_2P impl=BRAM
    
    // Stage 1: Row FFT (float → fixed-point → FFT → fixed-point → float)
    hls::stream<cmpx_hls_2048_t> row_in_stream("row_in");
    hls::stream<cmpx_hls_out_2048_t> row_out_stream("row_out");
    #pragma HLS STREAM variable=row_in_stream depth=128
    #pragma HLS STREAM variable=row_out_stream depth=128
    
    for (int row = 0; row < MAX_FFT_Y; row++) {
        #pragma HLS UNROLL factor=4
        
        // Convert float to fixed-point and run FFT
        load_row_to_fixed_stream_2048(input, row, row_in_stream);
        fft_1d_hls_fixed_2048(row_in_stream, row_out_stream, is_inverse);
        store_fixed_stream_to_row_2048(row_out_stream, temp, row);
    }
    
    // Stage 2: Column FFT (implicit transpose)
    hls::stream<cmpx_hls_2048_t> col_in_stream("col_in");
    hls::stream<cmpx_hls_out_2048_t> col_out_stream("col_out");
    #pragma HLS STREAM variable=col_in_stream depth=128
    #pragma HLS STREAM variable=col_out_stream depth=128
    
    for (int col = 0; col < MAX_FFT_X; col++) {
        #pragma HLS UNROLL factor=4
        
        // Convert float to fixed-point and run FFT
        load_col_to_fixed_stream_2048(temp, col, col_in_stream);
        fft_1d_hls_fixed_2048(col_in_stream, col_out_stream, is_inverse);
        store_fixed_stream_to_col_2048(col_out_stream, output, col);
    }
}

#endif // __SYNTHESIS__