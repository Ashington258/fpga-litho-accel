/**
 * SOCS HLS Dynamic Configuration Header
 * FPGA-Litho Project
 * 
 * Implements dynamic Nx/Ny calculation based on optical parameters
 * Formula: Nx = floor(NA * Lx * (1 + sigma_outer) / wavelength)
 * 
 * Usage:
 *   - For HLS synthesis: Use compile-time macro SOCS_NX_OVERRIDE
 *   - For Host runtime:  Calculate from config.json and pass to IP
 */

#ifndef SOCS_CONFIG_H
#define SOCS_CONFIG_H

// HLS headers must be included before using ap_fixed and hls::ip_fft types
#include <ap_fixed.h>
#include <hls_fft.h>
#include <cmath>
#include <complex>

// ============================================================================
// Compile-time Override (for HLS synthesis)
// ============================================================================
// If SOCS_NX_OVERRIDE is defined, use that value (enables HLS optimization)
// Otherwise, calculate dynamically based on optical parameters

#ifdef SOCS_NX_OVERRIDE
    #define Nx SOCS_NX_OVERRIDE
    #define Ny SOCS_NX_OVERRIDE
#else
    // Default configuration (matches golden_original.json)
    // NA=0.8, Lx=512, lambda=193, sigma_outer=0.9
    // Nx = floor(0.8 * 512 * 1.9 / 193) = floor(4.03) = 4
    #define Nx 4
    #define Ny 4
#endif

// ============================================================================
// Mask Dimensions (from config.json mask.period)
// ============================================================================
#define Lx 512
#define Ly 512

// ============================================================================
// Derived Parameters (all dynamically computed from Nx)
// ============================================================================
#define convX  (4 * Nx + 1)    // Convolution output width
#define convY  (4 * Ny + 1)    // Convolution output height
#define kerX   (2 * Nx + 1)    // Kernel support width
#define kerY   (2 * Ny + 1)    // Kernel support height

// ============================================================================
// FFT Dimensions (next power of two)
// ============================================================================
// FFT size must be 2^n (Vitis HLS FFT IP requirement)
// For Nx=4:  convX=17, next_pow2=32
// For Nx=16: convX=65, next_pow2=128
#define fftConvX (1 << (SOCS_LOG2_NEXT_POW2(convX)))
#define fftConvY (1 << (SOCS_LOG2_NEXT_POW2(convY)))

// Helper macro: log2 of next power of two
#define SOCS_LOG2_NEXT_POW2(n) \
    ((n) <=   1 ? 0 : \
     (n) <=   2 ? 1 : \
     (n) <=   4 ? 2 : \
     (n) <=   8 ? 3 : \
     (n) <=  16 ? 4 : \
     (n) <=  32 ? 5 : \
     (n) <=  64 ? 6 : \
     (n) <= 128 ? 7 : \
     (n) <= 256 ? 8 : 9)

// ============================================================================
// FFT Configuration for Vitis HLS FFT IP (Stream-based API)
// ============================================================================
// Following Vitis reference: interface_stream/fft_top.h pattern
// Use fixed-point for DSP optimization (80%+ reduction)
//
// CRITICAL: Only define FFT IP config during C Synthesis (__SYNTHESIS__ macro)
// C Simulation will use pure DFT implementation in socs_simple.cpp
// This avoids stream API issues during C Simulation

// ============================================================================
// Global FFT Type Configuration (Required for both C Simulation and C Synthesis)
// ============================================================================
// CRITICAL: These macros must be defined BEFORE any conditional compilation blocks
// They are used in type definitions (fft_data_t/cmpx_fft_t) which are shared

#define FFT_USE_FLOAT      1   // ENABLE float FFT (LUT mode for DSP-free)
#define FFT_INPUT_WIDTH    32  // Bit width for fixed-point FFT (if FFT_USE_FLOAT=0)

// ============================================================================
// FFT Data Types (Shared by C Simulation and C Synthesis)
// ============================================================================
// C Simulation uses fft_data_t/cmpx_fft_t in fft_1d_direct function
// C Synthesis uses them in HLS FFT IP instantiation

#if FFT_USE_FLOAT
typedef float                                             fft_data_t;
#else
typedef ap_fixed<FFT_INPUT_WIDTH, 1>                    fft_data_t;  // UNUSED (bit-width mismatch)
#endif
typedef std::complex<fft_data_t>                        cmpx_fft_t;

