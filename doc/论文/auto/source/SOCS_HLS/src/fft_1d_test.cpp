/**
 * Simple 1D FFT Test (128-point)
 * Purpose: Verify HLS FFT IP works correctly
 */

#include <cmath>
#include <complex>
#include <iostream>
#include <hls_stream.h>

#include "fft_top_reference.h"  // Use reference FFT configuration

// Top-level function for HLS
void fft_1d_test(
    hls::stream<cmpxDataIn> &xn,
    hls::stream<cmpxDataOut> &xk,
    bool is_inverse
) {
    #pragma HLS INTERFACE axis port=xn
    #pragma HLS INTERFACE axis port=xk
    #pragma HLS INTERFACE s_axilite port=is_inverse
    #pragma HLS INTERFACE s_axilite port=return
    
    ap_uint<1> dir = is_inverse ? 1 : 0;
    ap_uint<15> scaling = 0x7F;
    bool status;
    
    hls::fft<config1>(xn, xk, dir, scaling, -1, &status);
}

// Testbench
int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "HLS FFT IP 1D Test (128-point)" << std::endl;
    std::cout << "========================================" << std::endl;
    
    // Create test input: simple impulse
    hls::stream<cmpxDataIn> input_stream("input_stream");
    hls::stream<cmpxDataOut> output_stream("output_stream");
    
    // Write impulse: 1.0 at index 0, 0 elsewhere
    for (int i = 0; i < FFT_LENGTH; i++) {
        cmpxDataIn sample;
        if (i == 0) {
            sample = cmpxDataIn(data_in_t(1.0), data_in_t(0.0));  // Impulse
        } else {
            sample = cmpxDataIn(data_in_t(0.0), data_in_t(0.0));
        }
        input_stream.write(sample);
    }
    
    std::cout << "Input: Impulse at index 0" << std::endl;
    
    // Run FFT
    fft_1d_test(input_stream, output_stream, false);
    
    // Read output: FFT of impulse should be constant (all 1.0)
    std::cout << "\nFFT Output (should be constant ~1.0):" << std::endl;
    
    // Check for NaN and print first 8 samples
    bool has_nan = false;
    int sample_count = 0;
    
    while (!output_stream.empty()) {
        cmpxDataOut sample = output_stream.read();
        
        if (sample_count < 8) {
            std::cout << "Output[" << sample_count << "] = (" 
                      << (float)sample.real() << ", " 
                      << (float)sample.imag() << ")" << std::endl;
        }
        
        if (std::isnan((float)sample.real()) || std::isnan((float)sample.imag())) {
            has_nan = true;
            std::cout << "NaN detected at index " << sample_count << std::endl;
        }
        
        sample_count++;
    }
    
    std::cout << "\nTotal samples read: " << sample_count << std::endl;
    
    if (has_nan) {
        std::cout << "\n[FAIL] FFT output contains NaN" << std::endl;
        return 1;
    } else {
        std::cout << "\n[PASS] FFT output is valid (no NaN)" << std::endl;
        return 0;
    }
}
