/**
 * Testbench for SOCS HLS Simple Implementation
 * FPGA-Litho Project
 * 
 * Tests data loading, AXI-MM interfaces, and output generation
 * Current configuration: Nx=4, IFFT 32×32, Output 17×17
 */

#include <iostream>
#include <fstream>
#include <cmath>
#include <cstring>

// Forward declaration of HLS function
extern void calc_socs_simple_hls(
    float *mskf_r,
    float *mskf_i,
    float *scales,
    float *krn_r,
    float *krn_i,
    float *output
);

// Configuration
#define Lx 512
#define Ly 512
#define Nx 4
#define Ny 4
#define convX (4*Nx + 1)  // = 17
#define convY (4*Ny + 1)  // = 17
#define kerX (2*Nx + 1)   // = 9
#define kerY (2*Ny + 1)   // = 9
#define nk 10

// Helper function to load binary data
void load_binary_data(const char* filename, float* data, int size) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "ERROR: Cannot open file: " << filename << std::endl;
        exit(1);
    }
    file.read(reinterpret_cast<char*>(data), size * sizeof(float));
    file.close();
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "SOCS HLS Simple Implementation Test" << std::endl;
    std::cout << "========================================" << std::endl;
    
    // Allocate memory
    float *mskf_r = new float[Lx * Ly];
    float *mskf_i = new float[Lx * Ly];
    float *scales = new float[nk];
    float *krn_r = new float[nk * kerX * kerY];
    float *krn_i = new float[nk * kerX * kerY];
    float *output = new float[convX * convY];
    float *golden = new float[convX * convY];
    
    // Load input data
    std::cout << "\n[STEP 1] Loading input data..." << std::endl;
    // CSim runs in: hls/socs_simple_csim_v4/hls/csim/build/
    // Data location: SOCS_HLS/data/ (go up 5 levels from build)
    load_binary_data("../../../../../data/mskf_r.bin", mskf_r, Lx * Ly);
    load_binary_data("../../../../../data/mskf_i.bin", mskf_i, Lx * Ly);
    load_binary_data("../../../../../data/scales.bin", scales, nk);
    
    // Load kernels
    for (int k = 0; k < nk; k++) {
        char filename[256];
        sprintf(filename, "../../../../../data/kernels/krn_%d_r.bin", k);
        load_binary_data(filename, &krn_r[k * kerX * kerY], kerX * kerY);
        sprintf(filename, "../../../../../data/kernels/krn_%d_i.bin", k);
        load_binary_data(filename, &krn_i[k * kerX * kerY], kerX * kerY);
    }
    
    std::cout << "  ✓ Mask spectrum loaded: " << Lx << "×" << Ly << std::endl;
    std::cout << "  ✓ Eigenvalues loaded: " << nk << std::endl;
    std::cout << "  ✓ SOCS kernels loaded: " << nk << " × " << kerX << "×" << kerY << std::endl;
    
    // Print some input data statistics
    std::cout << "\n[STEP 2] Input data statistics..." << std::endl;
    float mskf_r_min = mskf_r[0], mskf_r_max = mskf_r[0];
    for (int i = 1; i < Lx * Ly; i++) {
        if (mskf_r[i] < mskf_r_min) mskf_r_min = mskf_r[i];
        if (mskf_r[i] > mskf_r_max) mskf_r_max = mskf_r[i];
    }
    std::cout << "  Mask spectrum real: [" << mskf_r_min << ", " << mskf_r_max << "]" << std::endl;
    std::cout << "  Eigenvalue 0: " << scales[0] << std::endl;
    std::cout << "  Kernel 0 first element: " << krn_r[0] << std::endl;
    
    // Load golden output
    std::cout << "\n[STEP 3] Loading golden output..." << std::endl;
    load_binary_data("../../../../../data/tmpImgp_pad32.bin", golden, convX * convY);
    
    float golden_min = golden[0], golden_max = golden[0], golden_sum = 0.0f;
    for (int i = 1; i < convX * convY; i++) {
        if (golden[i] < golden_min) golden_min = golden[i];
        if (golden[i] > golden_max) golden_max = golden[i];
        golden_sum += golden[i];
    }
    std::cout << "  Golden range: [" << golden_min << ", " << golden_max << "]" << std::endl;
    std::cout << "  Golden mean: " << golden_sum / (convX * convY) << std::endl;
    std::cout << "  Golden size: " << convX << "×" << convY << " = " << convX * convY << std::endl;
    
    // Call HLS kernel
    std::cout << "\n[STEP 4] Running HLS kernel..." << std::endl;
    calc_socs_simple_hls(mskf_r, mskf_i, scales, krn_r, krn_i, output);
    std::cout << "  ✓ HLS kernel execution completed" << std::endl;
    
    // Check output statistics
    std::cout << "\n[STEP 5] Output statistics..." << std::endl;
    float output_min = output[0], output_max = output[0], output_sum = 0.0f;
    for (int i = 1; i < convX * convY; i++) {
        if (output[i] < output_min) output_min = output[i];
        if (output[i] > output_max) output_max = output[i];
        output_sum += output[i];
    }
    std::cout << "  Output range: [" << output_min << ", " << output_max << "]" << std::endl;
    std::cout << "  Output mean: " << output_sum / (convX * convY) << std::endl;
    
    // Compare output with golden
    std::cout << "\n[STEP 6] Comparing with golden output..." << std::endl;
    
    float rmse = 0.0f;
    float max_error = 0.0f;
    float rel_error_sum = 0.0f;
    int error_count = 0;
    const float tolerance = 1e-3f;  // Tolerance threshold
    
    for (int i = 0; i < convX * convY; i++) {
        float error = std::abs(output[i] - golden[i]);
        rmse += error * error;
        if (error > max_error) max_error = error;
        
        // Relative error (avoid division by zero)
        if (std::abs(golden[i]) > 1e-6f) {
            float rel_error = error / std::abs(golden[i]);
            rel_error_sum += rel_error;
            if (rel_error > 0.01f) error_count++;  // > 1% error
        }
        
        // Print first few errors for debugging
        if (i < 5) {
            std::cout << "  [" << i << "] Output: " << output[i] 
                      << ", Golden: " << golden[i] 
                      << ", Error: " << error << std::endl;
        }
    }
    
    rmse = std::sqrt(rmse / (convX * convY));
    float avg_rel_error = rel_error_sum / (convX * convY);
    
    std::cout << "\n  RMSE: " << rmse << std::endl;
    std::cout << "  Max error: " << max_error << std::endl;
    std::cout << "  Avg relative error: " << avg_rel_error * 100 << "%" << std::endl;
    std::cout << "  Points with >1% error: " << error_count << " / " << convX * convY << std::endl;
    
    // Validation result
    std::cout << "\n========================================" << std::endl;
    bool passed = (rmse < tolerance) && (avg_rel_error < 0.01f);
    if (passed) {
        std::cout << "TEST RESULT: PASS" << std::endl;
        std::cout << "  ✓ Algorithm implementation verified" << std::endl;
        std::cout << "  ✓ Output matches golden within tolerance" << std::endl;
    } else {
        std::cout << "TEST RESULT: FAIL" << std::endl;
        std::cout << "  ⚠️  Output does not match golden" << std::endl;
        std::cout << "  ⚠️  Need to debug algorithm implementation" << std::endl;
    }
    std::cout << "========================================" << std::endl;
    
    // Cleanup
    delete[] mskf_r;
    delete[] mskf_i;
    delete[] scales;
    delete[] krn_r;
    delete[] krn_i;
    delete[] output;
    delete[] golden;
    
    return 0;
}