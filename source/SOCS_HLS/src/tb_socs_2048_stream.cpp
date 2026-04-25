/**
 * @file tb_socs_2048_stream.cpp
 * @brief Test bench for SOCS HLS stream-based implementation
 */

#include "socs_2048_stream.h"
#include <cstdio>
#include <cmath>
#include <fstream>
#include <vector>

using namespace std;

// RMSE threshold
#define RMSE_THRESHOLD 1e-5

// Helper function to read binary file
void read_binary_file(const char* filename, float* data, int size) {
    FILE* fp = fopen(filename, "rb");
    if (!fp) {
        printf("ERROR: Cannot open file %s\n", filename);
        exit(1);
    }
    fread(data, sizeof(float), size, fp);
    fclose(fp);
}

// Helper function to write binary file
void write_binary_file(const char* filename, float* data, int size) {
    FILE* fp = fopen(filename, "wb");
    if (!fp) {
        printf("ERROR: Cannot open file %s\n", filename);
        exit(1);
    }
    fwrite(data, sizeof(float), size, fp);
    fclose(fp);
}

// Calculate RMSE
float calculate_rmse(float* output, float* reference, int size) {
    float sum_sq_error = 0.0f;
    for (int i = 0; i < size; i++) {
        float error = output[i] - reference[i];
        sum_sq_error += error * error;
    }
    return sqrt(sum_sq_error / size);
}

int main() {
    printf("========================================\n");
    printf("SOCS HLS Stream-based Implementation Test\n");
    printf("========================================\n\n");
    
    // Test configuration (golden_1024)
    int Lx = 1024;
    int Ly = 1024;
    int nk = 10;
    int nx_actual = 8;
    int ny_actual = 8;
    
    // Calculate sizes
    int kerX_actual = 2 * nx_actual + 1;  // 17
    int kerY_actual = 2 * ny_actual + 1;  // 17
    int convX_actual = 4 * nx_actual + 1; // 33
    int convY_actual = 4 * ny_actual + 1; // 33
    
    printf("Configuration:\n");
    printf("  Lx = %d, Ly = %d\n", Lx, Ly);
    printf("  nk = %d\n", nk);
    printf("  nx_actual = %d, ny_actual = %d\n", nx_actual, ny_actual);
    printf("  kerX = %d, kerY = %d\n", kerX_actual, kerY_actual);
    printf("  convX = %d, convY = %d\n", convX_actual, convY_actual);
    printf("\n");
    
    // Allocate memory
    float* mskf_r = new float[Lx * Ly];
    float* mskf_i = new float[Lx * Ly];
    float* krn_r = new float[nk * kerX_actual * kerY_actual];
    float* krn_i = new float[nk * kerX_actual * kerY_actual];
    float* scales = new float[nk];
    float* output = new float[convX_actual * convY_actual];
    float* reference = new float[convX_actual * convY_actual];
    
    // Load input data
    printf("Loading input data...\n");
    read_binary_file("data/mskf_r.bin", mskf_r, Lx * Ly);
    read_binary_file("data/mskf_i.bin", mskf_i, Lx * Ly);
    
    // Load kernels (separate files for each kernel)
    for (int k = 0; k < nk; k++) {
        char fname_r[256], fname_i[256];
        sprintf(fname_r, "data/kernels/krn_%d_r.bin", k);
        sprintf(fname_i, "data/kernels/krn_%d_i.bin", k);
        read_binary_file(fname_r, &krn_r[k * kerX_actual * kerY_actual], kerX_actual * kerY_actual);
        read_binary_file(fname_i, &krn_i[k * kerX_actual * kerY_actual], kerX_actual * kerY_actual);
    }
    
    read_binary_file("data/scales.bin", scales, nk);
    
    // Load reference output (33x33 = 1089 floats)
    printf("Loading reference output...\n");
    read_binary_file("data/tmpImgp_pad128.bin", reference, convX_actual * convY_actual);
    
    // Run HLS IP
    printf("\nRunning SOCS HLS (Stream-based)...\n");
    calc_socs_2048_hls_stream(
        mskf_r, mskf_i, krn_r, krn_i,
        scales, output, nk, nx_actual, ny_actual, Lx, Ly
    );
    
    // Calculate RMSE
    printf("\nCalculating RMSE...\n");
    float rmse = calculate_rmse(output, reference, convX_actual * convY_actual);
    printf("RMSE = %.6e\n", rmse);
    
    // Check result
    printf("\n");
    if (rmse < RMSE_THRESHOLD) {
        printf("✅ TEST PASSED (RMSE < %.1e)\n", RMSE_THRESHOLD);
    } else {
        printf("❌ TEST FAILED (RMSE >= %.1e)\n", RMSE_THRESHOLD);
    }
    
    // Save output
    printf("\nSaving output...\n");
    write_binary_file("output_stream.bin", output, convX_actual * convY_actual);
    
    // Print sample values
    printf("\nSample output values:\n");
    for (int i = 0; i < 5 && i < convX_actual * convY_actual; i++) {
        printf("  output[%d] = %.6f, reference[%d] = %.6f, error = %.6e\n",
               i, output[i], i, reference[i], output[i] - reference[i]);
    }
    
    // Cleanup
    delete[] mskf_r;
    delete[] mskf_i;
    delete[] krn_r;
    delete[] krn_i;
    delete[] scales;
    delete[] output;
    delete[] reference;
    
    printf("\n========================================\n");
    printf("Test completed\n");
    printf("========================================\n");
    
    return (rmse < RMSE_THRESHOLD) ? 0 : 1;
}
