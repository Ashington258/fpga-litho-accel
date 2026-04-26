/**
 * @file fft_2d_test_24bit.cpp
 * @brief Minimal 2D FFT test with ap_fixed<24,1> to validate 2D FFT implementation
 * 
 * Purpose: Isolate 2D FFT issues from full SOCS pipeline
 * 
 * Test strategy:
 *   1. Create simple 2D impulse input
 *   2. Run 2D FFT (forward + inverse)
 *   3. Verify output matches input (within precision)
 */

#include "ap_fixed.h"
#include "hls_fft.h"
#include <complex>
#include <hls_stream.h>
#include <iostream>
#include <cmath>

using namespace std;

// ============================================================================
// Type Definitions
// ============================================================================

// CRITICAL FIX: Use 2-bit integer part to represent values in range [-2.0, +1.99999988]
// ap_fixed<24,1> can only represent [-1.0, +0.99999988], which cannot represent 1.0!
typedef ap_fixed<24, 2> data_t;  // Changed from <24,1> to <24,2>
typedef std::complex<data_t> cmpx_t;

// ============================================================================
// FFT Configuration
// ============================================================================

struct config_2d : hls::ip_fft::params_t {
    static const unsigned max_nfft = 7;  // log2(128) = 7
    static const unsigned input_width = 24;
    static const unsigned output_width = 24;
    static const unsigned output_ordering = hls::ip_fft::natural_order;
    static const unsigned implementation_options = hls::ip_fft::pipelined_streaming_io;
    static const unsigned scaling_options = hls::ip_fft::scaled;
};

// ============================================================================
// 1D FFT Function
// ============================================================================

void fft_1d_128(
    hls::stream<cmpx_t> &in_stream,
    hls::stream<cmpx_t> &out_stream,
    bool is_inverse
) {
    #pragma HLS interface ap_fifo depth=128 port=in_stream,out_stream
    #pragma HLS stream variable=in_stream
    #pragma HLS stream variable=out_stream
    #pragma HLS dataflow
    
    ap_uint<1> dir = is_inverse ? 1 : 0;
    ap_uint<15> scaling = 0x7F;  // Scaled mode
    
    bool status;
    hls::fft<config_2d>(in_stream, out_stream, dir, scaling, -1, &status);
}

// ============================================================================
// 2D FFT Function (Sequential, no UNROLL)
// ============================================================================

void fft_2d_128(
    cmpx_t input[128][128],
    cmpx_t output[128][128],
    bool is_inverse
) {
    #pragma HLS DATAFLOW
    
    // Internal streams
    hls::stream<cmpx_t> row_stream("row_stream");
    hls::stream<cmpx_t> row_fft_stream("row_fft_stream");
    hls::stream<cmpx_t> col_stream("col_stream");
    hls::stream<cmpx_t> col_fft_stream("col_fft_stream");
    
    #pragma HLS stream variable=row_stream depth=128
    #pragma HLS stream variable=row_fft_stream depth=128
    #pragma HLS stream variable=col_stream depth=128
    #pragma HLS stream variable=col_fft_stream depth=128
    
    // Temporary transpose buffer
    cmpx_t temp[128][128];
    #pragma HLS RESOURCE variable=temp core=RAM_2P_BRAM
    
    // Stage 1: Row FFT (sequential processing)
    for (int row = 0; row < 128; row++) {
        // Load row to stream
        for (int col = 0; col < 128; col++) {
            #pragma HLS PIPELINE II=1
            row_stream.write(input[row][col]);
        }
        
        // FFT on row
        fft_1d_128(row_stream, row_fft_stream, is_inverse);
        
        // Store row result
        for (int col = 0; col < 128; col++) {
            #pragma HLS PIPELINE II=1
            temp[row][col] = row_fft_stream.read();
        }
    }
    
    // Stage 2: Column FFT (transpose + FFT, sequential processing)
    for (int col = 0; col < 128; col++) {
        // Load column to stream (transpose)
        for (int row = 0; row < 128; row++) {
            #pragma HLS PIPELINE II=1
            col_stream.write(temp[row][col]);
        }
        
        // FFT on column
        fft_1d_128(col_stream, col_fft_stream, is_inverse);
        
        // Store column result (transpose back)
        for (int row = 0; row < 128; row++) {
            #pragma HLS PIPELINE II=1
            output[row][col] = col_fft_stream.read();
        }
    }
}

