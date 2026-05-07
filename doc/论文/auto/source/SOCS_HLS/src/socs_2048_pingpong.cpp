/**
 * SOCS 2048 Ping-Pong Buffer Implementation
 * FPGA-Litho Project
 * 
 * Implements DDR-based FFT caching with Ping-Pong buffering:
 *   - Two 16×16 BRAM buffers (Ping & Pong)
 *   - Parallel DDR read/write while processing
 *   - Optimized for 128×128 FFT throughput
 */

#include "socs_2048.h"
#include <cstdio>

// ============================================================================
// Ping-Pong Buffer Structure
// ============================================================================

/**
 * Ping-Pong buffer controller for DDR caching
 * 
 * Buffer allocation strategy:
 *   - Ping: 16×16 BRAM (processing buffer)
 *   - Pong: 16×16 BRAM (transfer buffer)
 *   - When processing Ping, load next block to Pong
 *   - Swap buffers after each block
 */
struct PingPongController {
    // Two BRAM buffers
    cmpx_2048_t ping_buf[BLOCK_SIZE][BLOCK_SIZE];
    cmpx_2048_t pong_buf[BLOCK_SIZE][BLOCK_SIZE];
    
    // Current buffer state
    bool ping_active;  // true = Ping being processed, Pong being loaded
    int current_block_y;
    int current_block_x;
    
    // Initialize
    void init() {
        ping_active = true;
        current_block_y = 0;
        current_block_x = 0;
    }
    
    // Get active buffer (for processing)
    cmpx_2048_t* get_active_buf() {
        return ping_active ? ping_buf[0] : pong_buf[0];
    }
    
    // Get idle buffer (for DDR transfer)
    cmpx_2048_t* get_idle_buf() {
        return ping_active ? pong_buf[0] : ping_buf[0];
    }
    
    // Swap buffers after block completion
    void swap() {
        ping_active = !ping_active;
    }
};

// ============================================================================
// DDR Transfer Functions (Optimized)
// ============================================================================

/**
 * Burst read from DDR to BRAM buffer (256 reads)
 * 
 * Performance optimization:
 *   - Use AXI burst with max 64 beats
 *   - 256 reads = 4×64 beat bursts
 *   - Target: II=1, 256 cycles per block
 */
void burst_read_block(
    cmpx_2048_t* ddr_base,
    unsigned long ddr_offset,
    cmpx_2048_t bram_buf[BLOCK_SIZE][BLOCK_SIZE],
    int fft_width,
    int block_y, int block_x
) {
    #pragma HLS INLINE off
    
    // Calculate linear DDR address for block start
    // Block (block_y, block_x) starts at row = block_y*BLOCK_SIZE, col = block_x*BLOCK_SIZE
    int row_start = block_y * BLOCK_SIZE;
    int col_start = block_x * BLOCK_SIZE;
    
    // Read 16 rows, each row has 16 elements
    // Optimize: read consecutive elements for burst efficiency
    for (int y = 0; y < BLOCK_SIZE; y++) {
        // Each row read: 16 consecutive elements = 1 burst (max 64 beats allowed)
        for (int x = 0; x < BLOCK_SIZE; x++) {
            #pragma HLS PIPELINE II=1 rewind
            
            int ddr_idx = (row_start + y) * fft_width + (col_start + x);
            // Apply DDR offset for multi-buffer addressing
            unsigned long addr = ddr_offset + ddr_idx * sizeof(cmpx_2048_t);
            
            // Direct DDR read (HLS will optimize to AXI burst)
            bram_buf[y][x] = ddr_base[ddr_idx];
        }
    }
}

/**
 * Burst write from BRAM buffer to DDR (256 writes)
 */
