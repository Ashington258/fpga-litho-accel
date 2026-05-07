/**
 * @file tb_socs_1024_stream_refactored_v13.cpp
 * @brief Testbench for SOCS 2048 Stream Refactored v13 (Direct DFT Only)
 * 
 * Forces Direct DFT path for both C Sim and Co-Sim to isolate NaN issue.
 * 
 * CRITICAL: This version FORCES Direct DFT for both C Sim and Co-Sim
 * - Does NOT use #ifdef __SYNTHESIS__
 * - Directly implements Direct DFT (copied from socs_2048.cpp)
 * - Bypasses fft_2d_full_2048() which has conditional compilation
 * 
 * EXPECTED BEHAVIOR:
 * - C Simulation: Should pass (same as v13)
 * - Co-Simulation: If NaN still occurs, issue is NOT in FFT
 *                  If NaN disappears, issue IS in HLS FFT IP
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <cmath>
#include <string>
#include <complex>

using namespace std;

// HLS function declaration
extern void calc_socs_2048_hls_stream_refactored_v13(
    float* mskf_r, float* mskf_i,
    float* krn_r, float* krn_i,
    float* scales,
    float* tmpImg_ddr,  // DDR storage for tmpImg_final
    float* output,
    int nk,
    int nx_actual,
    int ny_actual,
    int Lx,
    int Ly
);

// Constants from golden_1024.json
const int Lx = 1024;
const int Ly = 1024;
const int nk = 10;
const int nx = 8;
const int ny = 8;
const int MAX_FFT_X = 128;
const int MAX_FFT_Y = 128;

// Helper function to read binary file
int read_binary_file(const char* filename, float* data, int expected_size) {
    FILE* fp = fopen(filename, "rb");
    if (!fp) {
        printf("[ERROR] Cannot open file: %s\n", filename);
        return -1;
    }
    
    fseek(fp, 0, SEEK_END);
    int file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    if (file_size != expected_size) {
        printf("[ERROR] File size mismatch: %s (expected %d, got %d)\n", 
               filename, expected_size, file_size);
        fclose(fp);
        return -1;
    }
    
    int num_read = fread(data, 1, expected_size, fp);
    fclose(fp);
    
    printf("[INFO] Read %d bytes from %s\n", num_read, filename);
    return 0;
}

int main() {
    printf("====================================================\n");
    printf("SOCS 1024 Stream Refactored v3 Validation (Phase 3)\n");
    printf("Multi-FFT Instance Parallelization (UNROLL factor=2)\n");
    printf("====================================================\n");
    printf("Configuration: golden_1024.json\n");
    printf("  Lx/Ly: %d×%d\n", Lx, Ly);
    printf("  Nx: %d (dynamically calculated)\n", nx);
    printf("  Kernel: %d×%d\n", 2*nx+1, 2*ny+1);
    printf("  Output: %d×%d\n", MAX_FFT_X, MAX_FFT_Y);
    printf("  FFT: %d×%d (HLS FFT IP with ap_fixed)\n", MAX_FFT_X, MAX_FFT_Y);
    printf("  Parallelization: 2 kernels simultaneously\n");
    printf("====================================================\n\n");
    
    // Allocate memory
    float* mskf_r = (float*)malloc(Lx * Ly * sizeof(float));
    float* mskf_i = (float*)malloc(Lx * Ly * sizeof(float));
    float* scales = (float*)malloc(nk * sizeof(float));
    float* krn_r = (float*)malloc(nk * (2*nx+1) * (2*ny+1) * sizeof(float));
    float* krn_i = (float*)malloc(nk * (2*nx+1) * (2*ny+1) * sizeof(float));
    float* tmpImg_ddr = (float*)malloc(MAX_FFT_X * MAX_FFT_Y * sizeof(float));
    float* output = (float*)malloc(MAX_FFT_X * MAX_FFT_Y * sizeof(float));
    float* golden_output = (float*)malloc(MAX_FFT_X * MAX_FFT_Y * sizeof(float));
    
    if (!mskf_r || !mskf_i || !scales || !krn_r || !krn_i || !tmpImg_ddr || !output || !golden_output) {
        printf("[ERROR] Memory allocation failed\n");
        return 1;
    }
    
    // Step 1: Load input data
    printf("Step 1: Loading golden_1024 input data...\n");
    
    std::string base_path = "/home/ashington/fpga-litho-accel/output/verification/";
    
    if (read_binary_file((base_path + "mskf_r.bin").c_str(), mskf_r, Lx * Ly * sizeof(float)) < 0) return 1;
    if (read_binary_file((base_path + "mskf_i.bin").c_str(), mskf_i, Lx * Ly * sizeof(float)) < 0) return 1;
    if (read_binary_file((base_path + "scales.bin").c_str(), scales, nk * sizeof(float)) < 0) return 1;
    
    // Load kernels (individual files for each kernel)
    int kerX = 2 * nx + 1;
    int kerY = 2 * ny + 1;
    for (int k = 0; k < nk; k++) {
        char kr_file[256], ki_file[256];
        snprintf(kr_file, sizeof(kr_file), "%skernels/krn_%d_r.bin", base_path.c_str(), k);
        snprintf(ki_file, sizeof(ki_file), "%skernels/krn_%d_i.bin", base_path.c_str(), k);
        
        if (read_binary_file(kr_file, &krn_r[k * kerX * kerY], kerX * kerY * sizeof(float)) < 0) return 1;
        if (read_binary_file(ki_file, &krn_i[k * kerX * kerY], kerX * kerY * sizeof(float)) < 0) return 1;
    }
    
    printf("[INFO] Input data loaded successfully\n\n");
    
    // Step 2: Load golden output
    printf("Step 2: Loading golden output (tmpImgp_full_128.bin)...\n");
    
    if (read_binary_file((base_path + "tmpImgp_full_128.bin").c_str(), golden_output, 
                         MAX_FFT_X * MAX_FFT_Y * sizeof(float)) < 0) {
        printf("[ERROR] Failed to load golden output\n");
        return 1;
    }
    
    printf("[INFO] Golden output loaded successfully\n\n");
    
    // Step 3: Run HLS function
    printf("Step 3: Running calc_socs_2048_hls_stream_refactored_v13 (Direct DFT Only)...\n");
    printf("[INFO] Forces Direct DFT for both C Sim and Co-Sim\n");
    printf("[INFO] Purpose: Isolate NaN issue in Co-Simulation\n");
    
    // CRITICAL FIX: Initialize tmpImg_ddr to 0 before calling HLS function
    // This ensures DDR is properly initialized for RTL simulation
    for (int i = 0; i < MAX_FFT_X * MAX_FFT_Y; i++) {
        tmpImg_ddr[i] = 0.0f;
    }
    printf("[INFO] tmpImg_ddr initialized to 0\n");
    
    calc_socs_2048_hls_stream_refactored_v13(
        mskf_r, mskf_i,
        krn_r, krn_i,
        scales,
        tmpImg_ddr,  // DDR storage
        output,
        nk, nx, ny,
        Lx, Ly
    );
    
    printf("[INFO] HLS function completed\n\n");
    
    // Debug: Check output values
    printf("Step 4: Checking output values...\n");
    int zero_count = 0;
    int nan_count = 0;
    for (int i = 0; i < 10; i++) {
        printf("[DEBUG] output[%d] = %.6e\n", i, output[i]);
        if (output[i] == 0.0f) zero_count++;
        if (std::isnan(output[i])) nan_count++;
    }
    printf("[DEBUG] First 10 values: %d zeros, %d NaNs\n\n", zero_count, nan_count);
    
    // Step 5: Compare results
    printf("Step 5: Comparing HLS output with golden reference...\n");
    
    float sum_sq_error = 0.0f;
    float max_error = 0.0f;
    int errors = 0;
    const float tolerance = 1e-5f;
    
    for (int i = 0; i < MAX_FFT_X * MAX_FFT_Y; i++) {
        float diff = fabs(output[i] - golden_output[i]);
        sum_sq_error += diff * diff;
        
        if (diff > max_error) {
            max_error = diff;
        }
        
        if (diff > tolerance) {
            errors++;
            if (errors <= 5) {
                printf("[ERROR] Pixel %d: HLS=%.6e, Golden=%.6e, Diff=%.6e\n",
                       i, output[i], golden_output[i], diff);
            }
        }
    }
    
    float rmse = sqrt(sum_sq_error / (MAX_FFT_X * MAX_FFT_Y));
    
    printf("\n====================================================\n");
    printf("Validation Results:\n");
    printf("====================================================\n");
    printf("RMSE: %.3e\n", rmse);
    printf("Max Error: %.3e\n", max_error);
    printf("Errors (diff > %.0e): %d / %d\n", tolerance, errors, MAX_FFT_X * MAX_FFT_Y);
    printf("====================================================\n");
    
    if (rmse < tolerance) {
        printf("[PASS] RMSE < %.0e - Functional correctness verified!\n", tolerance);
        printf("\nExpected improvements from v8:\n");
        printf("  - BRAM: Reduced by ~304 (single FFT instance)\n");
        printf("  - BRAM: Reduced by ~60 (DDR-based FFTshift)\n");
        printf("  - Total BRAM savings: ~364\n");
        printf("  - Throughput: Sequential processing (slower but BRAM-efficient)\n");
        printf("====================================================\n");
        return 0;
    } else {
        printf("[FAIL] RMSE >= %.0e - Functional correctness NOT verified\n", tolerance);
        printf("====================================================\n");
        return 1;
    }
}
