/**
 * 2D FFT Validation Test
 * FPGA-Litho Project
 */

#include "../src/socs_2048.h"
#include <cstdio>
#include <cmath>

#define TEST_SIZE 128

int main() {
    std::printf("============================================\n");
    std::printf("2D FFT Validation Test\n");
    std::printf("============================================\n");
    
    // Test 1: 2D impulse → uniform frequency domain
    cmpx_2048_t impulse_2d[MAX_FFT_Y][MAX_FFT_X];
    cmpx_2048_t fft_2d_result[MAX_FFT_Y][MAX_FFT_X];
    
    for (int y = 0; y < MAX_FFT_Y; y++) {
        for (int x = 0; x < MAX_FFT_X; x++) {
            impulse_2d[y][x] = cmpx_2048_t(0.0f, 0.0f);
        }
    }
    impulse_2d[64][64] = cmpx_2048_t(1.0f, 0.0f);  // Center impulse
    
    std::printf("Test 1: 2D FFT on centered impulse\n");
    fft_2d_full_2048(impulse_2d, fft_2d_result, false);
    
    // Check frequency domain (should be uniform magnitude)
    float max_mag = 0.0f;
    float min_mag = 1e10f;
    for (int y = 0; y < 10; y++) {
        for (int x = 0; x < 10; x++) {
            float mag = std::abs(fft_2d_result[y][x]);
            max_mag = std::max(max_mag, mag);
            min_mag = std::min(min_mag, mag);
        }
    }
    
    std::printf("  FFT magnitude range: [%f, %f] (expected: ~1.0)\n", min_mag, max_mag);
    
    // Test 2: 2D IFFT round-trip
    cmpx_2048_t ifft_2d_result[MAX_FFT_Y][MAX_FFT_X];
    
    std::printf("Test 2: 2D IFFT round-trip\n");
    fft_2d_full_2048(fft_2d_result, ifft_2d_result, true);
    
    // Check reconstruction
    float center_value = ifft_2d_result[64][64].real();
    float error = std::abs(center_value - 1.0f);
    
    std::printf("  Center point reconstruction: %.6f (expected: 1.0)\n", center_value);
    std::printf("  Reconstruction error: %.6e\n", error);
    
    // Check background noise
    float bg_noise = 0.0f;
    for (int y = 0; y < MAX_FFT_Y; y++) {
        for (int x = 0; x < MAX_FFT_X; x++) {
            if (y != 64 && x != 64) {
                bg_noise += std::abs(ifft_2d_result[y][x].real());
            }
        }
    }
    bg_noise /= (MAX_FFT_Y * MAX_FFT_X - 1);
    
    std::printf("  Background noise: %.6e\n", bg_noise);
    
    if (error < 1e-5 && bg_noise < 1e-5) {
        std::printf("[PASS] 2D FFT validation successful!\n");
        return 0;
    } else {
        std::printf("[FAIL] 2D FFT reconstruction error!\n");
        return 1;
    }
}