/**
 * Simple FFT Test for HLS Integration
 * FPGA-Litho Project
 * 
 * Purpose: Quick validation of FFT implementation
 */

#include "../src/socs_2048.h"
#include <cstdio>
#include <cmath>

#define TEST_SIZE 128

int main() {
    std::printf("============================================\n");
    std::printf("Simple FFT Validation Test\n");
    std::printf("============================================\n");
    
    // Test signal: impulse at center
    cmpx_2048_t signal[TEST_SIZE];
    cmpx_2048_t fft_result[TEST_SIZE];
    cmpx_2048_t ifft_result[TEST_SIZE];
    
    // Initialize impulse
    for (int i = 0; i < TEST_SIZE; i++) {
        signal[i] = cmpx_2048_t(0.0f, 0.0f);
    }
    signal[0] = cmpx_2048_t(1.0f, 0.0f);
    
    std::printf("Test 1: Forward FFT on impulse\n");
    
    // Run FFT
    fft_1d_direct_2048(signal, fft_result, false);
    
    // Check result (impulse FFT should be uniform magnitude = 1.0)
    float max_mag = 0.0f;
    float min_mag = 1e10f;
    for (int i = 0; i < TEST_SIZE; i++) {
        float mag = std::abs(fft_result[i]);
        max_mag = std::max(max_mag, mag);
        min_mag = std::min(min_mag, mag);
    }
    
    std::printf("  FFT magnitude range: [%f, %f] (expected: 1.0 for impulse)\n", min_mag, max_mag);
    
    std::printf("Test 2: Inverse FFT (FFTW BACKWARD scaling)\n");
    
    // Run IFFT
    fft_1d_direct_2048(fft_result, ifft_result, true);
    
    // Check reconstruction
    float recovered = ifft_result[0].real();
    float error = std::abs(recovered - 1.0f);
    
    std::printf("  Recovered impulse: %.4f (expected: 1.0)\n", recovered);
    std::printf("  Error: %.4e\n", error);
    
    // FFTW BACKWARD: IFFT should apply 1/N scaling
    // So recovered impulse = 1.0 (not TEST_SIZE)
    
    if (error < 1e-5) {
        std::printf("[PASS] FFTW BACKWARD scaling verified!\n");
        return 0;
    } else {
        std::printf("[FAIL] Scaling mismatch!\n");
        return 1;
    }
}