// ============================================================================
// FFT IP-Specific Configuration (Only for C Synthesis)
// ============================================================================
#ifdef __SYNTHESIS__  // HLS FFT IP configuration (only for C Synthesis)

#define FFT_OUTPUT_WIDTH  32
#define FFT_TWIDDLE_WIDTH 24
#define FFT_CHANNELS      1

// FFT_NFFT_MAX = log2(max_fft_size) = 5 for 32-point (fixed size)
// Fixed FFT size: 32-point (fftConvX=32 is compile-time constant)
#define FFT_NFFT_MAX      5
#define FFT_HAS_NFFT      0  // Fixed size FFT (no runtime configurable length)

// Config channel width requirements (from Vitis HLS FFT IP documentation)
// - For has_nfft=1: config_width = 24 (dir(1) + scaling(15) + nfft(8))
// - For has_nfft=0: config_width = 16 (dir(1) + scaling(15))
#define FFT_CONFIG_WIDTH  16
#define FFT_PHASE_WIDTH   24  // Phase factor width for floating-point FFT

// Architecture: pipelined streaming (best throughput)
#define FFT_ARCH          hls::ip_fft::pipelined_streaming_io

#endif  // __SYNTHESIS__ (FFT IP config parameters)

// ============================================================================
// FFT IP Configuration Struct (Only for C Synthesis)
// ============================================================================
#ifdef __SYNTHESIS__  // FFT IP struct (only for C Synthesis)

// FFT config struct (Vitis stream API pattern)
struct fft_config_socs : hls::ip_fft::params_t {
    static const unsigned input_width              = FFT_INPUT_WIDTH;
    static const unsigned output_width             = FFT_OUTPUT_WIDTH;
    static const unsigned twiddle_width            = FFT_TWIDDLE_WIDTH;
    static const unsigned channels                 = FFT_CHANNELS;
    static const unsigned nfft_max                 = FFT_NFFT_MAX;
    static const bool       has_nfft               = FFT_HAS_NFFT;
    static const unsigned config_width             = FFT_CONFIG_WIDTH;
    static const unsigned phase_factor_width       = FFT_PHASE_WIDTH;
    static const unsigned ordering                 = hls::ip_fft::natural_order;
    static const unsigned scaling                  = hls::ip_fft::unscaled;  // Match FFTW BACKWARD behavior
    static const unsigned rounding                 = hls::ip_fft::truncation;
    static const unsigned arch                     = FFT_ARCH;
    static const unsigned stages_block_ram         = 0;
    static const unsigned complex_mult_type        = hls::ip_fft::use_luts;  // DSP-free (float mode also uses LUTs)
    static const unsigned butterfly_type           = hls::ip_fft::use_luts;  // DSP-free (float mode also uses LUTs)
};

typedef hls::ip_fft::config_t<fft_config_socs>  fft_config_t;
typedef hls::ip_fft::status_t<fft_config_socs>  fft_status_t;

#endif  // __SYNTHESIS__ (FFT IP config for C Synthesis only)

// ============================================================================
// Runtime Parameter Structure (for Host → IP communication)
// ============================================================================
struct socs_params_t {
    int nx;           // Dynamic Nx (passed from Host)
    int ny;           // Dynamic Ny
    int lxy;          // Lx/2 (mask center)
    int lyy;          // Ly/2
    int fft_size;     // Actual FFT length (32, 64, 128)
    int n_kernels;    // Number of SOCS kernels
};

// ============================================================================
// Utility Functions (Host-side, for runtime calculation)
// ============================================================================
// NOTE: These functions use prefixed parameter names to avoid macro conflicts
// (Lx, Ly, Nx, Ny are defined as macros above for HLS synthesis)

/**
 * Calculate Nx from optical parameters (Host-side function)
 * Formula: Nx = floor(NA * Lx * (1 + sigma_outer) / wavelength)
 */
inline int calculate_nx(double NA, int Lx_val, double wavelength, double sigma_outer) {
    return static_cast<int>(std::floor(NA * Lx_val * (1.0 + sigma_outer) / wavelength));
}

/**
 * Calculate next power of two (Host-side function)
 */
inline int next_power_of_two(int n) {
    int power = 1;
    while (power < n) power *= 2;
    return power;
}

/**
 * Calculate FFT size from Nx (Host-side function)
 */
inline int calculate_fft_size(int nx) {
    int conv_size = 4 * nx + 1;
    return next_power_of_two(conv_size);
}

#endif // SOCS_CONFIG_H