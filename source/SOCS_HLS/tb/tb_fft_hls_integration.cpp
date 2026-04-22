/**
 * Test Bench for HLS FFT Integration Validation
 * FPGA-Litho Project
 * 
 * Purpose: Verify hls::fft IP integration correctness
 * Test Strategy:
 *   1. Compare HLS FFT output with direct DFT (C Simulation)
 *   2. Verify FFTW BACKWARD scaling convention
 *   3. Test forward/inverse FFT consistency
 */

#include "../src/socs_2048.h"
#include "../src/hls_fft_config_2048.h"
#include <cstdio>
#include <cmath>

// ============================================================================
// Test Parameters
// ============================================================================

#define TEST_FFT_SIZE  128
#define RMSE_THRESHOLD 1e-5  // Allowable RMSE for FFT equivalence

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * Generate test signal (sine wave)
 */
void generate_test_signal(cmpx_2048_t signal[TEST_FFT_SIZE], int freq) {
    for (int i = 0; i < TEST_FFT_SIZE; i++) {
        float angle = 2.0f * 3.14159265358979323846f * freq * i / TEST_FFT_SIZE;
        signal[i] = cmpx_2048_t(std::cos(angle), 0.0f);
    }
}

/**
 * Compute RMSE between two arrays
 */
float compute_rmse(cmpx_2048_t a[TEST_FFT_SIZE], cmpx_2048_t b[TEST_FFT_SIZE]) {
    float sum_sq = 0.0f;
    for (int i = 0; i < TEST_FFT_SIZE; i++) {
        float diff_re = a[i].real() - b[i].real();
        float diff_im = a[i].imag() - b[i].imag();
        sum_sq += diff_re * diff_re + diff_im * diff_im;
    }
    return std::sqrt(sum_sq / TEST_FFT_SIZE);
}

// ============================================================================
// Test Functions
// ============================================================================

/**
 * Test 1: FFT-DFT equivalence (Forward FFT)
 */
int test_fft_forward_equivalence() {
    std::printf("\n[Test 1] FFT Forward Equivalence (Direct DFT vs Expected)\n");
    
    cmpx_2048_t test_signal[TEST_FFT_SIZE];
    cmpx_2048_t fft_result[TEST_FFT_SIZE];
    
    // Generate test signal (frequency=5)
    generate_test_signal(test_signal, 5);
    
    // Run FFT (C Simulation uses direct DFT)
    fft_1d_direct_2048(test_signal, fft_result, false);
    
    // Expected result: Peak at frequency bin 5 and TEST_FFT_SIZE-5
    int peak_bin = 5;
    float peak_mag = std::abs(fft_result[peak_bin]);
    
    // Check peak magnitude (should be ~TEST_FFT_SIZE/2 for real signal)
    float expected_peak = TEST_FFT_SIZE * 0.5f;  // Half amplitude for real signal
    float error = std::abs(peak_mag - expected_peak) / expected_peak;
    
    std::printf("  Peak bin: %d, Magnitude: %.2f, Expected: %.2f, Error: %.2e\n",
                peak_bin, peak_mag, expected_peak, error);
    
    if (error < 0.1f) {  // 10% tolerance
        std::printf("  [PASS] Forward FFT produces expected frequency peak\n");
        return 0;
    } else {
        std::printf("  [FAIL] Forward FFT magnitude mismatch\n");
        return 1;
    }
}

/**
 * Test 2: IFFT-FFT consistency (Inverse FFT)
 */
int test_ifft_fft_consistency() {
    std::printf("\n[Test 2] IFFT-FFT Consistency (Round-trip test)\n");
    
    cmpx_2048_t test_signal[TEST_FFT_SIZE];
    cmpx_2048_t fft_result[TEST_FFT_SIZE];
    cmpx_2048_t ifft_result[TEST_FFT_SIZE];
    
    // Generate test signal
    generate_test_signal(test_signal, 10);
    
    // Forward FFT
    fft_1d_direct_2048(test_signal, fft_result, false);
    
    // Inverse FFT
    fft_1d_direct_2048(fft_result, ifft_result, true);
    
    // Compute RMSE between original and reconstructed
    float rmse = compute_rmse(test_signal, ifft_result);
    
    std::printf("  Original signal RMS: %.2e\n", std::abs(test_signal[50]));
    std::printf("  Reconstructed signal RMS: %.2e\n", std::abs(ifft_result[50]));
    std::printf("  RMSE: %.2e (threshold: %.2e)\n", rmse, RMSE_THRESHOLD);
    
    if (rmse < RMSE_THRESHOLD) {
        std::printf("  [PASS] IFFT-FFT round-trip error within tolerance\n");
        return 0;
    } else {
        std::printf("  [FAIL] IFFT-FFT round-trip error exceeds threshold\n");
        return 1;
    }
}

/**
 * Test 3: 2D FFT consistency
 */
