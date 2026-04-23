/**
 * Minimal FFT Debug Test - Check NaN/inf propagation in 1D FFT
 */

#include <iostream>
#include <cmath>
#include "../src/socs_config_optimized.h"

// Direct FFT IP call (bypassing fft_2d_optimized)
void fft_1d_direct(cmpx_fft_t input[FFT_LENGTH], cmpx_fft_t output[FFT_LENGTH], int dir) {
    hls::ip_fft::config_t<fft_config_socs> fft_config;
    hls::ip_fft::status_t<fft_config_socs> fft_status;
    fft_config.setDir(dir);  // 0=forward, 1=inverse
    hls::fft<fft_config_socs>(input, output, &fft_status, &fft_config);
}

int main() {
    std::cout << "=== FFT 1D Debug Test ===" << std::endl;
    
    cmpx_fft_t input[FFT_LENGTH];
    cmpx_fft_t output[FFT_LENGTH];
    
    // Test 1: All zeros input
    std::cout << "\n[Test 1] All zeros input (32-point FFT)" << std::endl;
    for (int i = 0; i < FFT_LENGTH; i++) {
        input[i] = cmpx_fft_t(0.0f, 0.0f);
        output[i] = cmpx_fft_t(0.0f, 0.0f);  // Initialize output
    }
    
    fft_1d_direct(input, output, 1);  // IFFT
    
    bool has_nan = false;
    bool has_inf = false;
    float max_val = 0.0f;
    
    for (int i = 0; i < FFT_LENGTH; i++) {
        float re = output[i].real();
        float im = output[i].imag();
        if (std::isnan(re) || std::isnan(im)) has_nan = true;
        if (std::isinf(re) || std::isinf(im)) has_inf = true;
        float mag = std::sqrt(re*re + im*im);
        if (mag > max_val) max_val = mag;
    }
    
    std::cout << "  Output stats: max_val=" << max_val << ", has_nan=" << has_nan << ", has_inf=" << has_inf << std::endl;
    
    if (!has_nan && !has_inf && max_val == 0.0f) {
        std::cout << "  ✓ PASS - Zero input → zero output" << std::endl;
    } else {
        std::cout << "  ✗ FAIL - Unexpected output!" << std::endl;
        std::cout << "\n  First 5 output values:" << std::endl;
        for (int i = 0; i < 5; i++) {
            float re = output[i].real();
            float im = output[i].imag();
            std::cout << "    output[" << i << "] = (" << re << ", " << im << ")" << std::endl;
        }
    }
    
    // Test 2: DC component only (index 0)
    std::cout << "\n[Test 2] DC component only (input[0]=1.0)" << std::endl;
    for (int i = 0; i < FFT_LENGTH; i++) {
        input[i] = cmpx_fft_t(0.0f, 0.0f);
        output[i] = cmpx_fft_t(0.0f, 0.0f);
    }
    input[0] = cmpx_fft_t(1.0f, 0.0f);  // DC component
    
    fft_1d_direct(input, output, 1);  // IFFT
    
    has_nan = false;
    has_inf = false;
    max_val = 0.0f;
    
    for (int i = 0; i < FFT_LENGTH; i++) {
        float re = output[i].real();
        float im = output[i].imag();
        if (std::isnan(re) || std::isnan(im)) has_nan = true;
        if (std::isinf(re) || std::isinf(im)) has_inf = true;
        float mag = std::sqrt(re*re + im*im);
        if (mag > max_val) max_val = mag;
    }
    
    std::cout << "  Output stats: max_val=" << max_val << ", has_nan=" << has_nan << ", has_inf=" << has_inf << std::endl;
    
    // For unscaled IFFT: DC input should give constant output = 1.0/32 = 0.03125
    float expected_dc = 1.0f / FFT_LENGTH;
    bool pass = !has_nan && !has_inf && (max_val > 0.02f && max_val < 0.04f);
    
    if (pass) {
        std::cout << "  ✓ PASS - DC output ≈ " << expected_dc << std::endl;
    } else {
        std::cout << "  ✗ FAIL - Expected ≈ " << expected_dc << std::endl;
        std::cout << "\n  All output values:" << std::endl;
        for (int i = 0; i < FFT_LENGTH; i++) {
            float re = output[i].real();
            float im = output[i].imag();
            std::cout << "    output[" << i << "] = (" << re << ", " << im << ")" << std::endl;
        }
        return 1;
    }
    
    std::cout << "\n=== All Tests Passed ===" << std::endl;
    return 0;
}