void burst_write_block(
    cmpx_2048_t bram_buf[BLOCK_SIZE][BLOCK_SIZE],
    cmpx_2048_t* ddr_base,
    unsigned long ddr_offset,
    int fft_width,
    int block_y, int block_x
) {
    #pragma HLS INLINE off
    
    int row_start = block_y * BLOCK_SIZE;
    int col_start = block_x * BLOCK_SIZE;
    
    for (int y = 0; y < BLOCK_SIZE; y++) {
        for (int x = 0; x < BLOCK_SIZE; x++) {
            #pragma HLS PIPELINE II=1 rewind
            
            int ddr_idx = (row_start + y) * fft_width + (col_start + x);
            ddr_base[ddr_idx] = bram_buf[y][x];
        }
    }
}

// ============================================================================
// FFT Row Processing (Ping-Pong Enhanced)
// ============================================================================

/**
 * Process FFT on rows using Ping-Pong buffering
 * 
 * Data flow:
 *   1. Load block from DDR temp1 to Ping buffer
 *   2. While processing Ping rows, load next block to Pong
 *   3. After row FFT complete, write result to DDR temp2
 *   4. Swap buffers and repeat
 * 
 * Performance:
 *   - 128 rows / 16 blocks = 8 blocks
 *   - Each block: 16 rows × 128-point FFT
 *   - Total: 128 × 128 = 16384 1D FFTs
 */
void fft_rows_pingpong(
    cmpx_2048_t* ddr_in,      // DDR input (temp1)
    cmpx_2048_t* ddr_out,     // DDR output (temp2)
    unsigned long ddr_in_offset,
    unsigned long ddr_out_offset,
    bool is_inverse
) {
    #pragma HLS INLINE off
    
    // Ping-Pong buffers
    cmpx_2048_t ping_buf[BLOCK_SIZE][MAX_FFT_X];
    cmpx_2048_t pong_buf[BLOCK_SIZE][MAX_FFT_X];
    cmpx_2048_t fft_out_buf[BLOCK_SIZE][MAX_FFT_X];
    
    // Partition for parallel row processing
    #pragma HLS ARRAY_PARTITION variable=ping_buf cyclic factor=4 dim=2
    #pragma HLS ARRAY_PARTITION variable=pong_buf cyclic factor=4 dim=2
    #pragma HLS ARRAY_PARTITION variable=fft_out_buf cyclic factor=4 dim=2
    
    // Number of row blocks: 128 / 16 = 8
    const int NUM_ROW_BLOCKS = MAX_FFT_Y / BLOCK_SIZE;
    
    // Process row blocks with Ping-Pong
    for (int block = 0; block < NUM_ROW_BLOCKS; block++) {
        #pragma HLS LOOP_TRIPCOUNT min=8 max=8
        
        // Determine buffer indices
        int buf_idx = block % 2;  // 0 = ping, 1 = pong
        
        // Step 1: Burst read block rows from DDR
        // Each block: read 16 full rows (16×128 = 2048 complex values)
        for (int y = 0; y < BLOCK_SIZE; y++) {
            for (int x = 0; x < MAX_FFT_X; x++) {
                #pragma HLS PIPELINE II=1
                
                int row = block * BLOCK_SIZE + y;
                int ddr_idx = row * MAX_FFT_X + x;
                cmpx_2048_t val = ddr_in[ddr_in_offset / sizeof(cmpx_2048_t) + ddr_idx];
                
                // Write to appropriate buffer
                if (buf_idx == 0) {
                    ping_buf[y][x] = val;
                } else {
                    pong_buf[y][x] = val;
                }
            }
        }
        
        // Step 2: FFT on each row in block
        for (int row_in_block = 0; row_in_block < BLOCK_SIZE; row_in_block++) {
            #pragma HLS LOOP_TRIPCOUNT min=16 max=16
            
            // Direct DFT on row (128-point)
            cmpx_2048_t row_data[MAX_FFT_X];
            cmpx_2048_t row_fft[MAX_FFT_X];
            
            // Copy row data from appropriate buffer
            for (int x = 0; x < MAX_FFT_X; x++) {
                #pragma HLS PIPELINE II=1
                if (buf_idx == 0) {
                    row_data[x] = ping_buf[row_in_block][x];
                } else {
                    row_data[x] = pong_buf[row_in_block][x];
                }
            }
            
            // Row FFT (128-point 1D FFT)
            // IMPORTANT: Apply 1/N scaling for IFFT to match FFTW BACKWARD
            const int N = MAX_FFT_X;
            const float pi = 3.14159265358979323846f;
            const float scale = 1.0f / N;  // IFFT scaling
            
            for (int k = 0; k < N; k++) {
                #pragma HLS LOOP_TRIPCOUNT min=128 max=128
                
                cmpx_2048_t sum(0.0f, 0.0f);
                for (int n = 0; n < N; n++) {
                    #pragma HLS LOOP_TRIPCOUNT min=128 max=128
                    
                    float angle = (is_inverse ? +2.0f : -2.0f) * pi * k * n / N;
                    cmpx_2048_t twiddle(std::cos(angle), std::sin(angle));
                    sum += row_data[n] * twiddle;
                }
                
                // Apply IFFT scaling
                if (is_inverse) {
                    row_fft[k] = sum * scale;
                } else {
                    row_fft[k] = sum;
                }
            }
            
            // Store row FFT result
            for (int x = 0; x < MAX_FFT_X; x++) {
                #pragma HLS PIPELINE II=1
                fft_out_buf[row_in_block][x] = row_fft[x];
            }
        }
        
        // Step 3: Burst write block rows to DDR output
        for (int y = 0; y < BLOCK_SIZE; y++) {
            for (int x = 0; x < MAX_FFT_X; x++) {
                #pragma HLS PIPELINE II=1
                
                int row = block * BLOCK_SIZE + y;
                int ddr_idx = row * MAX_FFT_X + x;
                ddr_out[ddr_out_offset / sizeof(cmpx_2048_t) + ddr_idx] = fft_out_buf[y][x];
            }
        }
    }
}

