/**
 * SOCS 2048 HLS FFT Integration Wrapper
 * FPGA-Litho Project
 * 
 * Purpose: Replace direct DFT with hls::fft IP
 * This file provides array-to-stream adaptation layer
 */

#include "socs_2048.h"
#include "hls_fft_config_2048.h"

// ============================================================================
// Array-to-Stream Adapter Functions
// ============================================================================

/**
 * Load array row to stream (128 complex values)
 */
void load_array_row_to_stream_2048(
    cmpx_2048_t array[MAX_FFT_Y][MAX_FFT_X],
    int row,
    hls::stream<cmpx_2048_t>& stream
) {
    #pragma HLS PIPELINE II=1
    
    for (int x = 0; x < MAX_FFT_X; x++) {
        stream.write(array[row][x]);
    }
}

/**
 * Store stream to array row (128 complex values)
 */
void store_stream_to_array_row_2048(
    hls::stream<cmpx_2048_t>& stream,
    cmpx_2048_t array[MAX_FFT_Y][MAX_FFT_X],
    int row
) {
    #pragma HLS PIPELINE II=1
    
    for (int x = 0; x < MAX_FFT_X; x++) {
        array[row][x] = stream.read();
    }
}

/**
 * Load array column to stream (128 complex values)
 */
void load_array_col_to_stream_2048(
    cmpx_2048_t array[MAX_FFT_Y][MAX_FFT_X],
    int col,
    hls::stream<cmpx_2048_t>& stream
) {
    #pragma HLS PIPELINE II=1
    
    for (int y = 0; y < MAX_FFT_Y; y++) {
        stream.write(array[y][col]);
    }
}

/**
 * Store stream to array column (128 complex values)
 */
void store_stream_to_array_col_2048(
    hls::stream<cmpx_2048_t>& stream,
    cmpx_2048_t array[MAX_FFT_Y][MAX_FFT_X],
    int col
) {
    #pragma HLS PIPELINE II=1
    
    for (int y = 0; y < MAX_FFT_Y; y++) {
        array[y][col] = stream.read();
    }
}

// ============================================================================
// HLS FFT IP Wrapper (Array Interface)
// ============================================================================

/**
 * 2D FFT/IFFT using HLS FFT IP (Array Interface)
 * 
 * This function provides array interface compatibility with existing code,
 * while internally using stream-based hls::fft IP.
 * 
 * Replaces fft_2d_full_2048() with:
 *   - DSP: ~24 (vs 8,064 for direct DFT)
 *   - Latency: ~230k cycles (vs ~16M cycles)
 *   - Uses DATAFLOW for pipelining
 * 
 * @param input   Input 2D array (128×128)
 * @param output  Output 2D array (128×128)
 * @param is_inverse  true for IFFT, false for FFT
 */
void fft_2d_hls_wrapper_2048(
    cmpx_2048_t input[MAX_FFT_Y][MAX_FFT_X],
    cmpx_2048_t output[MAX_FFT_Y][MAX_FFT_X],
    bool is_inverse
) {
    #pragma HLS DATAFLOW
    
    // Intermediate buffer for transposition
    cmpx_2048_t temp[MAX_FFT_Y][MAX_FFT_X];
    #pragma HLS RESOURCE variable=temp core=RAM_2P_BRAM
    
    // ========== Stage 1: Row FFT ==========
    // Stream for row FFT
    hls::stream<cmpx_2048_t> row_in_stream("row_in_stream");
    hls::stream<cmpx_2048_t> row_out_stream("row_out_stream");
    #pragma HLS STREAM variable=row_in_stream depth=128
    #pragma HLS STREAM variable=row_out_stream depth=128
    
    // Process all rows (128×1D FFT)
    for (int row = 0; row < MAX_FFT_Y; row++) {
        #pragma HLS UNROLL factor=4
        
        // Load row to stream
        load_array_row_to_stream_2048(input, row, row_in_stream);
        
        // Call HLS FFT IP (1D transform)
        fft_1d_hls_stream_2048(row_in_stream, row_out_stream, is_inverse);
        
        // Store stream to temp buffer
        store_stream_to_array_row_2048(row_out_stream, temp, row);
    }
    
    // ========== Stage 2: Column FFT (includes transpose) ==========
    // Stream for column FFT
    hls::stream<cmpx_2048_t> col_in_stream("col_in_stream");
    hls::stream<cmpx_2048_t> col_out_stream("col_out_stream");
    #pragma HLS STREAM variable=col_in_stream depth=128
    #pragma HLS STREAM variable=col_out_stream depth=128
    
    // Process all columns (128×1D FFT)
    for (int col = 0; col < MAX_FFT_X; col++) {
        #pragma HLS UNROLL factor=4
        
        // Load column to stream (implicit transpose)
        load_array_col_to_stream_2048(temp, col, col_in_stream);
        
        // Call HLS FFT IP (1D transform)
        fft_1d_hls_stream_2048(col_in_stream, col_out_stream, is_inverse);
        
        // Store stream to output array
        store_stream_to_array_col_2048(col_out_stream, output, col);
    }
}

