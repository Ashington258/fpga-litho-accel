/**
 * SOCS HLS 2048 Architecture Header
 * FPGA-Litho Project
 * 
 * Key Features:
 *   - DDR-based FFT buffer (Ping-Pong caching)
 *   - Runtime configurable Nx (2~24)
 *   - Fixed 128×128 FFT (max support)
 *   - Zero-padding strategy for small Nx
 */

#ifndef SOCS_2048_H
#define SOCS_2048_H

// Conditional compilation for HLS vs standard C++
#ifdef __SYNTHESIS__
#include <hls_stream.h>
#include <ap_fixed.h>
#else
// Standard C++ headers for simulation
#include <iostream>
#include <fstream>
#include <vector>
#endif

#include <cmath>
#include <complex>

// ============================================================================
// 2048 Architecture Constants
// ============================================================================

// Maximum FFT size (fixed for all Nx configurations)
#define MAX_FFT_X  128
#define MAX_FFT_Y  128
#define MAX_FFT_LOG2  7  // log2(128) = 7

// Ping-Pong buffer block size
#define BLOCK_SIZE  16  // 16×16 block for DDR caching

// DDR buffer offsets (Host must allocate 512KB space)
#define DDR_TEMP1_OFFSET  0x00000000  // temp1: 128×128×8B = 128KB
#define DDR_TEMP2_OFFSET  0x00020000  // temp2: +128KB offset
#define DDR_TEMP3_OFFSET  0x00040000  // temp3: +256KB offset
#define DDR_BUF_SIZE      0x00080000  // Total: 512KB

// Maximum kernel size (Nx=24 → kerX=49)
#define MAX_KER_X  49
#define MAX_KER_Y  49

// Maximum convolution output size (Nx=24 → convX=97)
#define MAX_CONV_X  97
#define MAX_CONV_Y  97

// ============================================================================
// Data Types
// ============================================================================

typedef float data_2048_t;
typedef std::complex<float> cmpx_2048_t;

// ============================================================================
// Function Declarations
// ============================================================================

// ----------------- DDR Burst Transfer Functions -----------------

/**
 * Burst read block from DDR to BRAM buffer (256 reads)
 * 
 * @param ddr_base      DDR base address
 * @param ddr_offset    Offset within DDR buffer
 * @param bram_buf      BRAM target buffer (BLOCK_SIZE×BLOCK_SIZE)
 * @param fft_width     Total FFT width (128)
 * @param block_y       Block row index (0~7)
 * @param block_x       Block column index (0~7)
 */
void burst_read_block(
    cmpx_2048_t* ddr_base,
    unsigned long ddr_offset,
    cmpx_2048_t bram_buf[BLOCK_SIZE][BLOCK_SIZE],
    int fft_width,
    int block_y, int block_x
);

/**
 * Burst write block from BRAM buffer to DDR (256 writes)
 */
void burst_write_block(
    cmpx_2048_t bram_buf[BLOCK_SIZE][BLOCK_SIZE],
    cmpx_2048_t* ddr_base,
    unsigned long ddr_offset,
    int fft_width,
    int block_y, int block_x
);

// ----------------- Ping-Pong FFT Processing -----------------

/**
 * Row FFT processing with Ping-Pong DDR caching
 * 
 * Data flow: DDR temp1 → (row FFT) → DDR temp2
 * Processing: 8 blocks × 16 rows/block = 128 rows
 * 
 * @param ddr_in          Input DDR buffer
 * @param ddr_out         Output DDR buffer
 * @param ddr_in_offset   Input offset (DDR_TEMP1_OFFSET)
 * @param ddr_out_offset  Output offset (DDR_TEMP2_OFFSET)
 * @param is_inverse      FFT direction (true=IFFT)
 */
void fft_rows_pingpong(
    cmpx_2048_t* ddr_in,
    cmpx_2048_t* ddr_out,
    unsigned long ddr_in_offset,
    unsigned long ddr_out_offset,
    bool is_inverse
);

/**
 * Column FFT processing with Ping-Pong DDR caching
 * 
 * Data flow: DDR temp2 → (col FFT) → DDR temp3
 * Processing: 8 blocks × 16 cols/block = 128 columns
 */
void fft_cols_pingpong(
    cmpx_2048_t* ddr_in,
    cmpx_2048_t* ddr_out,
    unsigned long ddr_in_offset,
    unsigned long ddr_out_offset,
    bool is_inverse
);

/**
 * Full 2D FFT with DDR Ping-Pong buffering
 * 
 * Memory flow:
 *   1. Write fft_input to DDR temp1
 *   2. Row FFT: DDR temp1 → temp2
 *   3. Column FFT: DDR temp2 → temp3
 *   4. Read DDR temp3 to fft_output
 * 
 * @param fft_buf_ddr    DDR buffer base address (gmem6)
 * @param fft_buf_base   DDR physical base offset (AXI-Lite)
 * @param fft_input      Input array (128×128)
 * @param fft_output     Output array (128×128)
 * @param is_inverse     FFT direction
 */
void fft_2d_ddr_pingpong(
    cmpx_2048_t* fft_buf_ddr,
    unsigned long fft_buf_base,
    cmpx_2048_t fft_input[MAX_FFT_Y][MAX_FFT_X],
    cmpx_2048_t fft_output[MAX_FFT_Y][MAX_FFT_X],
    bool is_inverse
);

/**
 * 2D FFT using BRAM-only (no DDR caching)
 * 
 * For C simulation baseline and comparison
 */
