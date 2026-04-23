/**
 * Optimized SOCS HLS Testbench - Full Output Validation
 * FPGA-Litho Project - Vitis FFT IP Integration
 */

#include <iostream>
#include <fstream>
#include <cmath>
#include "../src/socs_config_optimized.h"

// DUT (Design Under Test)
extern void calc_socs_optimized_hls(
    float *mskf_r,
    float *mskf_i,
    float *scales,
    float *krn_r,
    float *krn_i,
    float *output_full,     // 32×32 FI output
    float *output_center    // 17×17 center
);

// Helper: load binary data
void load_binary_data(const char *filename, float *data, int count) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open file: " << filename << std::endl;
        exit(1);
    }
    file.read(reinterpret_cast<char*>(data), count * sizeof(float));
    file.close();
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  SOCS Optimized HLS Testbench (FFT IP)" << std::endl;
    std::cout << "========================================" << std::endl;
    
    // Allocate memory
    float *mskf_r = new float[Lx * Ly];
    float *mskf_i = new float[Lx * Ly];
    float *scales = new float[nk];
    float *krn_r = new float[nk * kerX * kerY];
    float *krn_i = new float[nk * kerX * kerY];
    
    float *output_full = new float[fftConvX * fftConvY];
    float *output_center = new float[convX * convY];
    
    float *golden_full = new float[fftConvX * fftConvY];
    float *golden_center = new float[convX * convY];
    
    // Load test data
    // 使用绝对路径避免CSIM工作目录问题
    const char* data_dir = "e:/fpga-litho-accel/source/SOCS_HLS/data";
    char path_buf[512];
    
    std::cout << "\n[Step 1] Loading input data..." << std::endl;
    sprintf(path_buf, "%s/mskf_r.bin", data_dir);
    load_binary_data(path_buf, mskf_r, Lx * Ly);
    sprintf(path_buf, "%s/mskf_i.bin", data_dir);
    load_binary_data(path_buf, mskf_i, Lx * Ly);
    sprintf(path_buf, "%s/scales.bin", data_dir);
    load_binary_data(path_buf, scales, nk);
    
    // Load kernels
    for (int k = 0; k < nk; k++) {
        sprintf(path_buf, "%s/kernels/krn_%d_r.bin", data_dir, k);
        load_binary_data(path_buf, &krn_r[k * kerX * kerY], kerX * kerY);
        sprintf(path_buf, "%s/kernels/krn_%d_i.bin", data_dir, k);
        load_binary_data(path_buf, &krn_i[k * kerX * kerY], kerX * kerY);
    }
    
    std::cout << "  ✓ Mask spectrum: " << Lx << "×" << Ly << std::endl;
    std::cout << "  ✓ Eigenvalues: " << nk << std::endl;
    std::cout << "  ✓ SOCS kernels: " << nk << " of " << kerX << "×" << kerY << std::endl;
    
    // Load golden outputs
    std::cout << "\n[Step 2] Loading golden outputs..." << std::endl;
    sprintf(path_buf, "%s/tmpImgp_full_32.bin", data_dir);
    load_binary_data(path_buf, golden_full, fftConvX * fftConvY);
    
    // Extract 17×17 center from 32×32 full output (center offset: (32-17)/2 = 7)
    std::cout << "  Extracting center 17×17 from 32×32 golden..." << std::endl;
    int center_offset = (fftConvX - convX) / 2;  // 7
    for (int y = 0; y < convY; y++) {
        for (int x = 0; x < convX; x++) {
            golden_center[y * convX + x] = golden_full[(y + center_offset) * fftConvX + (x + center_offset)];
        }
    }
    
    // Execute DUT
    std::cout << "\n[Step 3] Running calc_socs_optimized_hls..." << std::endl;
    calc_socs_optimized_hls(mskf_r, mskf_i, scales, krn_r, krn_i, 
                           output_full, output_center);
    
    // Compare results
    std::cout << "\n[Step 4] Comparing results..." << std::endl;
    
    // Compute RMSE for full output
    float rmse_full = 0.0f;
    float max_diff_full = 0.0f;
    for (int i = 0; i < fftConvX * fftConvY; i++) {
        float diff = output_full[i] - golden_full[i];
        rmse_full += diff * diff;
        if (fabs(diff) > max_diff_full) {
            max_diff_full = fabs(diff);
        }
    }
    rmse_full = sqrt(rmse_full / (fftConvX * fftConvY));
    
    // Compute RMSE for center output
    float rmse_center = 0.0f;
    float max_diff_center = 0.0f;
    for (int i = 0; i < convX * convY; i++) {
        float diff = output_center[i] - golden_center[i];
        rmse_center += diff * diff;
        if (fabs(diff) > max_diff_center) {
            max_diff_center = fabs(diff);
        }
    }
    rmse_center = sqrt(rmse_center / (convX * convY));
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "  Validation Results" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Full Output (32×32):" << std::endl;
    std::cout << "  RMSE: " << rmse_full << std::endl;
    std::cout << "  Max Diff: " << max_diff_full << std::endl;
    std::cout << "\nCenter Output (17×17):" << std::endl;
    std::cout << "  RMSE: " << rmse_center << std::endl;
    std::cout << "  Max Diff: " << max_diff_center << std::endl;
    
    // Validation threshold
    const float RMSE_THRESHOLD = 1e-5f;
    bool pass_full = (rmse_full < RMSE_THRESHOLD);
    bool pass_center = (rmse_center < RMSE_THRESHOLD);
    
    std::cout << "\n========================================" << std::endl;
    if (pass_full && pass_center) {
        std::cout << "  ✓ PASS - All outputs within tolerance" << std::endl;
        std::cout << "========================================" << std::endl;
        return 0;
    } else {
        std::cout << "  ✗ FAIL - Outputs exceed tolerance" << std::endl;
        std::cout << "========================================" << std::endl;
        return 1;
    }
}