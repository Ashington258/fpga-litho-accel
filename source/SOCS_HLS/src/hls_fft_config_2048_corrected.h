/**
 * HLS FFT Configuration for SOCS 128-point FFT (Corrected)
 * 
 * CRITICAL FIX: Must override default params_t values:
 *   - Default max_nfft = 10 (1024-point) -> Override to 7 (128-point)
 *   - Default input_width = 16-bit -> Override to 32-bit
 * 
 * This configuration is used for 2D FFT in SOCS reconstruction.
 */
#include "ap_fixed.h"
#include "hls_fft.h"
#include <complex>
#include <hls_stream.h>

// ============================================================================
// Global FFT Parameters (Override Defaults)
// ============================================================================

const int  FFT_LENGTH_128     = 128;    // 128-point FFT for SOCS
const int  FFT_NFFT_MAX_128   = 7;      // log2(128)
const int  FFT_INPUT_WIDTH_32 = 32;     // 32-bit precision (8-bit aligned)

// ============================================================================
// Type Definitions (ap_fixed for HLS FFT IP)
// ============================================================================

// Input/Output type: ap_fixed<32, 1> (HLS FFT native precision)
// HLS FFT library mandates 1-bit integer part (library design limitation)
// 
// SCALING STRATEGY: Use scaled mode (scaling=0x7F) to match FFTW BACKWARD
// - HLS FFT scaled mode: divides by N for each 1D FFT (total 1/N² for 2D)
// - FFTW BACKWARD: divides by N for each 1D IFFT (total 1/N² for 2D)
// - Result: HLS FFT output matches FFTW BACKWARD without manual compensation
typedef ap_fixed<FFT_INPUT_WIDTH_32, 1> data_fft_in_t;  // Matches HLS FFT library expectation
typedef std::complex<data_fft_in_t> cmpx_fft_in_t;

typedef ap_fixed<FFT_INPUT_WIDTH_32, 1> data_fft_out_t;  // Same as input for consistency
typedef std::complex<data_fft_out_t> cmpx_fft_out_t;

// ============================================================================
// FFT Config Struct (CRITICAL: Must override defaults)
// ============================================================================

struct config_socs_fft : hls::ip_fft::params_t {
    // Override default parameters (otherwise defaults to 1024-point, 16-bit)
    static const unsigned log2_transform_length = FFT_NFFT_MAX_128;  // 7 for 128-point
    static const unsigned input_width = FFT_INPUT_WIDTH_32;          // 32-bit total (integer part fixed to 1 by library)
    static const unsigned output_width = FFT_INPUT_WIDTH_32;         // 32-bit total (integer part fixed to 1 by library)
    static const unsigned output_ordering = hls::ip_fft::natural_order;
    
    // Optional: specify implementation options
    static const unsigned implementation_options = hls::ip_fft::pipelined_streaming_io;
    
    // SCALING STRATEGY: Use scaled mode to match FFTW BACKWARD behavior
    // - HLS FFT scaled mode: divides by N for each 1D FFT (total 1/N² for 2D)
    // - FFTW BACKWARD: divides by N for each 1D IFFT (total 1/N² for 2D)
    // - Result: HLS FFT output matches FFTW BACKWARD without manual compensation
    static const unsigned scaling_options = hls::ip_fft::scaled;
};

// ============================================================================
// FFT Helper Functions (Stream Interface)
// ============================================================================

/**
 * Convert 2D array to stream for HLS FFT IP
 */
void array_to_stream_128(
    cmpx_fft_in_t input[128][128],
    hls::stream<cmpx_fft_in_t>& out_stream
) {
    #pragma HLS INLINE off
    
    for (int i = 0; i < 128; i++) {
        for (int j = 0; j < 128; j++) {
            #pragma HLS PIPELINE II=1
            out_stream.write(input[i][j]);
        }
    }
}

/**
 * Convert stream to 2D array after HLS FFT IP
 */
void stream_to_array_128(
    hls::stream<cmpx_fft_out_t>& in_stream,
    cmpx_fft_out_t output[128][128]
) {
    #pragma HLS INLINE off
    
    for (int i = 0; i < 128; i++) {
        for (int j = 0; j < 128; j++) {
            #pragma HLS PIPELINE II=1
            output[i][j] = in_stream.read();
        }
    }
}

