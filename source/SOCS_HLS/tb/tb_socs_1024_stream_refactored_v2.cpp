/**
 * @file tb_socs_1024_stream_refactored_v2.cpp
 * @brief Testbench for SOCS 2048 Stream Refactored v2 (Phase 3)
 * 
 * Validates the stream refactored implementation against golden_1024 data.
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string>
#include <complex>

using namespace std;

// HLS function declaration
extern void calc_socs_2048_hls_stream_refactored(
    float* mskf_r, float* mskf_i,
    float* krn_r, float* krn_i,
    float* scales,
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
    printf("SOCS 1024 Stream Refactored v2 Validation (Phase 3)\n");
    printf("====================================================\n");
    printf("Configuration: golden_1024.json\n");
    printf("  Lx/Ly: %d×%d\n", Lx, Ly);
    printf("  Nx: %d (dynamically calculated)\n", nx);
    printf("  Kernel: %d×%d\n", 2*nx+1, 2*ny+1);
    printf("  Output: %d×%d\n", MAX_FFT_X, MAX_FFT_Y);
    printf("  FFT: %d×%d (HLS FFT IP with ap_fixed)\n", MAX_FFT_X, MAX_FFT_Y);
    printf("====================================================\n\n");
    
    // Allocate memory
    float* mskf_r = (float*)malloc(Lx * Ly * sizeof(float));
    float* mskf_i = (float*)malloc(Lx * Ly * sizeof(float));
    float* scales = (float*)malloc(nk * sizeof(float));
    float* krn_r = (float*)malloc(nk * (2*nx+1) * (2*ny+1) * sizeof(float));
    float* krn_i = (float*)malloc(nk * (2*nx+1) * (2*ny+1) * sizeof(float));
    float* output = (float*)malloc(MAX_FFT_X * MAX_FFT_Y * sizeof(float));
    float* golden_output = (float*)malloc(MAX_FFT_X * MAX_FFT_Y * sizeof(float));
    
    if (!mskf_r || !mskf_i || !scales || !krn_r || !krn_i || !output || !golden_output) {
        printf("[ERROR] Memory allocation failed\n");
        return 1;
    }
    
    // Step 1: Load input data
    printf("Step 1: Loading golden_1024 input data...\n");
    
    std::string base_path = "/home/ashington/fpga-litho-accel/output/verification/";
    
    if (read_binary_file((base_path + "mskf_r.bin").c_str(), mskf_r, Lx * Ly * sizeof(float)) < 0) return 1;
    if (read_binary_file((base_path + "mskf_i.bin").c_str(), mskf_i, Lx * Ly * sizeof(float)) < 0) return 1;
    if (read_binary_file((base_path + "scales.bin").c_str(), scales, nk * sizeof(float)) < 0) return 1;
    
    // Load kernels
    int kerX = 2 * nx + 1;
    int kerY = 2 * ny + 1;
    for (int k = 0; k < nk; k++) {
        char kr_file[256], ki_file[256];
        snprintf(kr_file, sizeof(kr_file), "%skernels/krn_%d_r.bin", base_path.c_str(), k);
        snprintf(ki_file, sizeof(ki_file), "%skernels/krn_%d_i.bin", base_path.c_str(), k);
        
        if (read_binary_file(kr_file, &krn_r[k * kerX * kerY], kerX * kerY * sizeof(float)) < 0) return 1;
        if (read_binary_file(ki_file, &krn_i[k * kerX * kerY], kerX * kerY * sizeof(float)) < 0) return 1;
    }
    printf("[INFO] Loaded %d kernels (%d×%d each) ✓\n", nk, kerX, kerY);
    
    // Load golden output
    if (read_binary_file((base_path + "tmpImgp_full_128.bin").c_str(), golden_output, 
                        MAX_FFT_X * MAX_FFT_Y * sizeof(float)) < 0) return 1;
    printf("[INFO] Output mode: Full tmpImgp (%d×%d) for FI ✓\n", MAX_FFT_X, MAX_FFT_Y);
    
    printf("[INFO] Data loading completed ✓\n\n");
    
    // Step 2: Run HLS SOCS calculation
    printf("Step 2: Running HLS SOCS calculation (Stream Refactored v2)...\n");
    printf("[INFO] Using HLS FFT IP with ap_fixed<24,1> precision\n");
    
    calc_socs_2048_hls_stream_refactored(
        mskf_r, mskf_i,
        krn_r, krn_i,
        scales,
        output,
        nk, nx, ny, Lx, Ly
    );
    
    printf("[INFO] HLS SOCS calculation completed ✓\n\n");
    
    // Step 3: Compare with golden reference
    printf("Step 3: Comparing HLS output with golden reference...\n");
    
    float max_err = 0.0f;
    float sum_sq_err = 0.0f;
    int error_count = 0;
    const float tolerance = 1e-5f;
    
    for (int i = 0; i < MAX_FFT_X * MAX_FFT_Y; i++) {
        float diff = fabsf(output[i] - golden_output[i]);
        sum_sq_err += diff * diff;
        
        if (diff > max_err) {
            max_err = diff;
        }
        
        if (diff > tolerance) {
            error_count++;
            if (error_count <= 10) {
                printf("[DIFF] SOCS_Output[%d]: HLS=%.8f, Golden=%.8f, diff=%.8f\n",
                       i, output[i], golden_output[i], diff);
            }
        }
    }
    
    float rmse = sqrtf(sum_sq_err / (MAX_FFT_X * MAX_FFT_Y));
    
    printf("[RESULT] SOCS_Output: errors=%d/%d, max_err=%.8f, RMSE=%.8f\n",
           error_count, MAX_FFT_X * MAX_FFT_Y, max_err, rmse);
    
    if (rmse < tolerance) {
        printf("[PASS] RMSE=%.8f < tolerance=%.8f ✓\n\n", rmse, tolerance);
    } else {
        printf("[FAIL] RMSE=%.8f >= tolerance=%.8f ✗\n\n", rmse, tolerance);
    }
    
    // Step 4: Save output for analysis
    printf("Step 4: Saving HLS output for analysis...\n");
    std::string output_path = "/home/ashington/fpga-litho-accel/source/SOCS_HLS/data/socs_output_stream_refactored_v2.bin";
    FILE* fp = fopen(output_path.c_str(), "wb");
    if (fp) {
        fwrite(output, sizeof(float), MAX_FFT_X * MAX_FFT_Y, fp);
        fclose(fp);
        printf("[INFO] Saved full tmpImgp (%d×%d) ✓\n", MAX_FFT_X, MAX_FFT_Y);
    }
    
    printf("\n====================================================\n");
    if (rmse < tolerance) {
        printf("Phase 3 Stream Refactored v2 Validation: PASS ✓\n");
        printf("  RMSE: %.8f\n", rmse);
        printf("  Max Error: %.8f\n", max_err);
    } else {
        printf("Phase 3 Stream Refactored v2 Validation: FAIL ✗\n");
        printf("  Check RMSE tolerance or data path\n");
    }
    printf("====================================================\n");
    
    // Cleanup
    free(mskf_r);
    free(mskf_i);
    free(scales);
    free(krn_r);
    free(krn_i);
    free(output);
    free(golden_output);
    
    return (rmse < tolerance) ? 0 : 255;
}
