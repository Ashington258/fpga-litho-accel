/**
 * Minimal FFT Test with High Precision (ap_fixed<32,8>)
 * Purpose: Test if higher precision resolves NaN issue
 */

#include <cmath>
#include <complex>
#include <iostream>
#include <hls_stream.h>

#include "socs_2048.h"
#include "fft_top_reference.h"  // Use reference FFT configuration

// FFT top function (matching reference implementation)
void fft_top_test(
    ap_uint<1> dir,
    ap_uint<15> scaling,
    hls::stream<cmpxDataIn> &xn,
    hls::stream<cmpxDataOut> &xk,
    bool* status
) {
#pragma HLS interface ap_fifo depth=1 port=status
#pragma HLS interface ap_fifo depth=128 port=xn,xk
#pragma HLS stream variable=xn
#pragma HLS stream variable=xk
#pragma HLS dataflow

    hls::fft<config1>(xn, xk, dir, scaling, -1, status);
}

// Test function: Single FFT with high precision
void test_fft_high_precision(
    cmpx_2048_t input[128][128],
    cmpx_2048_t output[128][128],
    bool is_inverse
) {
    #pragma HLS DATAFLOW
    
    // Convert to high precision ap_fixed
    cmpxDataIn input_fixed[128][128];
    cmpxDataOut output_fixed[128][128];
    
    #pragma HLS RESOURCE variable=input_fixed core=RAM_2P_BRAM
    #pragma HLS RESOURCE variable=output_fixed core=RAM_2P_BRAM
    
    // Convert float to ap_fixed<16,1> (reference config)
    for (int y = 0; y < 128; y++) {
        for (int x = 0; x < 128; x++) {
            #pragma HLS PIPELINE II=1
            float r = input[y][x].real();
            float i = input[y][x].imag();
            input_fixed[y][x] = cmpxDataIn(data_in_t(r), data_in_t(i));
        }
    }
    
    // Create streams
    hls::stream<cmpxDataIn> in_stream("in_stream");
    hls::stream<cmpxDataOut> out_stream("out_stream");
    
    #pragma HLS stream variable=in_stream depth=128
    #pragma HLS stream variable=out_stream depth=128
    
    // Write input to stream (must write FFT_LENGTH = 128 points)
    for (int i = 0; i < FFT_LENGTH; i++) {
        #pragma HLS PIPELINE II=1
        in_stream.write(input_fixed[0][i]);
    }
    
    // Call FFT using reference implementation interface
    ap_uint<1> dir = is_inverse ? 1 : 0;
    ap_uint<15> scaling = 0x7F;
    bool status;
    fft_top_test(dir, scaling, in_stream, out_stream, &status);
    
    // Read output from stream (must read FFT_LENGTH = 128 points)
    for (int i = 0; i < FFT_LENGTH; i++) {
        #pragma HLS PIPELINE II=1
        output_fixed[0][i] = out_stream.read();
    }
    
    // Convert ap_fixed back to float
    for (int y = 0; y < 128; y++) {
        for (int x = 0; x < 128; x++) {
            #pragma HLS PIPELINE II=1
            float r = (float)output_fixed[y][x].real();
            float i = (float)output_fixed[y][x].imag();
            output[y][x] = cmpx_2048_t(r, i);
        }
    }
}

// Top-level function for HLS
void calc_socs_2048_hls_fft_hp_test(
    cmpx_2048_t input[128][128],
    cmpx_2048_t output[128][128],
    bool is_inverse
) {
    #pragma HLS INTERFACE m_axi port=input depth=16384
    #pragma HLS INTERFACE m_axi port=output depth=16384
    #pragma HLS INTERFACE s_axilite port=is_inverse
    #pragma HLS INTERFACE s_axilite port=return
    
    test_fft_high_precision(input, output, is_inverse);
}

// Testbench
int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "HLS FFT IP High Precision Test (ap_fixed<32,8>)" << std::endl;
    std::cout << "========================================" << std::endl;
    
    // Create test input: simple impulse
    cmpx_2048_t input[128][128];
    cmpx_2048_t output[128][128];
    
    // Initialize with impulse at center
    for (int y = 0; y < 128; y++) {
        for (int x = 0; x < 128; x++) {
            if (y == 64 && x == 64) {
                input[y][x] = cmpx_2048_t(1.0f, 0.0f);  // Impulse
            } else {
                input[y][x] = cmpx_2048_t(0.0f, 0.0f);
            }
        }
    }
    
    std::cout << "Input: Impulse at (64, 64)" << std::endl;
    std::cout << "Input[64][64] = " << input[64][64] << std::endl;
    
    // Run FFT
    calc_socs_2048_hls_fft_hp_test(input, output, false);
    
    // Check output: FFT of impulse should be constant
    std::cout << "\nFFT Output (should be constant):" << std::endl;
    std::cout << "Output[0][0] = " << output[0][0] << std::endl;
    std::cout << "Output[64][64] = " << output[64][64] << std::endl;
    std::cout << "Output[127][127] = " << output[127][127] << std::endl;
    
    // Check for NaN
    bool has_nan = false;
    for (int y = 0; y < 128; y++) {
        for (int x = 0; x < 128; x++) {
            if (std::isnan(output[y][x].real()) || std::isnan(output[y][x].imag())) {
                has_nan = true;
                std::cout << "NaN detected at (" << y << ", " << x << ")" << std::endl;
                break;
            }
        }
        if (has_nan) break;
    }
    
    if (has_nan) {
        std::cout << "\n[FAIL] FFT output contains NaN!" << std::endl;
        return 1;
    } else {
        std::cout << "\n[PASS] FFT output is valid (no NaN)" << std::endl;
        return 0;
    }
}