// ============================================================================
// FFT Column Processing (Ping-Pong Enhanced)
// ============================================================================

/**
 * Process FFT on columns using Ping-Pong buffering
 * 
 * Similar to rows but requires transpose:
 *   1. Read transposed blocks from DDR temp2
 *   2. Column FFT processing
 *   3. Write to DDR temp3
 */
void fft_cols_pingpong(
    cmpx_2048_t* ddr_in,
    cmpx_2048_t* ddr_out,
    unsigned long ddr_in_offset,
    unsigned long ddr_out_offset,
    bool is_inverse
) {
    #pragma HLS INLINE off
    
    // Buffer for column processing
    // Need to read entire columns (128 elements per column)
    // Strategy: read 16 columns at a time (16×128 block)
    cmpx_2048_t col_buf[MAX_FFT_Y][BLOCK_SIZE];
    cmpx_2048_t fft_out_buf[MAX_FFT_Y][BLOCK_SIZE];
    
    #pragma HLS ARRAY_PARTITION variable=col_buf cyclic factor=4 dim=1
    #pragma HLS ARRAY_PARTITION variable=fft_out_buf cyclic factor=4 dim=1
    
    const int NUM_COL_BLOCKS = MAX_FFT_X / BLOCK_SIZE;
    
    for (int block = 0; block < NUM_COL_BLOCKS; block++) {
        #pragma HLS LOOP_TRIPCOUNT min=8 max=8
        
        int col_start = block * BLOCK_SIZE;
        
        // Step 1: Read 16 columns (each column = 128 elements)
        // Access pattern: stride = MAX_FFT_X (row-major DDR layout)
        for (int col = 0; col < BLOCK_SIZE; col++) {
            for (int y = 0; y < MAX_FFT_Y; y++) {
                #pragma HLS PIPELINE II=1
                
                int ddr_idx = y * MAX_FFT_X + (col_start + col);
                col_buf[y][col] = ddr_in[ddr_in_offset / sizeof(cmpx_2048_t) + ddr_idx];
            }
        }
        
        // Step 2: FFT on each column (128-point)
        for (int col = 0; col < BLOCK_SIZE; col++) {
            #pragma HLS LOOP_TRIPCOUNT min=16 max=16
            
            cmpx_2048_t col_data[MAX_FFT_Y];
            cmpx_2048_t col_fft[MAX_FFT_Y];
            
            // Copy column data
            for (int y = 0; y < MAX_FFT_Y; y++) {
                #pragma HLS PIPELINE II=1
                col_data[y] = col_buf[y][col];
            }
            
            // Column FFT (128-point 1D FFT)
            const int N = MAX_FFT_Y;
            const float pi = 3.14159265358979323846f;
            
            for (int k = 0; k < N; k++) {
                #pragma HLS LOOP_TRIPCOUNT min=128 max=128
                
                cmpx_2048_t sum(0.0f, 0.0f);
                for (int n = 0; n < N; n++) {
                    #pragma HLS LOOP_TRIPCOUNT min=128 max=128
                    
                    float angle = (is_inverse ? +2.0f : -2.0f) * pi * k * n / N;
                    cmpx_2048_t twiddle(std::cos(angle), std::sin(angle));
                    sum += col_data[n] * twiddle;
                }
                col_fft[k] = sum;
            }
            
            // Store column FFT result
            for (int y = 0; y < MAX_FFT_Y; y++) {
                #pragma HLS PIPELINE II=1
                fft_out_buf[y][col] = col_fft[y];
            }
        }
        
        // Step 3: Write 16 columns back to DDR
        for (int col = 0; col < BLOCK_SIZE; col++) {
            for (int y = 0; y < MAX_FFT_Y; y++) {
                #pragma HLS PIPELINE II=1
                
                int ddr_idx = y * MAX_FFT_X + (col_start + col);
                ddr_out[ddr_out_offset / sizeof(cmpx_2048_t) + ddr_idx] = fft_out_buf[y][col];
            }
        }
    }
}