// ============================================================================
// Top-Level Function
// ============================================================================

void fft_2d_test_24bit(
    cmpx_t input[128][128],
    cmpx_t output[128][128],
    bool is_inverse
) {
    #pragma HLS INTERFACE bram port=input
    #pragma HLS INTERFACE bram port=output
    #pragma HLS INTERFACE s_axilite port=is_inverse
    #pragma HLS INTERFACE s_axilite port=return
    
    fft_2d_128(input, output, is_inverse);
}

// ============================================================================
// Testbench
// ============================================================================

int main() {
    cout << "========================================" << endl;
    cout << "2D FFT Test with ap_fixed<24,1>" << endl;
    cout << "========================================" << endl;
    
    // Create 2D impulse input (DC at center)
    cmpx_t input[128][128];
    cmpx_t output[128][128];
    
    // Initialize to zero
    for (int i = 0; i < 128; i++) {
        for (int j = 0; j < 128; j++) {
            input[i][j] = cmpx_t(0.0, 0.0);
            output[i][j] = cmpx_t(0.0, 0.0);
        }
    }
    
    // Set DC component at center (64, 64)
    input[64][64] = cmpx_t(1.0, 0.0);
    
    cout << "Input: 2D impulse at (64, 64)" << endl;
    cout << "Input[64][64] = " << input[64][64] << endl;
    
    // Run forward FFT
    cout << "\nRunning forward FFT..." << endl;
    fft_2d_test_24bit(input, output, false);
    
    cout << "Output[0][0] (DC after FFT) = " << output[0][0] << endl;
    cout << "Expected: (1.0, 0.0)" << endl;
    
    // Run inverse FFT
    cmpx_t reconstructed[128][128];
    cout << "\nRunning inverse FFT..." << endl;
    fft_2d_test_24bit(output, reconstructed, true);
    
    cout << "Reconstructed[64][64] = " << reconstructed[64][64] << endl;
    cout << "Expected: (1.0, 0.0)" << endl;
    
    // Check for NaN (ap_fixed doesn't support std::isnan, check for unusual values)
    bool has_error = false;
    for (int i = 0; i < 128; i++) {
        for (int j = 0; j < 128; j++) {
            // For ap_fixed, check if value is outside expected range
            float real_val = reconstructed[i][j].real();
            float imag_val = reconstructed[i][j].imag();
            
            // Check for NaN by comparing with itself (NaN != NaN)
            if (real_val != real_val || imag_val != imag_val) {
                has_error = true;
                cout << "ERROR: NaN detected at (" << i << ", " << j << ")" << endl;
                break;
            }
        }
        if (has_error) break;
    }
    
    if (has_error) {
        cout << "\n[FAIL] NaN detected in output!" << endl;
        return 1;
    }
    
    // Calculate RMSE
    float rmse = 0.0;
    for (int i = 0; i < 128; i++) {
        for (int j = 0; j < 128; j++) {
            float diff_real = reconstructed[i][j].real() - input[i][j].real();
            float diff_imag = reconstructed[i][j].imag() - input[i][j].imag();
            rmse += diff_real * diff_real + diff_imag * diff_imag;
        }
    }
    rmse = sqrt(rmse / (128 * 128));
    
    cout << "\nRMSE: " << rmse << endl;
    
    if (rmse < 1e-5) {
        cout << "[PASS] 2D FFT test passed!" << endl;
        return 0;
    } else {
        cout << "[FAIL] RMSE too high!" << endl;
        return 1;
    }
}
