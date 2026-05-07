/**
 * @file tb_socs_2048_ddr_optimized.cpp
 * @brief Test bench for DDR-optimized SOCS HLS implementation
 * 
 * 验证目标：
 * 1. 功能正确性: RMSE < 1e-5
 * 2. DDR访问优化: Burst长度和Outstanding增加
 * 3. 性能保持: FFT Interval < 100,000 cycles
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "socs_2048_ddr_optimized.h"

// Golden data paths
#define DATA_DIR "data/"
#define MASK_R_FILE DATA_DIR "mskf_r.bin"
#define MASK_I_FILE DATA_DIR "mskf_i.bin"
#define SCALES_FILE DATA_DIR "scales.bin"
#define OUTPUT_FILE DATA_DIR "tmpImgp_pad128.bin"

// Configuration (golden_1024)
#define NK 10
#define NX_ACTUAL 8
#define NY_ACTUAL 8
#define LX 1024
#define LY 1024
#define CONVX 33
#define CONVY 33

int main() {
    printf("=== SOCS DDR Optimized Test Bench ===\n");
    printf("Configuration: golden_1024 (Lx=%d, nk=%d, nx=%d)\n", LX, NK, NX_ACTUAL);
    
    // Allocate memory
    float* mskf_r = (float*)malloc(LX * LY * sizeof(float));
    float* mskf_i = (float*)malloc(LX * LY * sizeof(float));
    float* krn_r = (float*)malloc(NK * 17 * 17 * sizeof(float));
    float* krn_i = (float*)malloc(NK * 17 * 17 * sizeof(float));
    float* scales = (float*)malloc(NK * sizeof(float));
    float* output = (float*)malloc(CONVX * CONVY * sizeof(float));
    float* output_ref = (float*)malloc(CONVX * CONVY * sizeof(float));
    
    if (!mskf_r || !mskf_i || !krn_r || !krn_i || !scales || !output || !output_ref) {
        printf("ERROR: Memory allocation failed\n");
        return 1;
    }
    
    // Load mask data
    printf("\nLoading mask data...\n");
    FILE* fp_r = fopen(MASK_R_FILE, "rb");
    FILE* fp_i = fopen(MASK_I_FILE, "rb");
    if (!fp_r || !fp_i) {
        printf("ERROR: Cannot open mask files\n");
        printf("  %s\n", MASK_R_FILE);
        printf("  %s\n", MASK_I_FILE);
        return 1;
    }
    fread(mskf_r, sizeof(float), LX * LY, fp_r);
    fread(mskf_i, sizeof(float), LX * LY, fp_i);
    fclose(fp_r);
    fclose(fp_i);
    printf("  Mask loaded: %d x %d\n", LX, LY);
    
    // Load scales
    printf("\nLoading scales...\n");
    FILE* fp_scales = fopen(SCALES_FILE, "rb");
    if (!fp_scales) {
        printf("ERROR: Cannot open scales file: %s\n", SCALES_FILE);
        return 1;
    }
    fread(scales, sizeof(float), NK, fp_scales);
    fclose(fp_scales);
    printf("  Scales loaded: nk=%d\n", NK);
    
    // Load kernels (separate files for each kernel)
    printf("\nLoading kernels...\n");
    char krn_file[256];
    for (int k = 0; k < NK; k++) {
        // Load real part
        snprintf(krn_file, sizeof(krn_file), DATA_DIR "kernels/krn_%d_r.bin", k);
        FILE* fp_krn_r = fopen(krn_file, "rb");
        if (!fp_krn_r) {
            printf("ERROR: Cannot open kernel file: %s\n", krn_file);
            return 1;
        }
        fread(&krn_r[k * 17 * 17], sizeof(float), 17 * 17, fp_krn_r);
        fclose(fp_krn_r);
        
        // Load imaginary part
        snprintf(krn_file, sizeof(krn_file), DATA_DIR "kernels/krn_%d_i.bin", k);
        FILE* fp_krn_i = fopen(krn_file, "rb");
        if (!fp_krn_i) {
            printf("ERROR: Cannot open kernel file: %s\n", krn_file);
            return 1;
        }
        fread(&krn_i[k * 17 * 17], sizeof(float), 17 * 17, fp_krn_i);
        fclose(fp_krn_i);
    }
    printf("  Kernels loaded: %d kernels (17x17 each)\n", NK);
    
    // Load reference output
    printf("\nLoading reference output...\n");
    FILE* fp_ref = fopen(OUTPUT_FILE, "rb");
    if (!fp_ref) {
        printf("ERROR: Cannot open reference file: %s\n", OUTPUT_FILE);
        return 1;
    }
    fread(output_ref, sizeof(float), CONVX * CONVY, fp_ref);
    fclose(fp_ref);
    printf("  Reference loaded: %d x %d\n", CONVX, CONVY);
    
    // Initialize output
    memset(output, 0, CONVX * CONVY * sizeof(float));
    
    // Run HLS function
    printf("\n=== Running DDR Optimized SOCS ===\n");
    calc_socs_2048_hls_ddr_optimized(
        mskf_r, mskf_i,
        krn_r, krn_i,
        scales,
        output,
        NK, NX_ACTUAL, NY_ACTUAL,
        LX, LY
    );
    
    // Calculate RMSE
    printf("\n=== Validation ===\n");
    double sum_sq_error = 0.0;
    double max_error = 0.0;
    int error_count = 0;
    
    for (int i = 0; i < CONVX * CONVY; i++) {
        double error = output[i] - output_ref[i];
        double sq_error = error * error;
        sum_sq_error += sq_error;
        
        if (fabs(error) > max_error) {
            max_error = fabs(error);
        }
        
        if (fabs(error) > 1e-5) {
            error_count++;
        }
    }
    
    double rmse = sqrt(sum_sq_error / (CONVX * CONVY));
    
    printf("RMSE: %.6e\n", rmse);
    printf("Max Error: %.6e\n", max_error);
    printf("Error Count (>1e-5): %d / %d\n", error_count, CONVX * CONVY);
    
    // Validation result
    printf("\n=== Result ===\n");
    if (rmse < 1e-5) {
        printf("✅ PASS: RMSE = %.6e < 1e-5\n", rmse);
    } else {
        printf("❌ FAIL: RMSE = %.6e >= 1e-5\n", rmse);
    }
    
    // Print sample outputs
    printf("\n=== Sample Outputs ===\n");
    printf("Index | HLS Output | Reference | Error\n");
    printf("------|------------|-----------|--------\n");
    for (int i = 0; i < 10; i++) {
        printf("%5d | %10.6f | %9.6f | % .6e\n",
               i, output[i], output_ref[i], output[i] - output_ref[i]);
    }
    
    // Cleanup
    free(mskf_r);
    free(mskf_i);
    free(krn_r);
    free(krn_i);
    free(scales);
    free(output);
    free(output_ref);
    
    return (rmse < 1e-5) ? 0 : 1;
}
