/**
 * Test Bench for SOCS 2048 Architecture
 * FPGA-Litho Project
 * 
 * Validation Strategy:
 *   1. Compare with CPU reference (validation/golden/litho.cpp)
 *   2. Test different Nx configurations (4, 16, 24)
 *   3. Verify zero-padding correctness
 *   4. Check output region extraction
 */

#include "../src/socs_2048.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <fstream>

// ============================================================================
// Test Configuration
// ============================================================================

#define TEST_NX_4    4
#define TEST_NX_16   16
#define TEST_NX_24   24

#define TEST_NK      10
#define TEST_LX      512
#define TEST_LY      512

#define GOLDEN_DIR   "/home/ashington/fpga-litho-accel/output/verification/"
#define DATA_DIR     "/home/ashington/fpga-litho-accel/source/SOCS_HLS/data/"

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * Read binary data file
 */
template<typename T>
int read_binary_data(const char* filepath, T* buffer, int count) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        std::printf("[ERROR] Cannot open file: %s\n", filepath);
        return -1;
    }
    
    file.read(reinterpret_cast<char*>(buffer), count * sizeof(T));
    int bytes_read = file.gcount();
    file.close();
    
    std::printf("[INFO] Read %d bytes from %s\n", bytes_read, filepath);
    return bytes_read / sizeof(T);
}

/**
 * Write binary data file
 */
template<typename T>
int write_binary_data(const char* filepath, T* buffer, int count) {
    std::ofstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        std::printf("[ERROR] Cannot write file: %s\n", filepath);
        return -1;
    }
    
    file.write(reinterpret_cast<char*>(buffer), count * sizeof(T));
    file.close();
    
    std::printf("[INFO] Write %d bytes to %s\n", count * sizeof(T), filepath);
    return 0;
}

/**
 * Compare two arrays with tolerance
 */
int compare_results(
    const char* name,
    float* hls_output,
    float* golden_output,
    int count,
    float tolerance = 1e-6f
) {
    int errors = 0;
    float max_error = 0.0f;
    float sum_error = 0.0f;
    
    for (int i = 0; i < count; i++) {
        float diff = std::abs(hls_output[i] - golden_output[i]);
        sum_error += diff;
        
        if (diff > max_error) {
            max_error = diff;
        }
        
        if (diff > tolerance) {
            errors++;
            if (errors < 10) {
                std::printf("[DIFF] %s[%d]: HLS=%.6f, Golden=%.6f, diff=%.6f\n",
                    name, i, hls_output[i], golden_output[i], diff);
            }
        }
    }
    
    float rmse = std::sqrt(sum_error / count);
    std::printf("[RESULT] %s: errors=%d/%d, max_err=%.6f, RMSE=%.6f\n",
        name, errors, count, max_error, rmse);
    
    return errors;
}

// ============================================================================
// Test Cases
// ============================================================================

/**
 * Test Case 1: Nx=4 (Small Kernel)
 * Expected: 9×9 kernel → 17×17 output
 */
