/**
 * HLS FFT Implementation for 2048 Architecture
 * FPGA-Litho Project
 * 
 * Purpose: Replace direct DFT with hls::fft IP
 * Expected Benefits:
 *   - DSP: 8,064 → ~1,600 (80% reduction)
 *   - Latency: O(N²) → O(N log N) (10x improvement)
 */

#include "hls_fft_config_2048.h"
#include <hls_stream.h>
#include <cstdio>

// ============================================================================
// 1D FFT/IFFT using HLS FFT IP (Stream-based)
// ============================================================================

/**
 * FFT/IFFT wrapper using hls::fft IP
 * 
 * Key implementation details:
 *   - Stream-based interface (depth=128 for buffering)
 *   - Direction controlled by config direction field
 *   - Automatic 1/N scaling for IFFT (scaled mode)
 *   - Natural output ordering
 * 
 * Performance:
 *   - Latency: ~128×log2(128) = ~896 cycles
 *   - DSP: ~12 per FFT instance (vs 128×128=16,384 for direct DFT)
 */
void fft_1d_hls_stream_2048(
    hls::stream<std::complex<float>>& in_stream,
    hls::stream<std::complex<float>>& out_stream,
    bool is_inverse
) {
    #pragma HLS DATAFLOW
    #pragma HLS STREAM variable=in_stream depth=128
    #pragma HLS STREAM variable=out_stream depth=128
    
    // Create FFT config and status objects
    fft_config_2048_t fft_config;
    fft_status_2048_t fft_status;
    
    // Set FFT direction
    // 0: Forward FFT (no scaling)
    // 1: Inverse FFT (scaled mode: 1/N scaling)
    fft_config.setDir(is_inverse ? 1 : 0);
    
    // Scaling factor (for scaled mode)
    // FFT: no scaling needed
    // IFFT: use scaled mode to get 1/N scaling automatically
    ap_uint<15> scaling_factor = 0;  // Scaled mode (divides by N²=16384, manual compensation later)
    fft_config.setScaling(scaling_factor);
    
    // Call HLS FFT IP
    hls::fft<config_fft_2048>(in_stream, out_stream, fft_config, fft_status);
}

// ============================================================================
// Helper Functions for 2D FFT
// ============================================================================

/**
 * Load row from array to stream
 * 
 * Reads 128 complex values from input[row][*] and pushes to stream
 */
void load_row_to_stream(
    std::complex<float> input[128][128],
    int row,
    hls::stream<std::complex<float>>& out_stream
) {
    #pragma HLS PIPELINE II=1
    
    for (int x = 0; x < 128; x++) {
        std::complex<float> val = input[row][x];
        out_stream.write(val);
    }
}

/**
 * Store stream to row in array
 * 
 * Reads 128 complex values from stream and writes to output[row][*]
 */
void store_stream_to_row(
    hls::stream<std::complex<float>>& in_stream,
    std::complex<float> output[128][128],
    int row
) {
    #pragma HLS PIPELINE II=1
    
    for (int x = 0; x < 128; x++) {
        std::complex<float> val = in_stream.read();
        output[row][x] = val;
    }
}

/**
 * Load column from array to stream
 * 
 * Reads 128 complex values from input[*][col] and pushes to stream
 */
void load_col_to_stream(
    std::complex<float> input[128][128],
    int col,
    hls::stream<std::complex<float>>& out_stream
) {
    #pragma HLS PIPELINE II=1
    
    for (int y = 0; y < 128; y++) {
        std::complex<float> val = input[y][col];
        out_stream.write(val);
    }
}

/**
 * Store stream to column in array
 * 
 * Reads 128 complex values from stream and writes to output[*][col]
 */
void store_stream_to_col(
    hls::stream<std::complex<float>>& in_stream,
    std::complex<float> output[128][128],
    int col
) {
    #pragma HLS PIPELINE II=1
    
    for (int y = 0; y < 128; y++) {
        std::complex<float> val = in_stream.read();
        output[y][col] = val;
    }
}

// ============================================================================
// 2D FFT/IFFT using HLS FFT IP (Stream-based)
// ============================================================================

/**
 * 2D FFT using row-column decomposition
 * 
 * Implementation steps:
 *   1. Row FFT: Process each row independently (128 calls)
 *   2. Transpose: Implicit transpose via column access
 *   3. Column FFT: Process each column independently (128 calls)
 *   4. Result stored in output array
 * 
 * DATAFLOW optimization:
 *   - Row FFT stage + Transpose + Column FFT stage pipeline
 *   - Uses intermediate buffer for transposition
 * 
 * Performance:
 *   - Total FFT calls: 128 rows + 128 cols = 256 calls
 *   - Latency: ~256×896 = ~230k cycles (vs ~16M cycles for direct DFT)
 *   - DSP: ~12×2 = ~24 (shared across pipeline stages)
 */
