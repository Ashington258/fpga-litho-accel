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

#include <cmath>
#include <complex>

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
const hls::ip_fft::scaling FFT_SCALING_2048 = hls::ip_fft::scaled;  // COMPENSATION: Manual N² scaling after IFFT
const hls::ip_fft::rounding FFT_ROUNDING_2048 = hls::ip_fft::truncation;

// Output ordering (natural order for easier integration)
const hls::ip_fft::ordering FFT_OUTPUT_ORDER_2048 = hls::ip_fft::natural_order;

// Memory configuration (not used in HLS FFT IP, kept for compatibility)
// const hls::ip_fft::mem FFT_MEM_DATA_2048 = hls::ip_fft::block_ram;
// const hls::ip_fft::mem FFT_MEM_PHASE_FACTORS_2048 = hls::ip_fft::block_ram;

// Optimization: Use DSPs for complex multiplication (configured in struct)
// const hls::ip_fft::opt FFT_COMPLEX_MULT_TYPE_2048 = hls::ip_fft::use_dsp;

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
    
    // Memory options (not used in HLS FFT IP, kept for compatibility)
    // static const hls::ip_fft::mem FFT_MEM_DATA = FFT_MEM_DATA_2048;
    // static const hls::ip_fft::mem FFT_MEM_PHASE_FACTORS = FFT_MEM_PHASE_FACTORS_2048;
    
    // Complex multiplication optimization (configured in struct)
    // static const hls::ip_fft::opt FFT_COMPLEX_MULT_TYPE = FFT_COMPLEX_MULT_TYPE_2048;
};

// ============================================================================
// FFT Config and Status Types (HLS Synthesis Only)
// ============================================================================

typedef hls::ip_fft::config_t<config_fft_2048> fft_config_2048_t;
typedef hls::ip_fft::status_t<config_fft_2048> fft_status_2048_t;

// ============================================================================
// Stream-based FFT Wrapper Functions (HLS Synthesis Only)
// ============================================================================

void fft_1d_hls_stream_2048(
    hls::stream<std::complex<float>>& in_stream,
    hls::stream<std::complex<float>>& out_stream,
    bool is_inverse
);

void fft_2d_hls_stream_2048(
    std::complex<float> input[128][128],
    std::complex<float> output[128][128],
    bool is_inverse
);

#endif // __SYNTHESIS__

#endif // HLS_FFT_CONFIG_2048_H