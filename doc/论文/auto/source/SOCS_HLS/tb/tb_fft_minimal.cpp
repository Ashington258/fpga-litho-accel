/**
 * Minimal FFT Test - Verify single 32-point IFFT
 * 
 * Test with known input to verify FFT IP functionality
 */
#include <iostream>
#include <fstream>
#include <cmath>
#include "socs_fft.h"

using namespace std;

// Simple test: 32-point IFFT with known input
int main() {
    cout << "\n========================================" << endl;
    cout << "Minimal FFT Test (Single 32-point IFFT)" << endl;
    cout << "========================================" << endl;
    
    // Test 1: Zero input
    cout << "\n[TEST 1] Zero input IFFT" << endl;
    cmpxData_t zero_in[32];
    cmpxData_t zero_out[32];
    
    for (int i = 0; i < 32; i++) {
        zero_in[i] = cmpxData_t(0.0f, 0.0f);
    }
    
    // Row-wise test
    hls::ip_fft::config_t<fft_config_32> fft_config;
    hls::ip_fft::status_t<fft_config_32> fft_status;
    
    fft_config.setDir(1);  // IFFT
    hls::fft<fft_config_32>(zero_in, zero_out, &fft_status, &fft_config);
    
    cout << "  Output: ";
    for (int i = 0; i < 5; i++) {
        cout << zero_out[i] << " ";
    }
    cout << endl;
    
    bool zero_nan = false;
    for (int i = 0; i < 32; i++) {
        if (isnan(zero_out[i].real()) || isnan(zero_out[i].imag())) {
            zero_nan = true;
            break;
        }
    }
    
    if (zero_nan) {
        cout << "  ❌ FAIL: Zero input produced NaN" << endl;
        return 1;
    } else {
        cout << "  ✓ PASS: Zero input IFFT works" << endl;
    }
    
    // Test 2: Known non-zero input (small values)
    cout << "\n[TEST 2] Small values IFFT" << endl;
    cmpxData_t small_in[32];
    cmpxData_t small_out[32];
    
    for (int i = 0; i < 32; i++) {
        // Small but normal floats (not denormalized)
        small_in[i] = cmpxData_t(1e-6f * (i + 1), 1e-7f * (i + 1));
    }
    
    fft_config.setDir(1);  // IFFT
    hls::fft<fft_config_32>(small_in, small_out, &fft_status, &fft_config);
    
    cout << "  Input range: [" << small_in[0] << ", " << small_in[31] << "]" << endl;
    cout << "  Output: ";
    for (int i = 0; i < 5; i++) {
        cout << small_out[i] << " ";
    }
    cout << endl;
    
    bool small_nan = false;
    for (int i = 0; i < 32; i++) {
        if (isnan(small_out[i].real()) || isnan(small_out[i].imag())) {
            small_nan = true;
            break;
        }
    }
    
    if (small_nan) {
        cout << "  ❌ FAIL: Small values produced NaN" << endl;
        return 1;
    } else {
        cout << "  ✓ PASS: Small values IFFT works" << endl;
    }
    
    // Test 3: Actual kernel data from golden
    cout << "\n[TEST 3] Actual padded data from kernel_0" << endl;
    
    // Load kernel_0 and mask
    const char* data_dir = "/root/project/FPGA-Litho/source/SOCS_HLS/data";
    float krn_r[81], krn_i[81];
    float mskf_r[262144], mskf_i[262144];
    
    ifstream krn_file(string(data_dir) + "/kernels/krn_0_r.bin", ios::binary);
    krn_file.read(reinterpret_cast<char*>(krn_r), 81 * sizeof(float));
    krn_file.close();
    
    ifstream krn_i_file(string(data_dir) + "/kernels/krn_0_i.bin", ios::binary);
    krn_i_file.read(reinterpret_cast<char*>(krn_i), 81 * sizeof(float));
    krn_i_file.close();
    
    ifstream mskf_r_file(string(data_dir) + "/mskf_r.bin", ios::binary);
    mskf_r_file.read(reinterpret_cast<char*>(mskf_r), 262144 * sizeof(float));
    mskf_r_file.close();
    
    ifstream mskf_i_file(string(data_dir) + "/mskf_i.bin", ios::binary);
    mskf_i_file.read(reinterpret_cast<char*>(mskf_i), 262144 * sizeof(float));
    mskf_i_file.close();
    
    // Build padded input (matching build_padded_ifft_input_32)
    cmpxData_t padded_in[32];
    cmpxData_t padded_out[32];
    
    // Initialize to zero
    for (int i = 0; i < 32; i++) {
        padded_in[i] = cmpxData_t(0.0f, 0.0f);
    }
    
    // Embed kernel at indices 23-31 (bottom-right corner)
    int Lxh = 256, Lyh = 256;
    int Nx = 4, Ny = 4;
    int difX = 23, difY = 23;
    int Lx_full = 512, Ly_full = 512;
    
    cout << "  Embedding kernel×mask at indices 23-31..." << endl;
    
    int nan_count = 0;
    int inf_count = 0;
    int denorm_count = 0;
    float denorm_threshold = 1e-35f;
    
    for (int ky = 0; ky < 9; ky++) {
        for (int kx = 0; kx < 9; kx++) {
            int phys_y = ky - Ny;
            int phys_x = kx - Nx;
            int mask_y = Lyh + phys_y;
            int mask_x = Lxh + phys_x;
            
            if (mask_y >= 0 && mask_y < Ly_full && mask_x >= 0 && mask_x < Lx_full) {
                float kr_r = krn_r[ky * 9 + kx];
                float kr_i = krn_i[ky * 9 + kx];
                int mask_idx = mask_y * Lx_full + mask_x;
                float ms_r = mskf_r[mask_idx];
                float ms_i = mskf_i[mask_idx];
                
                // Complex multiplication
                float prod_r = kr_r * ms_r - kr_i * ms_i;
                float prod_i = kr_r * ms_i + kr_i * ms_r;
                
                // Check denormalized
                if (abs(prod_r) < denorm_threshold) {
                    denorm_count++;
                    prod_r = 0.0f;
                }
                if (abs(prod_i) < denorm_threshold) {
                    denorm_count++;
                    prod_i = 0.0f;
                }
                
                // Check for NaN/Inf BEFORE FFT
                if (isnan(prod_r) || isnan(prod_i)) {
                    nan_count++;
                    cout << "    ❌ NaN at kernel[" << ky << "," << kx << "]: " << prod_r << ", " << prod_i << endl;
                }
                if (isinf(prod_r) || isinf(prod_i)) {
                    inf_count++;
                    cout << "    ❌ Inf at kernel[" << ky << "," << kx << "]: " << prod_r << ", " << prod_i << endl;
                }
                
                // Write to padded array (row-wise test: only one row)
                if (ky == 0) {  // Test first row
                    padded_in[difX + kx] = cmpxData_t(prod_r, prod_i);
                }
            }
        }
    }
    
    cout << "  Stats: NaN=" << nan_count << ", Inf=" << inf_count << ", Denorm=" << denorm_count << endl;
    
    if (nan_count > 0 || inf_count > 0) {
        cout << "  ❌ FAIL: NaN/Inf found BEFORE FFT (in padded input)" << endl;
        return 1;
    }
    
    // Perform IFFT on this row
    fft_config.setDir(1);  // IFFT
    hls::fft<fft_config_32>(padded_in, padded_out, &fft_status, &fft_config);
    
    cout << "  IFFT output: ";
    for (int i = 0; i < 5; i++) {
        cout << padded_out[i] << " ";
    }
    cout << endl;
    
    bool padded_nan = false;
    for (int i = 0; i < 32; i++) {
        if (isnan(padded_out[i].real()) || isnan(padded_out[i].imag())) {
            padded_nan = true;
            cout << "    ❌ NaN in IFFT output[" << i << "]: " << padded_out[i] << endl;
        }
    }
    
    if (padded_nan) {
        cout << "  ❌ FAIL: FFT produced NaN from valid input" << endl;
        return 1;
    } else {
        cout << "  ✓ PASS: FFT works with actual kernel data" << endl;
    }
    
    cout << "\n========================================" << endl;
    cout << "ALL TESTS PASSED" << endl;
    cout << "========================================" << endl;
    
    return 0;
}