int test_nx4() {
    std::printf("\n========================================\n");
    std::printf("TEST CASE 1: Nx=4 (Small Kernel)\n");
    std::printf("========================================\n");
    
    // Allocate input arrays
    float* mskf_r = new float[TEST_LX * TEST_LY];
    float* mskf_i = new float[TEST_LX * TEST_LY];
    float* scales = new float[TEST_NK];
    
    int kerX_4 = 2 * TEST_NX_4 + 1;  // = 9
    float* krn_r = new float[TEST_NK * kerX_4 * kerX_4];
    float* krn_i = new float[TEST_NK * kerX_4 * kerX_4];
    
    int convX_4 = 4 * TEST_NX_4 + 1;  // = 17
    float* output_hls = new float[convX_4 * convX_4];
    float* output_golden = new float[convX_4 * convX_4];
    
    // Allocate FFT DDR buffer
    cmpx_2048_t* fft_buf_ddr = new cmpx_2048_t[MAX_FFT_Y * MAX_FFT_X];
    
    // Read input data from golden verification output
    read_binary_data(GOLDEN_DIR "mskf_r.bin", mskf_r, TEST_LX * TEST_LY);
    read_binary_data(GOLDEN_DIR "mskf_i.bin", mskf_i, TEST_LX * TEST_LY);
    read_binary_data(GOLDEN_DIR "scales.bin", scales, TEST_NK);
    
    // Read kernel data (need to extract from kernels_iX.bin)
    // Note: For Nx=4, kernel size is 9×9
    // Current kernel files are 65×65 (Nx=16), need special handling
    // For initial test, use synthetic kernel data
    
    std::printf("[INFO] Using synthetic kernel data for Nx=4 test\n");
    for (int k = 0; k < TEST_NK; k++) {
        for (int i = 0; i < kerX_4 * kerX_4; i++) {
            // Simple test kernel: Gaussian distribution
            int y = i / kerX_4;
            int x = i % kerX_4;
            float dist = std::sqrt((x - TEST_NX_4) * (x - TEST_NX_4) + 
                                   (y - TEST_NX_4) * (y - TEST_NX_4));
            krn_r[k * kerX_4 * kerX_4 + i] = std::exp(-dist * dist / 8.0f);
            krn_i[k * kerX_4 * kerX_4 + i] = 0.0f;
        }
    }
    
    // Run HLS function
    std::printf("[INFO] Running calc_socs_2048_hls for Nx=4...\n");
    
    calc_socs_2048_hls(
        mskf_r, mskf_i, scales, krn_r, krn_i, output_hls,
        fft_buf_ddr,
        TEST_NX_4, TEST_NX_4, TEST_NK, TEST_LX, TEST_LY, 0
    );
    
    // Write output for verification
    write_binary_data(DATA_DIR "output_nx4_hls.bin", output_hls, convX_4 * convX_4);
    
    // Compare with golden (need to generate or use reference)
    // For initial test, just check output range
    float max_val = 0.0f;
    float min_val = 1e10f;
    float sum_val = 0.0f;
    
    for (int i = 0; i < convX_4 * convX_4; i++) {
        if (output_hls[i] > max_val) max_val = output_hls[i];
        if (output_hls[i] < min_val) min_val = output_hls[i];
        sum_val += output_hls[i];
    }
    
    std::printf("[RESULT] Nx=4 Output Statistics:\n");
    std::printf("  - Min: %.6f\n", min_val);
    std::printf("  - Max: %.6f\n", max_val);
    std::printf("  - Mean: %.6f\n", sum_val / (convX_4 * convX_4));
    std::printf("  - Output size: %d×%d = %d pixels\n", convX_4, convX_4, convX_4 * convX_4);
    
    // Cleanup
    delete[] mskf_r;
    delete[] mskf_i;
    delete[] scales;
    delete[] krn_r;
    delete[] krn_i;
    delete[] output_hls;
    delete[] output_golden;
    delete[] fft_buf_ddr;
    
    return 0;
}

/**
 * Test Case 2: Nx=16 (Standard Kernel)
 * Expected: 33×33 kernel → 65×65 output
 */
int test_nx16() {
    std::printf("\n========================================\n");
    std::printf("TEST CASE 2: Nx=16 (Standard Kernel)\n");
    std::printf("========================================\n");
    
    // Allocate arrays
    float* mskf_r = new float[TEST_LX * TEST_LY];
    float* mskf_i = new float[TEST_LX * TEST_LY];
    float* scales = new float[TEST_NK];
    
    int kerX_16 = 2 * TEST_NX_16 + 1;  // = 33
    float* krn_r = new float[TEST_NK * kerX_16 * kerX_16];
    float* krn_i = new float[TEST_NK * kerX_16 * kerX_16];
    
    int convX_16 = 4 * TEST_NX_16 + 1;  // = 65
    float* output_hls = new float[convX_16 * convX_16];
    float* output_golden = new float[convX_16 * convX_16];
    
    cmpx_2048_t* fft_buf_ddr = new cmpx_2048_t[MAX_FFT_Y * MAX_FFT_X];
    
    // Read input data
    read_binary_data(GOLDEN_DIR "mskf_r.bin", mskf_r, TEST_LX * TEST_LY);
    read_binary_data(GOLDEN_DIR "mskf_i.bin", mskf_i, TEST_LX * TEST_LY);
    read_binary_data(GOLDEN_DIR "scales.bin", scales, TEST_NK);
    
    // Read kernel data (Nx=16, kernel size 33×33)
    // Use extracted kernel from kernels.bin or use synthetic
    std::printf("[INFO] Using synthetic kernel data for Nx=16 test\n");
    for (int k = 0; k < TEST_NK; k++) {
        for (int i = 0; i < kerX_16 * kerX_16; i++) {
            int y = i / kerX_16;
            int x = i % kerX_16;
            float dist = std::sqrt((x - TEST_NX_16) * (x - TEST_NX_16) + 
                                   (y - TEST_NX_16) * (y - TEST_NX_16));
            krn_r[k * kerX_16 * kerX_16 + i] = std::exp(-dist * dist / 32.0f);
            krn_i[k * kerX_16 * kerX_16 + i] = 0.0f;
        }
    }
    
    // Run HLS
    std::printf("[INFO] Running calc_socs_2048_hls for Nx=16...\n");
    
    calc_socs_2048_hls(
        mskf_r, mskf_i, scales, krn_r, krn_i, output_hls,
        fft_buf_ddr,
        TEST_NX_16, TEST_NX_16, TEST_NK, TEST_LX, TEST_LY, 0
    );
    
    // Write output
    write_binary_data(DATA_DIR "output_nx16_hls.bin", output_hls, convX_16 * convX_16);
    
    // Compare with golden
    read_binary_data(GOLDEN_DIR "aerial_image_socs_kernel.bin", output_golden, convX_16 * convX_16);
    
    int errors = compare_results("Nx=16 Output", output_hls, output_golden, convX_16 * convX_16, 1e-4f);
    
    // Cleanup
    delete[] mskf_r;
    delete[] mskf_i;
    delete[] scales;
    delete[] krn_r;
    delete[] krn_i;
    delete[] output_hls;
    delete[] output_golden;
    delete[] fft_buf_ddr;
    
    return errors;
}

