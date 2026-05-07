/**
 * Minimal FFT Test for Co-Simulation Debug
 * Purpose: Test HLS FFT IP in isolation
 */

#include <cmath>
#include <complex>
#include <fstream>
#include <iostream>
#include <hls_stream.h>

#include "socs_2048.h"
#include "hls_fft_config_2048_corrected.h"

// Test function: Single FFT with known input
void test_fft_single(
    cmpx_2048_t input[128][128],
    cmpx_2048_t output[128][128],
    bool is_inverse
) {
    #pragma HLS DATAFLOW
    
    // Convert to ap_fixed
    cmpx_fft_in_t input_fixed[128][128];
    cmpx_fft_out_t output_fixed[128][128];
    
    #pragma HLS RESOURCE variable=input_fixed core=RAM_2P_BRAM
    #pragma HLS RESOURCE variable=output_fixed core=RAM_2P_BRAM
    
    // Convert float to ap_fixed
    for (int y = 0; y < 128; y++) {
        for (int x = 0; x < 128; x++) {
            #pragma HLS PIPELINE II=1
            float r = input[y][x].real();
            float i = input[y][x].imag();
            input_fixed[y][x] = cmpx_fft_in_t(data_fft_in_t(r), data_fft_in_t(i));
        }
    }
    
    // Call HLS FFT IP
    fft_2d_hls_128(input_fixed, output_fixed, is_inverse);
    
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
void calc_socs_2048_hls_fft_test(
    cmpx_2048_t input[128][128],
    cmpx_2048_t output[128][128],
    bool is_inverse
) {
    #pragma HLS INTERFACE m_axi port=input depth=16384
    #pragma HLS INTERFACE m_axi port=output depth=16384
    #pragma HLS INTERFACE s_axilite port=is_inverse
    #pragma HLS INTERFACE s_axilite port=return
    
    test_fft_single(input, output, is_inverse);
}

// Testbench
int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "HLS FFT IP Minimal Test" << std::endl;
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
    calc_socs_2048_hls_fft_test(input, output, false);
    
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