/**
 * 1D HLS FFT IP call (128-point)
 */
void fft_1d_hls_128(
    hls::stream<cmpx_fft_in_t>& in_stream,
    hls::stream<cmpx_fft_out_t>& out_stream,
    bool is_inverse
) {
    #pragma HLS interface ap_fifo depth=128 port=in_stream,out_stream
    #pragma HLS stream variable=in_stream
    #pragma HLS stream variable=out_stream
    #pragma HLS dataflow
    
    // FFT direction: 0=forward, 1=inverse
    ap_uint<1> dir = is_inverse ? 1 : 0;
    
    // CRITICAL FIX: Use scaled mode for IFFT to match FFTW BACKWARD behavior
    // FFTW BACKWARD: x[n] = (1/N) * sum_k X[k] * exp(+2πi*k*n/N)
    // HLS FFT scaled mode: divides by N for each stage (total 1/N for 1D FFT)
    // For 2D FFT: total scaling = 1/(N*M) = 1/16384 (matches FFTW BACKWARD)
    // 
    // Scaling schedule: 1 bit right shift per stage (standard scaled mode)
    // For 128-point FFT (7 stages): scaling = 0b1111111 = 0x7F
    ap_uint<15> scaling = 0x7F;  // Scaled mode: divide by 2^7 = 128 for 1D FFT
    
    bool status;
    
    // Call HLS FFT IP (scaled mode: IFFT divides by N, matches FFTW BACKWARD)
    hls::fft<config_socs_fft>(in_stream, out_stream, dir, scaling, -1, &status);
}

/**
 * 2D HLS FFT using stream interface (128×128)
 * 
 * IMPORTANT: This implementation uses HLS FFT IP for synthesis.
 * For C simulation, use direct DFT (defined in socs_2048.cpp)
 */
void fft_2d_hls_128(
    cmpx_fft_in_t input[128][128],
    cmpx_fft_out_t output[128][128],
    bool is_inverse
) {
    #pragma HLS DATAFLOW
    
    // Internal streams
    hls::stream<cmpx_fft_in_t> row_stream("row_stream");
    hls::stream<cmpx_fft_out_t> row_fft_stream("row_fft_stream");
    hls::stream<cmpx_fft_in_t> col_stream("col_stream");
    hls::stream<cmpx_fft_out_t> col_fft_stream("col_fft_stream");
    
    #pragma HLS stream variable=row_stream depth=128
    #pragma HLS stream variable=row_fft_stream depth=128
    #pragma HLS stream variable=col_stream depth=128
    #pragma HLS stream variable=col_fft_stream depth=128
    
    // Temporary transpose buffer
    cmpx_fft_out_t temp[128][128];
    #pragma HLS RESOURCE variable=temp core=RAM_2P_BRAM
    
    // Stage 1: Row FFT
    for (int row = 0; row < 128; row++) {
        #pragma HLS UNROLL factor=4
        
        // Load row to stream
        for (int col = 0; col < 128; col++) {
            #pragma HLS PIPELINE II=1
            row_stream.write(input[row][col]);
        }
        
        // FFT on row
        fft_1d_hls_128(row_stream, row_fft_stream, is_inverse);
        
        // Store row result
        for (int col = 0; col < 128; col++) {
            #pragma HLS PIPELINE II=1
            temp[row][col] = row_fft_stream.read();
        }
    }
    
    // Stage 2: Column FFT (transpose + FFT)
    for (int col = 0; col < 128; col++) {
        #pragma HLS UNROLL factor=4
        
        // Load column to stream (transpose)
        for (int row = 0; row < 128; row++) {
            #pragma HLS PIPELINE II=1
            col_stream.write(temp[row][col]);
        }
        
        // FFT on column
        fft_1d_hls_128(col_stream, col_fft_stream, is_inverse);
        
        // Store column result (transpose back)
        for (int row = 0; row < 128; row++) {
            #pragma HLS PIPELINE II=1
            output[row][col] = col_fft_stream.read();
        }
    }
}