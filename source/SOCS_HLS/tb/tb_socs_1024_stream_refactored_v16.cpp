/**
 * @file tb_socs_1024_stream_refactored_v16.cpp
 * @brief Testbench for SOCS 2048 Stream Refactored v16 (HLS FFT IP with Unscaled Mode)
 * 
 * PURPOSE: Validate v16 implementation with unscaled mode for maximum precision
 * 
 * EXPECTED RESULT: RMSE < 1e-5 (matching v13 Direct DFT)
 * 
 * CHANGES from v14:
 * - Use unscaled mode (no automatic scaling, maximum precision)
 * - Increase precision: ap_fixed<24,1> → ap_fixed<32,1>
 * - No compensation needed (FFTW BACKWARD is also unnormalized)
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <cmath>
#include <string>
#include <complex>

using namespace std;

// HLS function declaration
extern void calc_socs_2048_hls_stream_refactored_v16(
    float* mskf_r, float* mskf_i,
    float* krn_r, float* krn_i,
    float* scales,
    float* tmpImg_ddr,
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
    printf("SOCS 1024 Stream Refactored v16 Validation\n");
    printf("HLS FFT IP with Unscaled Mode\n");
    printf("====================================================\n");
    printf("Configuration: golden_1024.json\n");
    printf("  Lx/Ly: %d×%d\n", Lx, Ly);
    printf("  Nx: %d (dynamically calculated)\n", nx);
    printf("  Kernel: %d×%d\n", 2*nx+1, 2*ny+1);
    printf("  Output: %d×%d\n", MAX_FFT_X, MAX_FFT_Y);
    printf("  FFT: %d×%d (HLS FFT IP with ap_fixed<32,1>)\n", MAX_FFT_X, MAX_FFT_Y);
    printf("  Mode: Unscaled (maximum precision, no automatic scaling)\n");
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
    printf("Step 3: Running calc_socs_2048_hls_stream_refactored_v16 (HLS FFT IP with Unscaled Mode)...\n");
    printf("[INFO] Using ap_fixed<32,1> with unscaled mode\n");
    printf("[INFO] Expected: RMSE < 1e-5 (matching v13 Direct DFT)\n");
    
    // Initialize tmpImg_ddr to 0
    for (int i = 0; i < MAX_FFT_X * MAX_FFT_Y; i++) {
        tmpImg_ddr[i] = 0.0f;
    }
    printf("[INFO] tmpImg_ddr initialized to 0\n");
    
    // Call HLS function
    calc_socs_2048_hls_stream_refactored_v16(
        mskf_r, mskf_i,
        krn_r, krn_i,
        scales,
        tmpImg_ddr,
        output,
        nk,
        nx, ny,
        Lx, Ly
    );
    
    printf("[INFO] HLS function completed\n\n");
    
    // Step 4: Validate results
    printf("Step 4: Validating results against golden output...\n");
    
    float sum_sq_error = 0.0f;
    float sum_sq_golden = 0.0f;
    float max_error = 0.0f;
    int error_count = 0;
    
    for (int i = 0; i < MAX_FFT_X * MAX_FFT_Y; i++) {
        float error = output[i] - golden_output[i];
        float abs_error = fabs(error);
        
        sum_sq_error += error * error;
        sum_sq_golden += golden_output[i] * golden_output[i];
        
        if (abs_error > max_error) {
            max_error = abs_error;
        }
        
        if (abs_error > 1e-5) {
            error_count++;
        }
    }
    
    float rmse = sqrtf(sum_sq_error / (MAX_FFT_X * MAX_FFT_Y));
    float mean_golden = sqrtf(sum_sq_golden / (MAX_FFT_X * MAX_FFT_Y));
    
    printf("\n====================================================\n");
    printf("VALIDATION RESULTS\n");
    printf("====================================================\n");
    printf("RMSE: %.6e\n", rmse);
    printf("Max Error: %.6e\n", max_error);
    printf("Mean Golden: %.6e\n", mean_golden);
    printf("Error Count (|error| > 1e-5): %d / %d\n", error_count, MAX_FFT_X * MAX_FFT_Y);
    printf("====================================================\n");
    
    if (rmse < 1e-5) {
        printf("✅ PASS: RMSE < 1e-5\n");
        printf("✅ v16 Block Floating Point mode successfully fixed precision issue\n");
        return 0;
    } else {
        printf("❌ FAIL: RMSE >= 1e-5\n");
        printf("❌ Precision issue persists\n");
        return 1;
    }
}
