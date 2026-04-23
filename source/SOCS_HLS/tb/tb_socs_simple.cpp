/**
 * Testbench for SOCS HLS Simple Implementation - Full Output Version
 * FPGA-Litho Project
 * 
 * Tests data loading, AXI-MM interfaces, and output generation
 * Current configuration: Nx=4, IFFT 32×32
 * Output: FULL 32×32 (for FI validation) + CENTER 17×17 (for compatibility)
 */

#include <iostream>
#include <fstream>
#include <cmath>
#include <cstring>

// Forward declaration of HLS function (dual output)
extern void calc_socs_simple_hls(
    float *mskf_r,
    float *mskf_i,
    float *scales,
    float *krn_r,
    float *krn_i,
    float *output_full,    // 32×32 output for FI
    float *output_center   // 17×17 center output
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
#define fftConvX 32      // FFT padded size
#define fftConvY 32
#define nk 4             // FIXED: Match actual scales.bin length (was 10, caused garbage data read)

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
    
    // Dual output buffers
    float *output_full = new float[fftConvX * fftConvY];     // 32×32 for FI validation
    float *output_center = new float[convX * convY];         // 17×17 for compatibility
    
    // Golden references
    float *golden_full = new float[fftConvX * fftConvY];     // 32×32 full tmpImgp
    float *golden_center = new float[convX * convY];         // 17×17 center
    
    // Load input data
    std::cout << "\n[STEP 1] Loading input data..." << std::endl;
    // Use absolute path to avoid CSIM working directory issues
    const char* data_dir = "e:/fpga-litho-accel/source/SOCS_HLS/data";
    char path_buf[512];
    
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
    
    // Load golden outputs
    std::cout << "\n[STEP 3] Loading golden outputs..." << std::endl;
    
    // Load 32×32 full golden (for FI validation)
    sprintf(path_buf, "%s/tmpImgp_full_32.bin", data_dir);
    load_binary_data(path_buf, golden_full, fftConvX * fftConvY);
    
    float golden_full_min = golden_full[0], golden_full_max = golden_full[0];
    float golden_full_sum = 0.0f;
    for (int i = 1; i < fftConvX * fftConvY; i++) {
        if (golden_full[i] < golden_full_min) golden_full_min = golden_full[i];
        if (golden_full[i] > golden_full_max) golden_full_max = golden_full[i];
        golden_full_sum += golden_full[i];
    }
    std::cout << "  Golden 32×32 range: [" << golden_full_min << ", " << golden_full_max << "]" << std::endl;
    std::cout << "  Golden 32×32 mean: " << golden_full_sum / (fftConvX * fftConvY) << std::endl;
    
    // Load 17×17 center golden (for compatibility check)
    sprintf(path_buf, "%s/tmpImgp_pad32.bin", data_dir);
    load_binary_data(path_buf, golden_center, convX * convY);
    
    float golden_center_min = golden_center[0], golden_center_max = golden_center[0];
    float golden_center_sum = 0.0f;
    for (int i = 1; i < convX * convY; i++) {
        if (golden_center[i] < golden_center_min) golden_center_min = golden_center[i];
        if (golden_center[i] > golden_center_max) golden_center_max = golden_center[i];
        golden_center_sum += golden_center[i];
    }
    std::cout << "  Golden 17×17 range: [" << golden_center_min << ", " << golden_center_max << "]" << std::endl;
    std::cout << "  Golden 17×17 mean: " << golden_center_sum / (convX * convY) << std::endl;
    
    // Call HLS kernel (dual output)
    std::cout << "\n[STEP 4] Running HLS kernel..." << std::endl;
    calc_socs_simple_hls(mskf_r, mskf_i, scales, krn_r, krn_i, output_full, output_center);
    std::cout << "  ✓ HLS kernel execution completed" << std::endl;
    
    // Check 32×32 full output statistics
    std::cout << "\n[STEP 5] Output statistics (32×32 full)..." << std::endl;
    float full_min = output_full[0], full_max = output_full[0], full_sum = 0.0f;
    for (int i = 1; i < fftConvX * fftConvY; i++) {
        if (output_full[i] < full_min) full_min = output_full[i];
        if (output_full[i] > full_max) full_max = output_full[i];
        full_sum += output_full[i];
    }
    std::cout << "  Output 32×32 range: [" << full_min << ", " << full_max << "]" << std::endl;
    std::cout << "  Output 32×32 mean: " << full_sum / (fftConvX * fftConvY) << std::endl;
    
    // Compare 32×32 full output with golden
    std::cout << "\n[STEP 6] Comparing 32×32 full output with golden..." << std::endl;
    
    float rmse_full = 0.0f;
    float max_error_full = 0.0f;
    
    for (int i = 0; i < fftConvX * fftConvY; i++) {
        float error = std::abs(output_full[i] - golden_full[i]);
        rmse_full += error * error;
        if (error > max_error_full) max_error_full = error;
        
        // Print first few errors for debugging
        if (i < 5) {
            std::cout << "  [" << i << "] Output: " << output_full[i] 
                      << ", Golden: " << golden_full[i] 
                      << ", Error: " << error << std::endl;
        }
    }
    
    rmse_full = std::sqrt(rmse_full / (fftConvX * fftConvY));
    std::cout << "\n  32×32 RMSE: " << rmse_full << std::endl;
    std::cout << "  32×32 Max error: " << max_error_full << std::endl;
    
    // Compare 17×17 center output with golden
    std::cout << "\n[STEP 7] Comparing 17×17 center output with golden..." << std::endl;
    
    float rmse_center = 0.0f;
    float max_error_center = 0.0f;
    
    for (int i = 0; i < convX * convY; i++) {
        float error = std::abs(output_center[i] - golden_center[i]);
        rmse_center += error * error;
        if (error > max_error_center) max_error_center = error;
        
        // Print first few errors for debugging
        if (i < 5) {
            std::cout << "  [" << i << "] Output: " << output_center[i] 
                      << ", Golden: " << golden_center[i] 
                      << ", Error: " << error << std::endl;
        }
    }
    
    rmse_center = std::sqrt(rmse_center / (convX * convY));
    std::cout << "\n  17×17 RMSE: " << rmse_center << std::endl;
    std::cout << "  17×17 Max error: " << max_error_center << std::endl;
    
    // Validation result
    std::cout << "\n========================================" << std::endl;
    const float tolerance = 1e-5f;  // Tolerance for full 32×32 (FI requirement)
    
    bool passed_full = (rmse_full < tolerance);
    bool passed_center = (rmse_center < 1e-3f);  // Relaxed tolerance for center
    
    // Save HLS outputs regardless of test result (for diagnosis)
    std::cout << "\n[STEP 8] Saving HLS outputs..." << std::endl;
    std::ofstream out_full("../../../../data/hls_output_full_32.bin", std::ios::binary);
    if (out_full.is_open()) {
        out_full.write(reinterpret_cast<char*>(output_full), fftConvX * fftConvY * sizeof(float));
        out_full.close();
        std::cout << "  ✓ 32×32 full output saved to: data/hls_output_full_32.bin" << std::endl;
    }
    
    std::ofstream out_center("../../../../data/hls_output_center_17.bin", std::ios::binary);
    if (out_center.is_open()) {
        out_center.write(reinterpret_cast<char*>(output_center), convX * convY * sizeof(float));
        out_center.close();
        std::cout << "  ✓ 17×17 center output saved to: data/hls_output_center_17.bin" << std::endl;
    }
    
    if (passed_full && passed_center) {
        std::cout << "TEST RESULT: PASS" << std::endl;
        std::cout << "  ✓ 32×32 full output verified (RMSE: " << rmse_full << ")" << std::endl;
        std::cout << "  ✓ 17×17 center output verified (RMSE: " << rmse_center << ")" << std::endl;
        std::cout << "  ✓ Ready for FI validation with full 32×32 spectrum" << std::endl;
    } else {
        std::cout << "TEST RESULT: FAIL" << std::endl;
        if (!passed_full) {
            std::cout << "  ⚠️ 32×32 full output mismatch (RMSE: " << rmse_full << ")" << std::endl;
        }
        if (!passed_center) {
            std::cout << "  ⚠️ 17×17 center output mismatch (RMSE: " << rmse_center << ")" << std::endl;
        }
    }
    std::cout << "========================================" << std::endl;
    
    // Cleanup
    delete[] mskf_r;
    delete[] mskf_i;
    delete[] scales;
    delete[] krn_r;
    delete[] krn_i;
    delete[] output_full;
    delete[] output_center;
    delete[] golden_full;
    delete[] golden_center;
    
    return 0;
}