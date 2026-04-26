/**
 * FFT 128-point Length Verification Test
 * 
 * PURPOSE: Verify that FFT is using 128-point (max_nfft=7) instead of 1024-point (max_nfft=10)
 * 
 * TEST METHOD:
 *   - Input: Single impulse at DC (index 0)
 *   - Expected output for 128-point IFFT: All outputs = 1/128 = 0.0078125
 *   - If output = 1/1024 = 0.000976562, then FFT is using wrong length
 * 
 * REFERENCE:
 *   - IFFT of impulse at DC: x[n] = 1/N for all n
 *   - For N=128: x[n] = 1/128 = 0.0078125
 *   - For N=1024: x[n] = 1/1024 = 0.000976562
 */
#include <iostream>
#include <cmath>
#include <hls_stream.h>
#include "../src/hls_fft_config_2048_corrected.h"

using namespace std;

// Test 128-point IFFT with impulse input
int main() {
    cout << "\n==========================================" << endl;
    cout << "FFT 128-point Length Verification Test" << endl;
    cout << "==========================================" << endl;
    
    // Create impulse at DC (index 0)
    hls::stream<cmpx_fft_in_t> in_stream("input_stream");
    hls::stream<cmpx_fft_out_t> out_stream("output_stream");
    
    // Input: impulse at DC
    // For IFFT: X[0] = 1, X[k] = 0 for k != 0
    // Expected output: x[n] = 1/N for all n
    
    cout << "\n[TEST] Impulse at DC (128-point IFFT)" << endl;
    cout << "  Input: X[0] = 1, X[1..127] = 0" << endl;
    cout << "  Expected output: x[n] = 1/128 = 0.0078125 for all n" << endl;
    cout << "  If output = 1/1024 = 0.000976562, FFT is using wrong length!" << endl;
    
    // Write impulse to stream
    for (int i = 0; i < 128; i++) {
        cmpx_fft_in_t sample;
        if (i == 0) {
            // DC component = 1.0
            // WARNING: ap_fixed<24,1> cannot represent 1.0 exactly!
            // It will overflow to -1.0
            // Use 0.9999999 instead
            sample = cmpx_fft_in_t(0.9999999, 0.0);
        } else {
            sample = cmpx_fft_in_t(0.0, 0.0);
        }
        in_stream.write(sample);
    }
    
    // Run IFFT using the correct API from hls_fft_config_2048_corrected.h
    // API: hls::fft<config>(in_stream, out_stream, dir, scaling, cp_len, &ovflo, &blk_exp)
    ap_uint<1> dir = 1;  // IFFT
    // CRITICAL: For nfft=7 (odd), final stage scaling must be 0 or 1
    // Default 0x2AA = 0b1010101010 has final stage = 2 (INVALID for odd nfft)
    // Use 0x155 = 0b101010101 (each stage = 1, valid for odd nfft)
    ap_uint<15> scaling = 0x155;  // Scaled mode with correct schedule
    bool ovflo;
    unsigned blk_exp;
    
    hls::fft<config_socs_fft>(in_stream, out_stream, dir, scaling, -1, &ovflo, &blk_exp);
    
    // Read output
    cout << "\n  Output samples:" << endl;
    double sum = 0.0;
    double first_val = 0.0;
    bool all_same = true;
    bool has_nan = false;
    
    for (int i = 0; i < 128; i++) {
        cmpx_fft_out_t sample = out_stream.read();
        double real_val = sample.real().to_double();
        double imag_val = sample.imag().to_double();
        
        if (i == 0) {
            first_val = real_val;
        } else if (fabs(real_val - first_val) > 1e-6) {
            all_same = false;
        }
        
        if (isnan(real_val) || isnan(imag_val)) {
            has_nan = true;
        }
        
        sum += real_val;
        
        if (i < 5 || i >= 123) {
            cout << "    x[" << i << "] = " << real_val << " + " << imag_val << "j" << endl;
        } else if (i == 5) {
            cout << "    ..." << endl;
        }
    }
    
    cout << "\n  Analysis:" << endl;
    cout << "    First value: " << first_val << endl;
    cout << "    Sum of all values: " << sum << endl;
    cout << "    All values same: " << (all_same ? "YES" : "NO") << endl;
    cout << "    Has NaN: " << (has_nan ? "YES" : "NO") << endl;
    
    // Determine FFT length from output
    double expected_128 = 1.0 / 128.0;  // 0.0078125
    double expected_1024 = 1.0 / 1024.0;  // 0.000976562
    
    cout << "\n  Expected for 128-point: " << expected_128 << endl;
    cout << "  Expected for 1024-point: " << expected_1024 << endl;
    
    double diff_128 = fabs(first_val - expected_128);
    double diff_1024 = fabs(first_val - expected_1024);
    
    cout << "  Difference from 128-point: " << diff_128 << endl;
    cout << "  Difference from 1024-point: " << diff_1024 << endl;
    
    // Check result
    if (has_nan) {
        cout << "\n  ❌ FAIL: Output contains NaN" << endl;
        return 1;
    }
    
    if (diff_128 < diff_1024) {
        cout << "\n  ✓ PASS: FFT is using 128-point (max_nfft=7)" << endl;
        cout << "  Output matches expected 1/128 = " << expected_128 << endl;
        return 0;
    } else {
        cout << "\n  ❌ FAIL: FFT is using 1024-point (max_nfft=10 default)" << endl;
        cout << "  Output matches 1/1024 = " << expected_1024 << endl;
        cout << "  FIX: Check config_socs_fft struct parameters" << endl;
        return 1;
    }
}