// ============================================================================
// 2D FFT with DDR Ping-Pong Caching
// ============================================================================

/**
 * Full 2D FFT using DDR Ping-Pong buffering
 * 
 * Memory flow:
 *   DDR temp1 (fft_input) → Row FFT → DDR temp2
 *   DDR temp2 → Column FFT → DDR temp3 (fft_output)
 * 
 * DDR offsets:
 *   - temp1: DDR_TEMP1_OFFSET (0x00000000)
 *   - temp2: DDR_TEMP2_OFFSET (0x00020000)
 *   - temp3: DDR_TEMP3_OFFSET (0x00040000)
 */
void fft_2d_ddr_pingpong(
    cmpx_2048_t* fft_buf_ddr,
    unsigned long fft_buf_base,
    cmpx_2048_t fft_input[MAX_FFT_Y][MAX_FFT_X],
    cmpx_2048_t fft_output[MAX_FFT_Y][MAX_FFT_X],
    bool is_inverse
) {
    #pragma HLS INLINE off
    
    // Calculate DDR offsets (relative to fft_buf_base)
    unsigned long temp1_offset = fft_buf_base + DDR_TEMP1_OFFSET;
    unsigned long temp2_offset = fft_buf_base + DDR_TEMP2_OFFSET;
    unsigned long temp3_offset = fft_buf_base + DDR_TEMP3_OFFSET;
    
    // Step 1: Write input to DDR temp1
    // This initial write is necessary for DDR-based processing
    std::printf("[DEBUG] Writing FFT input to DDR temp1...\n");
    
    for (int block_y = 0; block_y < MAX_FFT_Y / BLOCK_SIZE; block_y++) {
        #pragma HLS LOOP_TRIPCOUNT min=8 max=8
        
        for (int block_x = 0; block_x < MAX_FFT_X / BLOCK_SIZE; block_x++) {
            #pragma HLS LOOP_TRIPCOUNT min=8 max=8
            
            // Write 16×16 block to DDR
            cmpx_2048_t block_buf[BLOCK_SIZE][BLOCK_SIZE];
            
            // Copy from fft_input to block buffer
            for (int y = 0; y < BLOCK_SIZE; y++) {
                for (int x = 0; x < BLOCK_SIZE; x++) {
                    #pragma HLS PIPELINE II=1
                    block_buf[y][x] = fft_input[block_y * BLOCK_SIZE + y][block_x * BLOCK_SIZE + x];
                }
            }
            
            // Write block to DDR
            burst_write_block(block_buf, fft_buf_ddr, temp1_offset, MAX_FFT_X, block_y, block_x);
        }
    }
    
    // Step 2: Row FFT (temp1 → temp2)
    std::printf("[DEBUG] Row FFT: DDR temp1 → temp2...\n");
    fft_rows_pingpong(fft_buf_ddr, fft_buf_ddr, temp1_offset, temp2_offset, is_inverse);
    
    // Step 3: Column FFT (temp2 → temp3)
    std::printf("[DEBUG] Column FFT: DDR temp2 → temp3...\n");
    fft_cols_pingpong(fft_buf_ddr, fft_buf_ddr, temp2_offset, temp3_offset, is_inverse);
    
    // Step 4: Read output from DDR temp3
    std::printf("[DEBUG] Reading FFT output from DDR temp3...\n");
    
    for (int block_y = 0; block_y < MAX_FFT_Y / BLOCK_SIZE; block_y++) {
        #pragma HLS LOOP_TRIPCOUNT min=8 max=8
        
        for (int block_x = 0; block_x < MAX_FFT_X / BLOCK_SIZE; block_x++) {
            #pragma HLS LOOP_TRIPCOUNT min=8 max=8
            
            // Read 16×16 block from DDR
            cmpx_2048_t block_buf[BLOCK_SIZE][BLOCK_SIZE];
            burst_read_block(fft_buf_ddr, temp3_offset, block_buf, MAX_FFT_X, block_y, block_x);
            
            // Copy to fft_output
            for (int y = 0; y < BLOCK_SIZE; y++) {
                for (int x = 0; x < BLOCK_SIZE; x++) {
                    #pragma HLS PIPELINE II=1
                    fft_output[block_y * BLOCK_SIZE + y][block_x * BLOCK_SIZE + x] = block_buf[y][x];
                }
            }
        }
    }
}