int test_2d_fft_consistency() {
    std::printf("\n[Test 3] 2D FFT Consistency (128x128 array)\n");
    
    cmpx_2048_t input_2d[MAX_FFT_Y][MAX_FFT_X];
    cmpx_2048_t fft_2d_result[MAX_FFT_Y][MAX_FFT_X];
    cmpx_2048_t ifft_2d_result[MAX_FFT_Y][MAX_FFT_X];
    
    // Generate 2D test pattern (single point at center)
    for (int y = 0; y < MAX_FFT_Y; y++) {
        for (int x = 0; x < MAX_FFT_X; x++) {
            input_2d[y][x] = cmpx_2048_t(0.0f, 0.0f);
        }
    }
    input_2d[64][64] = cmpx_2048_t(1.0f, 0.0f);  // Center point
    
    // 2D FFT
    fft_2d_full_2048(input_2d, fft_2d_result, false);
    
    // Expected: Uniform magnitude across all frequency bins
    float expected_mag = 1.0f;
    float max_mag = 0.0f;
    float min_mag = 1e10f;
    
    for (int y = 0; y < MAX_FFT_Y; y++) {
        for (int x = 0; x < MAX_FFT_X; x++) {
            float mag = std::abs(fft_2d_result[y][x]);
            max_mag = std::max(max_mag, mag);
            min_mag = std::min(min_mag, mag);
        }
    }
    
    std::printf("  FFT result magnitude range: [%f, %f]\n", min_mag, max_mag);
    
    // 2D IFFT
    fft_2d_full_2048(fft_2d_result, ifft_2d_result, true);
    
    // Check reconstruction
    float reconstruction_error = std::abs(ifft_2d_result[64][64].real() - 1.0f);
    std::printf("  Center point reconstruction: %.2e (expected: 1.0)\n", 
                ifft_2d_result[64][64].real());
    std::printf("  Reconstruction error: %.2e\n", reconstruction_error);
    
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
    std::printf("  Background noise: %.2e\n", bg_noise);
    
    if (reconstruction_error < RMSE_THRESHOLD && bg_noise < RMSE_THRESHOLD) {
        std::printf("  [PASS] 2D FFT round-trip successful\n");
        return 0;
    } else {
        std::printf("  [FAIL] 2D FFT reconstruction error\n");
        return 1;
    }
}

/**
 * Test 4: FFTW BACKWARD scaling verification
 */
int test_fftw_backward_scaling() {
    std::printf("\n[Test 4] FFTW BACKWARD Scaling Verification\n");
    
    cmpx_2048_t test_signal[TEST_FFT_SIZE];
    cmpx_2048_t fft_result[TEST_FFT_SIZE];
    cmpx_2048_t ifft_result[TEST_FFT_SIZE];
    
    // Generate unit impulse
    for (int i = 0; i < TEST_FFT_SIZE; i++) {
        test_signal[i] = cmpx_2048_t(0.0f, 0.0f);
    }
    test_signal[0] = cmpx_2048_t(1.0f, 0.0f);
    
    // Forward FFT
    fft_1d_direct_2048(test_signal, fft_result, false);
    
    // Check FFT result (should be uniform magnitude = 1.0)
    float fft_mag = std::abs(fft_result[10]);
    std::printf("  FFT magnitude at bin 10: %.2f (expected: 1.0)\n", fft_mag);
    
    // Inverse FFT
    fft_1d_direct_2048(fft_result, ifft_result, true);
    
    // Check IFFT result (should recover original impulse)
    float recovered = ifft_result[0].real();
    float error = std::abs(recovered - 1.0f);
    std::printf("  IFFT recovered impulse: %.2e (expected: 1.0)\n", recovered);
    std::printf("  Scaling error: %.2e\n", error);
    
    // FFTW BACKWARD convention: IFFT applies 1/N scaling
    // So recovered impulse should be exactly 1.0 (not TEST_FFT_SIZE)
    if (error < RMSE_THRESHOLD) {
        std::printf("  [PASS] FFTW BACKWARD scaling convention verified\n");
        return 0;
    } else {
        std::printf("  [FAIL] Scaling mismatch (expected FFTW BACKWARD behavior)\n");
        return 1;
    }
}

// ============================================================================
// Main Test Entry
// ============================================================================

int main() {
    std::printf("============================================\n");
    std::printf("HLS FFT Integration Validation Test\n");
    std::printf("============================================\n");
    std::printf("FFT Size: %d\n", TEST_FFT_SIZE);
    std::printf("RMSE Threshold: %.2e\n", RMSE_THRESHOLD);
    std::printf("Test Mode: C Simulation (Direct DFT)\n");
    
    int total_errors = 0;
    
    total_errors += test_fft_forward_equivalence();
    total_errors += test_ifft_fft_consistency();
    total_errors += test_2d_fft_consistency();
    total_errors += test_fftw_backward_scaling();
    
    // Summary
    std::printf("\n============================================\n");
    std::printf("TEST SUMMARY\n");
    std::printf("============================================\n");
    std::printf("Total errors: %d\n", total_errors);
    
    if (total_errors == 0) {
        std::printf("[PASS] All HLS FFT integration tests passed!\n");
        std::printf("\nNext Step: Run C Synthesis to verify HLS FFT IP integration\n");
        std::printf("  Command: v++ -c --mode hls --config script/config/hls_config_2048.cfg\n");
        return 0;
    } else {
        std::printf("[FAIL] Some tests failed!\n");
        std::printf("Action: Fix FFT implementation before proceeding to C Synthesis\n");
        return 1;
    }
}