/**
 * HLS FFT Configuration (Exact Reference Pattern)
 * FPGA-Litho Project
 * 
 * CRITICAL: Must follow reference implementation EXACTLY!
 * Reference: reference/vitis_hls_ftt的实现/interface_stream/fft_top.h
 */

#ifdef __SYNTHESIS__

#include "ap_fixed.h"
#include "hls_fft.h"
#include <complex>

// ============================================================================
// FFT Global Parameters (128-point Transform)
// ============================================================================

// configurable params (following reference pattern EXACTLY)
// CRITICAL: input_width must be multiple of 8 (HLS FFT aligns to 8-bit boundaries)
const char FFT_INPUT_WIDTH                     = 32;  // 32-bit fixed-point (aligned to 8)
const char FFT_OUTPUT_WIDTH                    = FFT_INPUT_WIDTH;
const char FFT_STATUS_WIDTH                    = 8;
const char FFT_CHANNELS                        = 1;
const int  FFT_LENGTH                          = 128;  // 128-point FFT (2^7)
const char FFT_NFFT_MAX                        = 7;    // log2(128)
const bool FFT_HAS_NFFT                        = 0;    // Fixed-length
const hls::ip_fft::arch FFT_ARCH               = hls::ip_fft::pipelined_streaming_io;
const char FFT_TWIDDLE_WIDTH                   = 16;
const hls::ip_fft::ordering FFT_OUTPUT_ORDER   = hls::ip_fft::natural_order;
const bool FFT_OVFLO                           = 1;
const bool FFT_CYCLIC_PREFIX_INSERTION         = 0;
const bool FFT_XK_INDEX                        = 0;
// COMPENSATION STRATEGY: Use scaled mode (HLS FFT native support)
// - Scaled IFFT divides by N² = 16384 (128×128 2D FFT)
// - Manual compensation after IFFT: multiply output by 16384
// - Matches FFTW BACKWARD behavior after compensation
const hls::ip_fft::scaling FFT_SCALING         = hls::ip_fft::scaled;
const hls::ip_fft::rounding FFT_ROUNDING       = hls::ip_fft::truncation;

// not configurable yet (following reference)
const hls::ip_fft::mem FFT_MEM_DATA            = hls::ip_fft::block_ram;
const hls::ip_fft::mem FFT_MEM_PHASE_FACTORS   = hls::ip_fft::block_ram;
const hls::ip_fft::mem FFT_MEM_REORDER         = hls::ip_fft::block_ram;
const char FFT_STAGES_BLOCK_RAM                = 4;
const bool FFT_MEM_OPTIONS_HYBRID              = 0;
const hls::ip_fft::opt FFT_COMPLEX_MULT_TYPE   = hls::ip_fft::use_luts;
const hls::ip_fft::opt FFT_BUTTERLY_TYPE       = hls::ip_fft::use_luts;

// ============================================================================
// Fixed-Point Types (following reference pattern)
// ============================================================================

// Type definitions (HLS FFT native precision)
// ap_fixed<32, 1> - HLS FFT library mandates 1-bit integer part
// COMPENSATION: Manual N² scaling after IFFT to match FFTW BACKWARD
typedef ap_fixed<FFT_INPUT_WIDTH, 1> data_in_2048_t;  // ap_fixed<32,1>
typedef ap_fixed<FFT_OUTPUT_WIDTH, 1> data_out_2048_t;  // ap_fixed<32,1>

// Complex types for HLS FFT (following reference)
typedef std::complex<data_in_2048_t> cmpx_hls_2048_t;
typedef std::complex<data_out_2048_t> cmpx_hls_out_2048_t;

// ============================================================================
// Configuration Struct (minimal, following reference pattern)
// ============================================================================

struct config_fft_2048 : hls::ip_fft::params_t {
    // CRITICAL: Override default 1024-point FFT parameters
    static const unsigned max_length = FFT_LENGTH;  // 128-point
    static const unsigned nfft_max = FFT_NFFT_MAX;  // log2(128) = 7
    
    // HLS FFT library mandates integer part = 1 (no custom config)
    static const hls::ip_fft::ordering ordering = hls::ip_fft::natural_order;
};

#endif // __SYNTHESIS__