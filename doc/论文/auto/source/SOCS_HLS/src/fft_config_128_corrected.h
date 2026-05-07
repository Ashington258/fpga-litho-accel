/**
 * HLS FFT Configuration for 128-point FFT (Corrected Pattern)
 * Following reference implementation pattern exactly
 */
#include "ap_fixed.h"
#include "hls_fft.h"

// Global parameters (128-point FFT)
const char FFT_INPUT_WIDTH                     = 32;  // 8-bit aligned
const char FFT_OUTPUT_WIDTH                    = FFT_INPUT_WIDTH;
const char FFT_STATUS_WIDTH                    = 8;
const char FFT_CHANNELS                        = 1;
const int  FFT_LENGTH                          = 128;  // Target 128-point
const char FFT_NFFT_MAX                        = 7;    // log2(128)
const bool FFT_HAS_NFFT                        = 0;    // Fixed length (false)
const hls::ip_fft::arch FFT_ARCH               = hls::ip_fft::pipelined_streaming_io;
const char FFT_TWIDDLE_WIDTH                   = 32;  // Match input width
const hls::ip_fft::ordering FFT_OUTPUT_ORDER   = hls::ip_fft::natural_order;
const bool FFT_OVFLO                           = 1;
const bool FFT_CYCLIC_PREFIX_INSERTION         = 0;
const bool FFT_XK_INDEX                        = 0;
// COMPENSATION STRATEGY: Use scaled mode (HLS FFT native support)
// - Scaled IFFT divides by N² = 16384 (128×128 2D FFT)
// - Manual compensation after IFFT: multiply output by 16384
// - Matches FFTW BACKWARD behavior after compensation
const hls::ip_fft::scaling FFT_SCALING = hls::ip_fft::scaled;
const hls::ip_fft::rounding FFT_ROUNDING       = hls::ip_fft::truncation;
const hls::ip_fft::mem FFT_MEM_DATA            = hls::ip_fft::block_ram;
const hls::ip_fft::mem FFT_MEM_PHASE_FACTORS   = hls::ip_fft::block_ram;
const hls::ip_fft::mem FFT_MEM_REORDER         = hls::ip_fft::block_ram;
const char FFT_STAGES_BLOCK_RAM                = 4;
const bool FFT_MEM_OPTIONS_HYBRID              = 0;
const hls::ip_fft::opt FFT_COMPLEX_MULT_TYPE   = hls::ip_fft::use_luts;
const hls::ip_fft::opt FFT_BUTTERLY_TYPE       = hls::ip_fft::use_luts;

// Type definitions (HLS FFT native precision)
// Input: ap_fixed<32, 1> - HLS FFT library mandates 1-bit integer part
// Output: ap_fixed<32, 1> - same precision (integer part fixed by library)
// COMPENSATION: Manual N² scaling after IFFT to match FFTW BACKWARD
typedef ap_fixed<FFT_INPUT_WIDTH, 1> data_in_128_t;
typedef ap_fixed<FFT_OUTPUT_WIDTH, 1> data_out_128_t;

#include <complex>
typedef std::complex<data_in_128_t> cmpx_in_128_t;
typedef std::complex<data_out_128_t> cmpx_out_128_t;

using namespace std;

// Config struct (must override defaults: max_nfft=10, input_width=16)
struct config_128 : hls::ip_fft::params_t {
    // Override default parameters (critical for correct array sizing)
    static const unsigned log2_transform_length = 7;  // 128-point FFT (not default 10)
    static const unsigned input_width = 32;            // 32-bit total (integer part fixed to 1)
    static const unsigned output_width = 32;           // 32-bit total (integer part fixed to 1)
    static const unsigned output_ordering = hls::ip_fft::natural_order;
};

// FFT types
typedef hls::ip_fft::config_t<config_128> config_t_128;
typedef hls::ip_fft::status_t<config_128> status_t_128;