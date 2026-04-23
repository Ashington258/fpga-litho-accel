/**
 * Optimized SOCS HLS Configuration Header
 * FPGA-Litho Project - Vitis FFT IP Integration
 */

#ifndef SOCS_CONFIG_OPTIMIZED_H
#define SOCS_CONFIG_OPTIMIZED_H

#include <ap_fixed.h>
#include <ap_int.h>
#include <hls_fft.h>
#include <complex>

// ============================================================================
// Compile-time Configuration (Nx=4, Ny=4 case)
// ============================================================================
#ifndef SOCS_NX_OVERRIDE
    #define Nx 4
    #define Ny 4
#endif

#define Lx 512
#define Ly 512
#define nk 4           // Number of SOCS kernels

// Derived dimensions
#define convX (4*Nx + 1)   // 17
#define convY (4*Ny + 1)   // 17
#define kerX  (2*Nx + 1)   // 9
#define kerY  (2*Ny + 1)   // 9

// FFT dimensions (power-of-two padded)
#define fftConvX 32
#define fftConvY 32

// ============================================================================
// FFT Parameters (FLOATING-POINT - avoids fixed-point width constraints)
// ============================================================================
// Using float FFT avoids the fixed-point output_width alignment issue:
// - Fixed-point unscaled requires: output_width = input_width + nfft_max + 1 = 22
// - HLS FFT IP alignment requires: (22+7)/8*8 = 24
// - Vivado FFT IP rejects 24 for unscaled -> CONFLICT!
// Solution: Use std::complex<float> which has no width constraints
const char FFT_NFFT_MAX                        = 5;   // log2(32) = 5
const int  FFT_LENGTH                          = 32;

// ============================================================================
// FFT Data Types (FLOATING-POINT - use HLS FFT native float support)
// ============================================================================
typedef std::complex<float> cmpx_fft_t;      // Float FFT input/output (no width constraints)

// ============================================================================
// FFT Configuration Class (FLOATING-POINT - specific width requirements)
// ============================================================================
struct fft_config_socs : hls::ip_fft::params_t {
    static const unsigned nfft_max = 5;               // log2(32) = 5
    static const unsigned has_nfft = 0;               // Fixed transform length
    static const unsigned config_width = 16;          // FLOAT FFT requires config_width=16
    static const unsigned phase_factor_width = 24;    // FLOAT FFT requires 24 or 25
    static const hls::ip_fft::scaling scaling_opt = hls::ip_fft::unscaled;    // Unscaled mode (match FFTW)
    static const hls::ip_fft::arch arch_type = hls::ip_fft::pipelined_streaming_io;
    static const hls::ip_fft::mem mem_data = hls::ip_fft::block_ram;
    static const hls::ip_fft::mem mem_phase_factors = hls::ip_fft::block_ram;
    static const hls::ip_fft::mem mem_reorder = hls::ip_fft::block_ram;
    static const hls::ip_fft::opt complex_mult_type = hls::ip_fft::use_luts;  // DSP-free
    static const hls::ip_fft::opt butterfly_type = hls::ip_fft::use_luts;     // DSP-free
};

// ============================================================================
// Runtime Parameters (from Host)
// ============================================================================
struct socs_params_t {
    int nx;
    int ny;
    int lxy;
    int lyy;
    int fft_size;
    int n_kernels;
};

#endif // SOCS_CONFIG_OPTIMIZED_H