/**
 * @file tb_socs_2048_stream_optimized.cpp
 * @brief Test bench for optimized stream-based SOCS implementation
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <unistd.h>
#include <fstream>
#include <iostream>
#include "socs_2048_stream_optimized.h"

using namespace std;

// RMSE threshold
#define RMSE_THRESHOLD 1e-5

int main() {
    printf("=== SOCS 2048 Stream Optimized Test Bench ===\n");
    printf("Configuration: golden_1024 (Lx=1024, nk=10, nx=8)\n\n");
    
    // Configuration parameters (golden_1024)
    const int Lx = 1024;
    const int Ly = 1024;
    const int nk = 10;
    const int nx_actual = 8;
    const int ny_actual = 8;
    const int convX_actual = 4 * nx_actual + 1;  // 33
    const int convY_actual = 4 * ny_actual + 1;  // 33
    
    // Allocate memory
    float* mskf_r = (float*)malloc(Lx * Ly * sizeof(float));
    float* mskf_i = (float*)malloc(Lx * Ly * sizeof(float));
    float* scales = (float*)malloc(nk * sizeof(float));
    float* output = (float*)malloc(convX_actual * convY_actual * sizeof(float));
    float* reference = (float*)malloc(convX_actual * convY_actual * sizeof(float));
    
    // Allocate kernel arrays (separate files for each kernel)
    float** krn_r = (float**)malloc(nk * sizeof(float*));
    float** krn_i = (float**)malloc(nk * sizeof(float*));
    for (int k = 0; k < nk; k++) {
        krn_r[k] = (float*)malloc(17 * 17 * sizeof(float));
        krn_i[k] = (float*)malloc(17 * 17 * sizeof(float));
    }
    
    // Load mask spectrum data
    printf("Loading mask spectrum data...\n");
    printf("Current working directory: %s\n", getcwd(NULL, 0));
    printf("Trying to open: data/mskf_r.bin\n");
    
    ifstream mskf_r_file("data/mskf_r.bin", ios::binary);
    ifstream mskf_i_file("data/mskf_i.bin", ios::binary);
    if (!mskf_r_file || !mskf_i_file) {
        printf("ERROR: Cannot open mask spectrum files\n");
        printf("  mskf_r_file.good() = %d\n", mskf_r_file.good());
        printf("  mskf_i_file.good() = %d\n", mskf_i_file.good());
        return 1;
    }
    mskf_r_file.read((char*)mskf_r, Lx * Ly * sizeof(float));
    mskf_i_file.read((char*)mskf_i, Lx * Ly * sizeof(float));
    mskf_r_file.close();
    mskf_i_file.close();
    printf("  Loaded mskf_r/i.bin: %d x %d\n", Lx, Ly);
    
    // Load scales
    printf("Loading scales...\n");
    ifstream scales_file("data/scales.bin", ios::binary);
    if (!scales_file) {
        printf("ERROR: Cannot open scales.bin\n");
        return 1;
    }
    scales_file.read((char*)scales, nk * sizeof(float));
    scales_file.close();
    printf("  Loaded scales.bin: nk=%d\n", nk);
    
    // Load kernels (separate files)
    printf("Loading kernels...\n");
    for (int k = 0; k < nk; k++) {
        char filename[256];
        
        // Load krn_k_r.bin
        sprintf(filename, "data/kernels/krn_%d_r.bin", k);
        ifstream krn_r_file(filename, ios::binary);
        if (!krn_r_file) {
            printf("ERROR: Cannot open %s\n", filename);
            return 1;
        }
        krn_r_file.read((char*)krn_r[k], 17 * 17 * sizeof(float));
        krn_r_file.close();
        
        // Load krn_k_i.bin
        sprintf(filename, "data/kernels/krn_%d_i.bin", k);
        ifstream krn_i_file(filename, ios::binary);
        if (!krn_i_file) {
            printf("ERROR: Cannot open %s\n", filename);
            return 1;
        }
        krn_i_file.read((char*)krn_i[k], 17 * 17 * sizeof(float));
        krn_i_file.close();
    }
    printf("  Loaded %d kernels (17x17 each)\n", nk);
    
    // Load reference output
    printf("Loading reference output...\n");
    ifstream ref_file("data/tmpImgp_pad128.bin", ios::binary);
    if (!ref_file) {
        printf("ERROR: Cannot open tmpImgp_pad128.bin\n");
        return 1;
    }
    ref_file.read((char*)reference, convX_actual * convY_actual * sizeof(float));
    ref_file.close();
    printf("  Loaded tmpImgp_pad128.bin: %d x %d\n", convX_actual, convY_actual);
    
    // Combine kernels into single arrays for HLS function
    float* krn_r_combined = (float*)malloc(nk * 17 * 17 * sizeof(float));
    float* krn_i_combined = (float*)malloc(nk * 17 * 17 * sizeof(float));
    for (int k = 0; k < nk; k++) {
        memcpy(krn_r_combined + k * 17 * 17, krn_r[k], 17 * 17 * sizeof(float));
        memcpy(krn_i_combined + k * 17 * 17, krn_i[k], 17 * 17 * sizeof(float));
    }
    
    // Run HLS function
    printf("\nRunning calc_socs_2048_hls_stream_optimized...\n");
    calc_socs_2048_hls_stream_optimized(
        mskf_r, mskf_i,
        krn_r_combined, krn_i_combined,
        scales, output,
        nk, nx_actual, ny_actual, Lx, Ly
    );
    printf("  Completed\n");
    
    // Calculate RMSE
    printf("\nCalculating RMSE...\n");
    double sum_sq_error = 0.0;
    for (int i = 0; i < convX_actual * convY_actual; i++) {
        double error = output[i] - reference[i];
        sum_sq_error += error * error;
        
        if (i < 5) {
            printf("  output[%d] = %f, reference[%d] = %f, error = %e\n",
                   i, output[i], i, reference[i], error);
        }
    }
    double rmse = sqrt(sum_sq_error / (convX_actual * convY_actual));
    printf("  RMSE = %e\n", rmse);
    
    // Check result
    printf("\n=== Result ===\n");
    if (rmse < RMSE_THRESHOLD) {
        printf("✅ PASS: RMSE = %e < %e\n", rmse, RMSE_THRESHOLD);
        
        // Save output for debugging
        ofstream out_file("output_stream_optimized.bin", ios::binary);
        out_file.write((char*)output, convX_actual * convY_actual * sizeof(float));
        out_file.close();
        printf("  Saved output to output_stream_optimized.bin\n");
        
        return 0;
    } else {
        printf("❌ FAIL: RMSE = %e >= %e\n", rmse, RMSE_THRESHOLD);
        return 1;
    }
}
