/**
 * Test Bench for SOCS 1024 Architecture (golden_1024.json)
 * FPGA-Litho Project - Phase 1.4 Validation
 * 
 * Configuration: golden_1024.json
 *   - Lx/Ly: 1024×1024 (4x resolution improvement)
 *   - NA: 0.8
 *   - wavelength: 193nm
 *   - sigma_outer: 0.9
 *   - Nx ≈ 8 (dynamically calculated)
 *   - FFT size: 128×128
 * 
 * Validation Strategy:
 *   1. Load golden_1024 data from output/verification/
 *   2. Run HLS SOCS with ap_fixed FFT
 *   3. Compare with CPU reference (tmpImgp_pad32.bin)
 *   4. Verify RMSE < 1e-5
 */

#include "../src/socs_2048.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <fstream>
#include <complex>

// ============================================================================
// Test Configuration (golden_1024.json)
// ============================================================================

#define TEST_LX      1024
#define TEST_LY      1024
#define TEST_NK      10

// Nx dynamically calculated: Nx = floor(NA * Lx * (1+sigma_outer) / lambda)
// For golden_1024: Nx ≈ 8
#define TEST_NX      8

// Kernel and output dimensions
#define KER_X        (2 * TEST_NX + 1)   // = 17
#define CONV_X       (4 * TEST_NX + 1)   // = 33

// FFT dimensions (128×128 for 2048 architecture)
#define MAX_FFT_X    128
#define MAX_FFT_Y    128

// Data paths
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
 * Compare two arrays with tolerance and RMSE calculation
 */
int compare_results(
    const char* name,
    float* hls_output,
    float* golden_output,
    int count,
    float tolerance = 1e-5f
) {
    int errors = 0;
    float max_error = 0.0f;
    float sum_error = 0.0f;
    
    for (int i = 0; i < count; i++) {
        float diff = std::abs(hls_output[i] - golden_output[i]);
        sum_error += diff * diff;  // RMSE uses squared error
        
        if (diff > max_error) {
            max_error = diff;
        }
        
        if (diff > tolerance) {
            errors++;
            if (errors < 10) {
                std::printf("[DIFF] %s[%d]: HLS=%.8f, Golden=%.8f, diff=%.8f\n",
                    name, i, hls_output[i], golden_output[i], diff);
            }
        }
    }
    
    float rmse = std::sqrt(sum_error / count);
    std::printf("[RESULT] %s: errors=%d/%d, max_err=%.8f, RMSE=%.8f\n",
        name, errors, count, max_error, rmse);
    
    if (rmse < tolerance) {
        std::printf("[PASS] RMSE=%.8f < tolerance=%.8f ✓\n", rmse, tolerance);
        return 0;
    } else {
        std::printf("[FAIL] RMSE=%.8f >= tolerance=%.8f ✗\n", rmse, tolerance);
        return -1;
    }
}

// ============================================================================
// Main Test
// ============================================================================

