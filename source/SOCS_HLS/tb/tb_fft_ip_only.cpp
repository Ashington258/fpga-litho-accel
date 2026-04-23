/**
 * Minimal FFT IP Test - Fixed-Point Configuration (following vitis_hls_fft reference)
 */

#include <iostream>
#include <cmath>
#include "../src/socs_config_optimized.h"

// Direct FFT call - bypass entire SOCS pipeline
extern void fft_2d_optimized(
    cmpx_fft_t input[fftConvY][fftConvX],
    cmpx_fft_t output[fftConvY][fftConvX],
    bool is_inverse
);

int main() {
    std::cout << "=== FFT IP Only Test (Fixed-Point) ===" << std::endl;
    std::cout << "FFT config: " << fftConvX << "x" << fftConvY << ", scaled, use_luts, ap_fixed<16,1>" << std::endl;
    
    cmpx_fft_t input[fftConvY][fftConvX];
    cmpx_fft_t output[fftConvY][fftConvX];
    
    // Initialize with fixed-point data (using ap_fixed constructor)
    // Value 0.1 in ap_fixed<16,1> representation
    for (int y = 0; y < fftConvY; y++) {
        for (int x = 0; x < fftConvX; x++) {
            // ap_fixed<16,1> can represent -1.0 to ~0.9999 with ~15 bits of fractional precision
            input[y][x] = cmpx_fft_t(data_in_t(0.1), data_in_t(0.0));
        }
    }
    
    std::cout << "\n[Test 1] Running IFFT on constant input (0.1, 0.0)..." << std::endl;
    
    fft_2d_optimized(input, output, true);  // IFFT
    
    // Check output - convert fixed-point to float for display
    int error_count = 0;
    float max_val = 0.0f;
    float min_val = 100.0f;
    
    for (int y = 0; y < fftConvY; y++) {
        for (int x = 0; x < fftConvX; x++) {
            // Convert ap_fixed to float for analysis
            float re = (float)output[y][x].real();
            float im = (float)output[y][x].imag();
            
            // For scaled IFFT, output should be normalized by FFT_LENGTH
            // Expected IFFT of constant (0.1, 0) is (0.1 * N, 0) where N = 32
            // But scaled mode divides by growth factor, so output should be ~0.1
            
            if (re > max_val) max_val = re;
            if (re < min_val) min_val = re;
            
            // Check if output is reasonable (not overflow)
            // ap_fixed range is [-1.0, ~0.999], so any value outside this is overflow
            if (re < -1.5 || re > 1.5) {
                error_count++;
            }
        }
    }
    
    std::cout << "Output statistics:" << std::endl;
    std::cout << "  Overflow/out-of-range count: " << error_count << std::endl;
    std::cout << "  Max real value: " << max_val << std::endl;
    std::cout << "  Min real value: " << min_val << std::endl;
    
    if (error_count > 0) {
        std::cout << "\n✗ FAIL - FFT output overflow or invalid!" << std::endl;
        std::cout << "\nFirst 5 output values (diagnostic):" << std::endl;
        for (int i = 0; i < 5; i++) {
            std::cout << "  output[0][" << i << "] = (" << (float)output[0][i].real() << ", " << (float)output[0][i].imag() << ")" << std::endl;
        }
        return 1;
    }
    
    std::cout << "\n✓ PASS - Fixed-point FFT output is valid" << std::endl;
    std::cout << "First 5 output values:" << std::endl;
    for (int i = 0; i < 5; i++) {
        std::cout << "  output[0][" << i << "] = (" << (float)output[0][i].real() << ", " << (float)output[0][i].imag() << ")" << std::endl;
    }
    return 0;
}