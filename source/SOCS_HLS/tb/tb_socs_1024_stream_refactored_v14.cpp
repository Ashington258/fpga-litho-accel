/**
 * @file tb_socs_1024_stream_refactored_v14.cpp
 * @brief Testbench for SOCS 2048 Stream Refactored v14 (HLS FFT IP)
 * 
 * Replaces Direct DFT with HLS FFT IP for O(N log N) performance.
 * Should pass C Simulation AND Co-Simulation (no RTL timeout expected).
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <cmath>
#include <string>
#include <complex>

using namespace std;

// HLS function declaration
extern void calc_socs_2048_hls_stream_refactored_v14(
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
    printf("SOCS 1024 Stream Refactored v14 Validation\n");
    printf("HLS FFT IP Integration (Replaces Direct DFT)\n");
    printf("====================================================\n");
    printf("Configuration: golden_1024.json\n");
    printf("  Lx/Ly: %dx%d\n", Lx, Ly);
    printf("  Nx: %d (dynamically calculated)\n", nx);
    printf("  Kernel: %dx%d\n", 2*nx+1, 2*ny+1);
    printf("  Output: %dx%d\n", MAX_FFT_X, MAX_FFT_Y);
    printf("  FFT: %dx%d (HLS FFT IP, ap_fixed<24,1>)\n", MAX_FFT_X, MAX_FFT_Y);
    printf("  Scaling: HLS scaled mode (0x155) - matches FFTW BACKWARD\n");
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
    printf("Step 3: Running calc_socs_2048_hls_stream_refactored_v14 (HLS FFT IP)...\n");
    printf("[INFO] Uses HLS FFT IP for both C Sim and Co-Sim\n");
    printf("[INFO] Expected: O(N log N) performance, ~18x faster than Direct DFT\n");
    
    // Initialize tmpImg_ddr to 0 before calling HLS function
    for (int i = 0; i < MAX_FFT_X * MAX_FFT_Y; i++) {
        tmpImg_ddr[i] = 0.0f;
    }
    printf("[INFO] tmpImg_ddr initialized to 0\n");
    
    calc_socs_2048_hls_stream_refactored_v14(
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
    float hls_min = 1e30f, hls_max = -1e30f, hls_sum = 0.0f;
    float golden_min = 1e30f, golden_max = -1e30f, golden_sum = 0.0f;
    
    for (int i = 0; i < MAX_FFT_X * MAX_FFT_Y; i++) {
        if (output[i] == 0.0f) zero_count++;
        if (std::isnan(output[i])) nan_count++;
        if (output[i] < hls_min) hls_min = output[i];
        if (output[i] > hls_max) hls_max = output[i];
        hls_sum += output[i];
        if (golden_output[i] < golden_min) golden_min = golden_output[i];
        if (golden_output[i] > golden_max) golden_max = golden_output[i];
        golden_sum += golden_output[i];
    }
    
    printf("[DEBUG] HLS output: min=%.6e, max=%.6e, mean=%.6e\n", 
           hls_min, hls_max, hls_sum / (MAX_FFT_X * MAX_FFT_Y));
    printf("[DEBUG] Golden:     min=%.6e, max=%.6e, mean=%.6e\n",
           golden_min, golden_max, golden_sum / (MAX_FFT_X * MAX_FFT_Y));
    printf("[DEBUG] %d zeros, %d NaNs\n\n", zero_count, nan_count);
    
    for (int i = 0; i < 10; i++) {
        printf("[DEBUG] output[%d] = %.6e, golden[%d] = %.6e\n", 
               i, output[i], i, golden_output[i]);
    }
    
    // Step 5: Compare results
    printf("\nStep 5: Comparing HLS output with golden reference...\n");
    
    float sum_sq_error = 0.0f;
    float max_error = 0.0f;
    int max_error_idx = 0;
    int errors = 0;
    const float tolerance = 1e-5f;
    
    // Error histogram bins
    int err_bins[7] = {0}; // <1e-5, <1e-4, <1e-3, <1e-2, <1e-1, <1e-1, >=1e-1
    float err_thresholds[7] = {1e-5f, 1e-4f, 1e-3f, 1e-2f, 1e-1f, 0.5f, 1e30f};
    
    // Relative error tracking
    float max_rel_error = 0.0f;
    int max_rel_error_idx = 0;
    int rel_err_above_1pct = 0;
    
    for (int i = 0; i < MAX_FFT_X * MAX_FFT_Y; i++) {
        float diff = fabs(output[i] - golden_output[i]);
        sum_sq_error += diff * diff;
        
        if (diff > max_error) {
            max_error = diff;
            max_error_idx = i;
        }
        
        // Relative error
        if (fabs(golden_output[i]) > 1e-10f) {
            float rel_err = diff / fabs(golden_output[i]);
            if (rel_err > max_rel_error) {
                max_rel_error = rel_err;
                max_rel_error_idx = i;
            }
            if (rel_err > 0.01f) rel_err_above_1pct++;
        }
        
        // Error histogram
        for (int b = 0; b < 7; b++) {
            if (diff < err_thresholds[b]) {
                err_bins[b]++;
                break;
            }
        }
        
        if (diff > tolerance) {
            errors++;
            if (errors <= 10) {
                int y = i / MAX_FFT_X;
                int x = i % MAX_FFT_X;
                printf("[ERROR] Pixel (%d,%d): HLS=%.6e, Golden=%.6e, Diff=%.6e, Rel=%.2f%%\n",
                       y, x, output[i], golden_output[i], diff,
                       fabs(golden_output[i]) > 1e-10f ? diff/fabs(golden_output[i])*100 : 0);
            }
        }
    }
    
    printf("\n[DIAG] Error histogram:\n");
    printf("  < 1e-5: %d\n", err_bins[0]);
    printf("  < 1e-4: %d\n", err_bins[1]);
    printf("  < 1e-3: %d\n", err_bins[2]);
    printf("  < 1e-2: %d\n", err_bins[3]);
    printf("  < 1e-1: %d\n", err_bins[4]);
    printf("  < 0.5:  %d\n", err_bins[5]);
    printf("  >= 0.5: %d\n", err_bins[6]);
    
    int my = max_error_idx / MAX_FFT_X;
    int mx = max_error_idx % MAX_FFT_X;
    printf("[DIAG] Max abs error: %.6e at pixel (%d,%d): HLS=%.6e, Golden=%.6e\n",
           max_error, my, mx, output[max_error_idx], golden_output[max_error_idx]);
    
    int mry = max_rel_error_idx / MAX_FFT_X;
    int mrx = max_rel_error_idx % MAX_FFT_X;
    printf("[DIAG] Max rel error: %.2f%% at pixel (%d,%d): HLS=%.6e, Golden=%.6e\n",
           max_rel_error * 100, mry, mrx, output[max_rel_error_idx], golden_output[max_rel_error_idx]);
    printf("[DIAG] Pixels with rel error > 1%%: %d / %d\n", rel_err_above_1pct, MAX_FFT_X * MAX_FFT_Y);
    
    float rmse = sqrt(sum_sq_error / (MAX_FFT_X * MAX_FFT_Y));
    
    printf("\n====================================================\n");
    printf("Validation Results:\n");
    printf("====================================================\n");
    printf("RMSE: %.3e\n", rmse);
    printf("Max Error: %.3e\n", max_error);
    printf("Errors (diff > %.0e): %d / %d\n", tolerance, errors, MAX_FFT_X * MAX_FFT_Y);
    printf("====================================================\n");
    
    if (rmse < tolerance) {
        printf("[PASS] RMSE < %.0e - HLS FFT IP integration verified!\n", tolerance);
        printf("\nExpected improvements from v13 (Direct DFT):\n");
        printf("  - Latency: O(N^2) -> O(N log N), ~18x faster\n");
        printf("  - Co-Sim: Should complete without timeout\n");
        printf("  - Scaling: Auto 1/N per dim (matches FFTW BACKWARD)\n");
        printf("====================================================\n");
        return 0;
    } else {
        printf("[FAIL] RMSE >= %.0e - HLS FFT IP integration NOT verified\n", tolerance);
        printf("====================================================\n");
        return 1;
    }
}