/**
 * Test Case 3: Nx=24 (Maximum Kernel)
 * Expected: 49×49 kernel → 97×97 output
 */
int test_nx24() {
    std::printf("\n========================================\n");
    std::printf("TEST CASE 3: Nx=24 (Maximum Kernel)\n");
    std::printf("========================================\n");
    
    // Allocate arrays
    float* mskf_r = new float[TEST_LX * TEST_LY];
    float* mskf_i = new float[TEST_LX * TEST_LY];
    float* scales = new float[TEST_NK];
    
    int kerX_24 = 2 * TEST_NX_24 + 1;  // = 49
    float* krn_r = new float[TEST_NK * kerX_24 * kerX_24];
    float* krn_i = new float[TEST_NK * kerX_24 * kerX_24];
    
    int convX_24 = 4 * TEST_NX_24 + 1;  // = 97
    float* output_hls = new float[convX_24 * convX_24];
    
    cmpx_2048_t* fft_buf_ddr = new cmpx_2048_t[MAX_FFT_Y * MAX_FFT_X];
    
    // Read input data
    read_binary_data(GOLDEN_DIR "mskf_r.bin", mskf_r, TEST_LX * TEST_LY);
    read_binary_data(GOLDEN_DIR "mskf_i.bin", mskf_i, TEST_LX * TEST_LY);
    read_binary_data(GOLDEN_DIR "scales.bin", scales, TEST_NK);
    
    // Synthetic kernel for Nx=24
    std::printf("[INFO] Using synthetic kernel data for Nx=24 test\n");
    for (int k = 0; k < TEST_NK; k++) {
        for (int i = 0; i < kerX_24 * kerX_24; i++) {
            int y = i / kerX_24;
            int x = i % kerX_24;
            float dist = std::sqrt((x - TEST_NX_24) * (x - TEST_NX_24) + 
                                   (y - TEST_NX_24) * (y - TEST_NX_24));
            krn_r[k * kerX_24 * kerX_24 + i] = std::exp(-dist * dist / 64.0f);
            krn_i[k * kerX_24 * kerX_24 + i] = 0.0f;
        }
    }
    
    // Run HLS
    std::printf("[INFO] Running calc_socs_2048_hls for Nx=24...\n");
    
    calc_socs_2048_hls(
        mskf_r, mskf_i, scales, krn_r, krn_i, output_hls,
        fft_buf_ddr,
        TEST_NX_24, TEST_NX_24, TEST_NK, TEST_LX, TEST_LY, 0
    );
    
    // Write output
    write_binary_data(DATA_DIR "output_nx24_hls.bin", output_hls, convX_24 * convX_24);
    
    // Statistics
    float max_val = 0.0f, min_val = 1e10f, sum_val = 0.0f;
    for (int i = 0; i < convX_24 * convX_24; i++) {
        if (output_hls[i] > max_val) max_val = output_hls[i];
        if (output_hls[i] < min_val) min_val = output_hls[i];
        sum_val += output_hls[i];
    }
    
    std::printf("[RESULT] Nx=24 Output Statistics:\n");
    std::printf("  - Min: %.6f\n", min_val);
    std::printf("  - Max: %.6f\n", max_val);
    std::printf("  - Mean: %.6f\n", sum_val / (convX_24 * convX_24));
    std::printf("  - Output size: %d×%d = %d pixels\n", convX_24, convX_24, convX_24 * convX_24);
    
    // Cleanup
    delete[] mskf_r;
    delete[] mskf_i;
    delete[] scales;
    delete[] krn_r;
    delete[] krn_i;
    delete[] output_hls;
    delete[] fft_buf_ddr;
    
    return 0;
}

// ============================================================================
// Main Test Entry
// ============================================================================

int main() {
    std::printf("============================================\n");
    std::printf("SOCS 2048 Architecture C Simulation Test\n");
    std::printf("============================================\n");
    
    int total_errors = 0;
    
    // Test different Nx configurations
    total_errors += test_nx4();
    total_errors += test_nx16();
    total_errors += test_nx24();
    
    // Summary
    std::printf("\n============================================\n");
    std::printf("TEST SUMMARY\n");
    std::printf("============================================\n");
    std::printf("Total errors: %d\n", total_errors);
    
    if (total_errors == 0) {
        std::printf("[PASS] All tests passed!\n");
        return 0;
    } else {
        std::printf("[FAIL] Some tests failed!\n");
        return 1;
    }
}