int main() {
    std::printf("\n");
    std::printf("====================================================\n");
    std::printf("SOCS 1024 Architecture Validation (Phase 1.4)\n");
    std::printf("====================================================\n");
    std::printf("Configuration: golden_1024.json\n");
    std::printf("  Lx/Ly: %d×%d\n", TEST_LX, TEST_LY);
    std::printf("  Nx: %d (dynamically calculated)\n", TEST_NX);
    std::printf("  Kernel: %d×%d\n", KER_X, KER_X);
    std::printf("  Output: %d×%d\n", CONV_X, CONV_X);
    std::printf("  FFT: %d×%d (HLS FFT IP with ap_fixed)\n", MAX_FFT_X, MAX_FFT_Y);
    std::printf("====================================================\n\n");
    
    // ========================================================================
    // Step 1: Load Input Data
    // ========================================================================
    std::printf("Step 1: Loading golden_1024 input data...\n");
    
    // Allocate arrays
    float* mskf_r = new float[TEST_LX * TEST_LY];
    float* mskf_i = new float[TEST_LX * TEST_LY];
    float* scales = new float[TEST_NK];
    float* krn_r = new float[TEST_NK * KER_X * KER_X];
    float* krn_i = new float[TEST_NK * KER_X * KER_X];
    float* output_hls = new float[CONV_X * CONV_X];
    float* output_golden = new float[CONV_X * CONV_X];
    
    // FFT DDR buffer
    cmpx_2048_t* fft_buf_ddr = new cmpx_2048_t[MAX_FFT_Y * MAX_FFT_X];
    
    // Read mask frequency data
    int read_count = 0;
    read_count = read_binary_data(GOLDEN_DIR "mskf_r.bin", mskf_r, TEST_LX * TEST_LY);
    if (read_count != TEST_LX * TEST_LY) {
        std::printf("[ERROR] Failed to read mskf_r.bin (expected %d, got %d)\n",
            TEST_LX * TEST_LY, read_count);
        return -1;
    }
    
    read_count = read_binary_data(GOLDEN_DIR "mskf_i.bin", mskf_i, TEST_LX * TEST_LY);
    if (read_count != TEST_LX * TEST_LY) {
        std::printf("[ERROR] Failed to read mskf_i.bin\n");
        return -1;
    }
    
    // Read scales
    read_count = read_binary_data(GOLDEN_DIR "scales.bin", scales, TEST_NK);
    if (read_count != TEST_NK) {
        std::printf("[ERROR] Failed to read scales.bin\n");
        return -1;
    }
    
    // Read kernel data (golden_1024 format: individual kernel files)
    std::printf("[INFO] Loading kernel data for Nx=%d (17×17)...\n", TEST_NX);
    
    for (int k = 0; k < TEST_NK; k++) {
        char krn_path_r[256], krn_path_i[256];
        std::snprintf(krn_path_r, 256, "%skernels/krn_%d_r.bin", GOLDEN_DIR, k);
        std::snprintf(krn_path_i, 256, "%skernels/krn_%d_i.bin", GOLDEN_DIR, k);
        
        read_binary_data(krn_path_r, krn_r + k * KER_X * KER_X, KER_X * KER_X);
        read_binary_data(krn_path_i, krn_i + k * KER_X * KER_X, KER_X * KER_X);
    }
    
    std::printf("[INFO] Loaded %d kernels (17×17 each) ✓\n", TEST_NK);
    
    // Read golden output (tmpImgp_pad64.bin for Nx=8, 33×33)
    // Note: tmpImgp_pad64.bin contains the padded intermediate output (33×33)
    read_binary_data(GOLDEN_DIR "tmpImgp_pad64.bin", output_golden, CONV_X * CONV_X);
    
    std::printf("[INFO] Data loading completed ✓\n\n");
    
    // ========================================================================
    // Step 2: Run HLS SOCS Function
    // ========================================================================
    std::printf("Step 2: Running HLS SOCS calculation...\n");
    std::printf("[INFO] Using HLS FFT IP with ap_fixed<32,1> precision\n");
    
    // Call HLS top-level function
    calc_socs_2048_hls(
        mskf_r, mskf_i,
        scales,
        krn_r, krn_i,
        output_hls,
        fft_buf_ddr,
        TEST_NX,        // nx_actual (runtime parameter)
        TEST_NX,        // ny_actual (symmetric)
        TEST_NK,        // nk (number of kernels)
        TEST_LX,        // Lx (mask width 1024)
        TEST_LY,        // Ly (mask height 1024)
        0               // fft_buf_base (DDR base address, 0 for test)
    );
    
    std::printf("[INFO] HLS SOCS calculation completed ✓\n\n");
    
    // ========================================================================
    // Step 3: Compare Results
    // ========================================================================
    std::printf("Step 3: Comparing HLS output with golden reference...\n");
    
    int result = compare_results(
        "SOCS_Output",
        output_hls,
        output_golden,
        CONV_X * CONV_X,
        1e-5f  // RMSE tolerance
    );
    
    // ========================================================================
    // Step 4: Save Results
    // ========================================================================
    std::printf("\nStep 4: Saving HLS output for analysis...\n");
    write_binary_data(DATA_DIR "socs_output_hls_1024.bin", output_hls, CONV_X * CONV_X);
    
    // ========================================================================
    // Cleanup
    // ========================================================================
    delete[] mskf_r;
    delete[] mskf_i;
    delete[] scales;
    delete[] krn_r;
    delete[] krn_i;
    delete[] output_hls;
    delete[] output_golden;
    delete[] fft_buf_ddr;
    
    std::printf("\n====================================================\n");
    if (result == 0) {
        std::printf("Phase 1.4 Validation: PASS ✓\n");
        std::printf("  RMSE < 1e-5\n");
        std::printf("  HLS FFT IP integration verified\n");
    } else {
        std::printf("Phase 1.4 Validation: FAIL ✗\n");
        std::printf("  Check RMSE tolerance or data path\n");
    }
    std::printf("====================================================\n\n");
    
    return result;
}