void fft_2d_bram_only(
    cmpx_2048_t input[MAX_FFT_Y][MAX_FFT_X],
    cmpx_2048_t output[MAX_FFT_Y][MAX_FFT_X],
    bool is_inverse
);

// ----------------- Direct DFT Functions (C Simulation) -----------------

/**
 * 1D Direct DFT implementation (for C simulation)
 * 
 * FFTW BACKWARD convention: IFFT applies 1/N scaling
 * 
 * @param input    Input array (128 elements)
 * @param output   Output array (128 elements)
 * @param is_inverse  true=IFFT, false=FFT
 */
void fft_1d_direct_2048(
    cmpx_2048_t input[MAX_FFT_X],
    cmpx_2048_t output[MAX_FFT_X],
    bool is_inverse
);

/**
 * 2D Direct DFT implementation (for C simulation)
 * 
 * @param input    Input 2D array (128×128)
 * @param output   Output 2D array (128×128)
 * @param is_inverse  true=IFFT, false=FFT
 */
void fft_2d_full_2048(
    cmpx_2048_t input[MAX_FFT_Y][MAX_FFT_X],
    cmpx_2048_t output[MAX_FFT_Y][MAX_FFT_X],
    bool is_inverse
);

/**
 * 2D FFT shift: move zero-frequency component to center
 * 
 * @param input    Input 2D array (128×128)
 * @param output   Output 2D array (128×128)
 */
void fftshift_2d_2048(
    float input[MAX_FFT_Y][MAX_FFT_X],
    float output[MAX_FFT_Y][MAX_FFT_X]
);

// ----------------- Legacy Functions (Compatibility) -----------------

/**
 * Burst read from DDR to BRAM cache (legacy interface)
 */
void burst_read_ddr_2048(
    cmpx_2048_t* ddr_addr,
    cmpx_2048_t bram_buf[BLOCK_SIZE][BLOCK_SIZE],
    int block_y, int block_x,
    int fft_width
);

/**
 * Burst write from BRAM cache to DDR (legacy interface)
 */
void burst_write_ddr_2048(
    cmpx_2048_t bram_buf[BLOCK_SIZE][BLOCK_SIZE],
    cmpx_2048_t* ddr_addr,
    int block_y, int block_x,
    int fft_width
);

/**
 * Embed kernel × mask product with zero-padding to 128×128
 * 
 * @param nx_actual   Runtime Nx parameter (2~24)
 * @param kernel_idx  Current kernel index (0 ~ nk-1)
 */
void embed_kernel_mask_padded_2048(
    float* mskf_r, float* mskf_i,
    float* krn_r, float* krn_i,
    cmpx_2048_t fft_input[MAX_FFT_Y][MAX_FFT_X],
    int nx_actual, int ny_actual,
    int kernel_idx,
    int Lx, int Ly
);

/**
 * Extract valid output region based on actual Nx
 * 
 * @param nx_actual   Runtime Nx (determines output size: 4*Nx+1)
 */
void extract_valid_region_2048(
    float ifft_output[MAX_FFT_Y][MAX_FFT_X],
    float* output_ddr,
    int nx_actual, int ny_actual
);

/**
 * 2D FFT with DDR caching (Ping-Pong buffer)
 * 
 * Data flow:
 *   DDR temp1 → Ping buf → FFT rows → Pong buf → DDR temp2
 *   DDR temp2 → Ping buf → Transpose → Pong buf → DDR temp3
 *   DDR temp3 → Ping buf → FFT cols → Output
 */
void fft_2d_ddr_cached_2048(
    cmpx_2048_t* fft_buf_ddr,    // DDR buffer base address
    unsigned long fft_buf_base,  // DDR physical address offset
    cmpx_2048_t fft_input[MAX_FFT_Y][MAX_FFT_X],
    cmpx_2048_t fft_output[MAX_FFT_Y][MAX_FFT_X],
    bool is_inverse
);

/**
 * Main SOCS 2048 calculation function
 * 
 * AXI Interfaces:
 *   - gmem0-5: Data inputs/outputs (same as v1.0)
 *   - gmem6: FFT DDR buffer (NEW)
 *   - AXI-Lite: nx_actual, ny_actual, fft_buf_base (NEW)
 */
void calc_socs_2048_hls(
    // Data AXI-MM interfaces (gmem0-5)
    float* mskf_r,      // Mask spectrum real (512×512)
    float* mskf_i,      // Mask spectrum imaginary
    float* scales,      // Eigenvalues (nk)
    float* krn_r,       // SOCS kernels real (nk×MAX_KER_X×MAX_KER_Y)
    float* krn_i,       // SOCS kernels imaginary
    float* output,      // Output intensity (MAX_CONV_X×MAX_CONV_Y)
    
    // FFT DDR buffer interface (gmem6 - NEW)
    cmpx_2048_t* fft_buf_ddr,  // DDR cache for FFT intermediates
    
    // AXI-Lite runtime parameters (NEW)
    int nx_actual,      // Runtime Nx (2~24)
    int ny_actual,      // Runtime Ny
    int nk,             // Number of kernels (runtime)
    int Lx,             // Mask width (512)
    int Ly,             // Mask height
    unsigned long fft_buf_base,  // DDR buffer physical base address
    
    // Output mode selection (Phase 1.4+)
    int output_mode     // 0=center 33×33 (default), 1=full 128×128 (for FI)
);

#endif // SOCS_2048_H