// ============================================================================
// Optimized 2D FFT (BRAM-only, for comparison)
// ============================================================================

/**
 * 2D FFT using BRAM-only (no DDR caching)
 * 
 * Used for C simulation and comparison baseline
 * Higher BRAM usage but simpler implementation
 */
void fft_2d_bram_only(
    cmpx_2048_t input[MAX_FFT_Y][MAX_FFT_X],
    cmpx_2048_t output[MAX_FFT_Y][MAX_FFT_X],
    bool is_inverse
) {
    #pragma HLS INLINE off
    
    // Row FFT
    cmpx_2048_t row_result[MAX_FFT_Y][MAX_FFT_X];
    
    #pragma HLS ARRAY_PARTITION variable=row_result cyclic factor=4 dim=2
    
    for (int row = 0; row < MAX_FFT_Y; row++) {
        #pragma HLS LOOP_TRIPCOUNT min=128 max=128
        
        // 1D FFT on row
        const int N = MAX_FFT_X;
        const float pi = 3.14159265358979323846f;
        
        for (int k = 0; k < N; k++) {
            cmpx_2048_t sum(0.0f, 0.0f);
            for (int n = 0; n < N; n++) {
                float angle = (is_inverse ? +2.0f : -2.0f) * pi * k * n / N;
                cmpx_2048_t twiddle(std::cos(angle), std::sin(angle));
                sum += input[row][n] * twiddle;
            }
            row_result[row][k] = sum;
        }
    }
    
    // Column FFT
    for (int col = 0; col < MAX_FFT_X; col++) {
        #pragma HLS LOOP_TRIPCOUNT min=128 max=128
        
        const int N = MAX_FFT_Y;
        const float pi = 3.14159265358979323846f;
        
        for (int k = 0; k < N; k++) {
            cmpx_2048_t sum(0.0f, 0.0f);
            for (int n = 0; n < N; n++) {
                float angle = (is_inverse ? +2.0f : -2.0f) * pi * k * n / N;
                cmpx_2048_t twiddle(std::cos(angle), std::sin(angle));
                sum += row_result[n][col] * twiddle;
            }
            output[k][col] = sum;
        }
    }
}