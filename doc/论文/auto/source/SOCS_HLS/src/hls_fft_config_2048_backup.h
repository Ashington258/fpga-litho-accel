/**
 * HLS FFT Configuration for 2048 Architecture
 * FPGA-Litho Project
 * 
 * Purpose: Replace direct DFT with hls::fft IP
 * Expected Benefits:
 *   - DSP: 8,064 → ~1,600 (80% reduction)
 *   - Latency: O(N²) → O(N log N) (10x improvement)
 *   - Fmax: 273MHz → 300MHz+
 */

#ifndef HLS_FFT_CONFIG_2048_H
#define HLS_FFT_CONFIG_2048_H

// Conditional compilation for HLS vs standard C++
#ifdef __SYNTHESIS__
#include <ap_fixed.h>
#include <hls_fft.h>

// ============================================================================
// FFT Configuration Parameters (HLS Synthesis Only)
// ============================================================================

// Target: 128-point FFT (MAX_FFT_X = 128)
const char FFT_INPUT_WIDTH_2048     = 32;  // Float: 32 bits
const char FFT_OUTPUT_WIDTH_2048    = 32;
const char FFT_NFFT_MAX_2048        = 7;   // log2(128) = 7
const int  FFT_LENGTH_2048          = 128; // Fixed length
const bool FFT_HAS_NFFT_2048        = 0;   // Not runtime configurable

// Architecture selection
const hls::ip_fft::arch FFT_ARCH_2048 = hls::ip_fft::pipelined_streaming_io;

// Twiddle and scaling
const char FFT_TWIDDLE_WIDTH_2048   = 24;  // Float twiddle: 24 bits
const hls::ip_fft::scaling FFT_SCALING_2048 = hls::ip_fft::scaled;
const hls::ip_fft::rounding FFT_ROUNDING_2048 = hls::ip_fft::truncation;

// Output ordering (natural order for easier integration)
const hls::ip_fft::ordering FFT_OUTPUT_ORDER_2048 = hls::ip_fft::natural_order;

// Memory configuration
const hls::ip_fft::mem FFT_MEM_DATA_2048 = hls::ip_fft::block_ram;
const hls::ip_fft::mem FFT_MEM_PHASE_FACTORS_2048 = hls::ip_fft::block_ram;

// Optimization: Use DSPs for complex multiplication (better performance)
const hls::ip_fft::opt FFT_COMPLEX_MULT_TYPE_2048 = hls::ip_fft::use_dsp;

// ============================================================================
// FFT Configuration Struct (HLS Synthesis Only)
// ============================================================================

struct config_fft_2048 : hls::ip_fft::params_t {
    // Fixed transform length: 128-point
    static const int FFT_NFFT_MAX = FFT_NFFT_MAX_2048;
    static const bool FFT_HAS_NFFT = FFT_HAS_NFFT_2048;
    
    // Input/output data width
    static const int FFT_INPUT_WIDTH = FFT_INPUT_WIDTH_2048;
    static const int FFT_OUTPUT_WIDTH = FFT_OUTPUT_WIDTH_2048;
    
    // Twiddle width
    static const int FFT_TWIDDLE_WIDTH = FFT_TWIDDLE_WIDTH_2048;
    
    // Architecture
    static const hls::ip_fft::arch FFT_ARCH = FFT_ARCH_2048;
    
    // Scaling mode
    static const hls::ip_fft::scaling FFT_SCALING = FFT_SCALING_2048;
    static const hls::ip_fft::rounding FFT_ROUNDING = FFT_ROUNDING_2048;
    
    // Output ordering
    static const hls::ip_fft::ordering FFT_OUTPUT_ORDERING = FFT_OUTPUT_ORDER_2048;
    
    // Memory options
    static const hls::ip_fft::mem FFT_MEM_DATA = FFT_MEM_DATA_2048;
    static const hls::ip_fft::mem FFT_MEM_PHASE_FACTORS = FFT_MEM_PHASE_FACTORS_2048;
    
    // Complex multiplication optimization
    static const hls::ip_fft::opt FFT_COMPLEX_MULT_TYPE = FFT_COMPLEX_MULT_TYPE_2048;
};

// ============================================================================
// FFT Config and Status Types (HLS Synthesis Only)
// ============================================================================

typedef hls::ip_fft::config_t<config_fft_2048> fft_config_2048_t;
typedef hls::ip_fft::status_t<config_fft_2048> fft_status_2048_t;

// ============================================================================
// Stream-based FFT Wrapper Functions (HLS Synthesis Only)
// ============================================================================

/**
 * 1D FFT/IFFT using HLS FFT IP (Stream-based)
 * 
 * Replaces fft_1d_direct_2048() with hls::fft IP
 * 
 * Key differences from direct DFT:
 *   - Uses stream interface (lower latency)
 *   - DSP-based complex multiplication (lower DSP count)
 *   - Pipelined architecture (higher throughput)
 * 
 * @param in_stream   Input data stream (128 complex)
 * @param out_stream  Output data stream (128 complex)
 * @param is_inverse  true for IFFT, false for FFT
 */
void fft_1d_hls_stream_2048(
    hls::stream<std::complex<float>>& in_stream,
    hls::stream<std::complex<float>>& out_stream,
    bool is_inverse
);

/**
 * 2D FFT/IFFT using HLS FFT IP (Stream-based)
 * 
 * Replaces fft_2d_full_2048() with hls::fft IP
 * 
 * Implementation: Row FFT → Transpose → Column FFT
 * 
 * @param input       Input 2D array (128×128)
 * @param output      Output 2D array (128×128)
 * @param is_inverse  true for IFFT, false for FFT
 */
void fft_2d_hls_stream_2048(
    std::complex<float> input[MAX_FFT_Y][MAX_FFT_X],
    std::complex<float> output[MAX_FFT_Y][MAX_FFT_X],
    bool is_inverse
);

#else
// ============================================================================
// Standard C++ Simulation Mode (No HLS-specific code)
// ============================================================================
// In simulation mode, use direct DFT implementation (defined in socs_2048.cpp)
// No HLS FFT configuration needed

#endif // __SYNTHESIS__

#endif // HLS_FFT_CONFIG_2048_H 
 * @param input   Input 2D array (128×128)
 * @param output  Output 2D array (128×128)
 * @param is_inverse  true for IFFT, false for FFT
 */
void fft_2d_hls_stream_2048(
    std::complex<float> input[128][128],
    std::complex<float> output[128][128],
    bool is_inverse
);

#endif // HLS_FFT_CONFIG_2048_H