// ============================================================================
// Conditional FFT Selection (Simulation vs Synthesis)
// ============================================================================

/**
 * Unified FFT function with automatic selection
 * 
 * Behavior:
 *   - C Simulation: Use direct DFT (for validation against golden)
 *   - C Synthesis: Use HLS FFT IP (for performance optimization)
 * 
 * This enables both validation and optimization without code duplication.
 */
#ifdef __SYNTHESIS__
    // Synthesis mode: Use HLS FFT IP
    #define FFT_2D_2048(input, output, is_inverse) \
        fft_2d_hls_wrapper_2048(input, output, is_inverse)
#else
    // Simulation mode: Use direct DFT
    #define FFT_2D_2048(input, output, is_inverse) \
        fft_2d_direct_2048(input, output, is_inverse)
#endif

// ============================================================================
// Ping-Pong DDR Caching Version (Optional)
// ============================================================================

/**
 * Row FFT with Ping-Pong DDR caching
 * 
 * Optimized for DDR burst access:
 *   - Read 16×16 block from DDR (256 reads per burst)
 *   - Process 16 rows using stream-based FFT
 *   - Write 16×16 block back to DDR
 * 
 * Usage: For calc_socs_2048_hls() DDR caching optimization
 */
void fft_rows_pingpong_hls_wrapper_2048(
    cmpx_2048_t* ddr_in,
    cmpx_2048_t* ddr_out,
    bool is_inverse,
    int fft_width  // MAX_FFT_X = 128
) {
    #pragma HLS DATAFLOW
    
    // Ping-Pong BRAM buffers (2×16×16)
    cmpx_2048_t ping_buf[BLOCK_SIZE][BLOCK_SIZE];
    cmpx_2048_t pong_buf[BLOCK_SIZE][BLOCK_SIZE];
    #pragma HLS RESOURCE variable=ping_buf core=RAM_2P_BRAM
    #pragma HLS RESOURCE variable=pong_buf core=RAM_2P_BRAM
    
    // Process 8 blocks (8×16 = 128 rows)
    for (int block = 0; block < 8; block++) {
        #pragma HLS UNROLL factor=2
        
        // Select Ping/Pong buffer
        cmpx_2048_t (&buf)[BLOCK_SIZE][BLOCK_SIZE] = 
            (block % 2 == 0) ? ping_buf : pong_buf;
        
        // ========== DDR Read Stage ==========
        int ddr_offset_row = block * BLOCK_SIZE;
        for (int y = 0; y < BLOCK_SIZE; y++) {
            for (int x = 0; x < BLOCK_SIZE; x++) {
                #pragma HLS PIPELINE II=1
                
                int ddr_idx = (ddr_offset_row + y) * fft_width + x;
                buf[y][x] = ddr_in[ddr_idx];
            }
        }
        
        // ========== Row FFT Stage ==========
        // Stream for this block's rows
        hls::stream<cmpx_2048_t> row_stream("row_stream");
        hls::stream<cmpx_2048_t> fft_out_stream("fft_out_stream");
        #pragma HLS STREAM variable=row_stream depth=128
        #pragma HLS STREAM variable=fft_out_stream depth=128
        
        // Process 16 rows in this block
        for (int row_in_block = 0; row_in_block < BLOCK_SIZE; row_in_block++) {
            // Load row from BRAM to stream
            load_array_row_to_stream_2048(buf, row_in_block, row_stream);
            
            // Row FFT
            fft_1d_hls_stream_2048(row_stream, fft_out_stream, is_inverse);
            
            // Store back to BRAM
            store_stream_to_array_row_2048(fft_out_stream, buf, row_in_block);
        }
        
        // ========== DDR Write Stage ==========
        for (int y = 0; y < BLOCK_SIZE; y++) {
            for (int x = 0; x < BLOCK_SIZE; x++) {
                #pragma HLS PIPELINE II=1
                
                int ddr_idx = (ddr_offset_row + y) * fft_width + x;
                ddr_out[ddr_idx] = buf[y][x];
            }
        }
    }
}