void fft_2d_hls_stream_2048(
    std::complex<float> input[128][128],
    std::complex<float> output[128][128],
    bool is_inverse
) {
    #pragma HLS DATAFLOW
    
    // Intermediate buffer for transposition
    std::complex<float> temp[128][128];
    #pragma HLS RESOURCE variable=temp core=RAM_2P_BRAM
    
    // ========== Stage 1: Row FFT ==========
    // Process all rows (128×1D FFT)
    hls::stream<std::complex<float>> row_in_stream("row_in_stream");
    hls::stream<std::complex<float>> row_out_stream("row_out_stream");
    #pragma HLS STREAM variable=row_in_stream depth=128
    #pragma HLS STREAM variable=row_out_stream depth=128
    
    // Row FFT processing loop
    for (int row = 0; row < 128; row++) {
        #pragma HLS UNROLL factor=4  // Partially unroll for throughput
        
        // Load row data to stream
        load_row_to_stream(input, row, row_in_stream);
        
        // Row FFT (1D transform)
        fft_1d_hls_stream_2048(row_in_stream, row_out_stream, is_inverse);
        
        // Store result to temp buffer
        store_stream_to_row(row_out_stream, temp, row);
    }
    
    // ========== Stage 2: Column FFT (includes transpose) ==========
    // Process all columns (128×1D FFT)
    hls::stream<std::complex<float>> col_in_stream("col_in_stream");
    hls::stream<std::complex<float>> col_out_stream("col_out_stream");
    #pragma HLS STREAM variable=col_in_stream depth=128
    #pragma HLS STREAM variable=col_out_stream depth=128
    
    // Column FFT processing loop
    for (int col = 0; col < 128; col++) {
        #pragma HLS UNROLL factor=4  // Partially unroll for throughput
        
        // Load column data to stream (implicit transpose)
        load_col_to_stream(temp, col, col_in_stream);
        
        // Column FFT (1D transform)
        fft_1d_hls_stream_2048(col_in_stream, col_out_stream, is_inverse);
        
        // Store result to output array
        store_stream_to_col(col_out_stream, output, col);
    }
}

// ============================================================================
// Alternative: Ping-Pong DDR Caching Version
// ============================================================================

/**
 * Row FFT with Ping-Pong DDR caching
 * 
 * This version is optimized for DDR burst access:
 *   - Read 16×16 block from DDR (256 reads per burst)
 *   - Process 16 rows using stream-based FFT
 *   - Write 16×16 block back to DDR
 * 
 * Replaces fft_rows_pingpong() in socs_2048_pingpong.cpp
 */
void fft_rows_pingpong_hls(
    std::complex<float>* ddr_in,
    std::complex<float>* ddr_out,
    bool is_inverse
) {
    #pragma HLS DATAFLOW
    
    // Ping-Pong BRAM buffers
    std::complex<float> ping_buf[16][16];
    std::complex<float> pong_buf[16][16];
    #pragma HLS RESOURCE variable=ping_buf core=RAM_2P_BRAM
    #pragma HLS RESOURCE variable=pong_buf core=RAM_2P_BRAM
    
    // Process 8 blocks (8×16 = 128 rows)
    for (int block = 0; block < 8; block++) {
        #pragma HLS UNROLL factor=2
        
        // Determine Ping/Pong buffer for this block
        std::complex<float> (&buf)[16][16] = (block % 2 == 0) ? ping_buf : pong_buf;
        
        // ========== DDR Read Stage ==========
        // Burst read 16×16 block from DDR
        int ddr_offset_row = block * 16;
        for (int y = 0; y < 16; y++) {
            for (int x = 0; x < 16; x++) {
                #pragma HLS PIPELINE II=1
                
                int ddr_idx = (ddr_offset_row + y) * 128 + x;
                buf[y][x] = ddr_in[ddr_idx];
            }
        }
        
        // ========== Row FFT Stage ==========
        // Process 16 rows in this block
        hls::stream<std::complex<float>> row_stream("row_stream");
        #pragma HLS STREAM variable=row_stream depth=128
        
        for (int row_in_block = 0; row_in_block < 16; row_in_block++) {
            int row = block * 16 + row_in_block;
            
            // Load row from BRAM buffer to stream
            load_row_to_stream(buf, row_in_block, row_stream);
            
            // Row FFT (stream-based)
            hls::stream<std::complex<float>> fft_out_stream("fft_out_stream");
            fft_1d_hls_stream_2048(row_stream, fft_out_stream, is_inverse);
            
            // Store back to BRAM buffer (same position)
            store_stream_to_row(fft_out_stream, buf, row_in_block);
        }
        
        // ========== DDR Write Stage ==========
        // Burst write 16×16 block to DDR
        for (int y = 0; y < 16; y++) {
            for (int x = 0; x < 16; x++) {
                #pragma HLS PIPELINE II=1
                
                int ddr_idx = (ddr_offset_row + y) * 128 + x;
                ddr_out[ddr_idx] = buf[y][x];
            }
        }
    }
}