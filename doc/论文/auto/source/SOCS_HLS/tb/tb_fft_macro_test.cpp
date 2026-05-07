// Compile-time test to verify _CONFIG_T_max_nfft macro expansion
#include <iostream>
#include "../src/hls_fft_config_2048_corrected.h"

// Manually expand the macro to see what value it produces
// This simulates what the HLS FFT library does internally

int main() {
    std::cout << "==========================================" << std::endl;
    std::cout << "_CONFIG_T_max_nfft Macro Expansion Test" << std::endl;
    std::cout << "==========================================" << std::endl;
    
    // The macro is defined as:
    // #define _CONFIG_T_max_nfft \
    //   HLS_FFT_CHECK(log2_transform_length, max_nfft, 10)
    //
    // Which expands to:
    // ((_CONFIG_T::log2_transform_length) != 10 ? (_CONFIG_T::log2_transform_length) : (_CONFIG_T::old_name))
    
    // Manually compute what the macro should return
    unsigned manual_result = (config_socs_fft::log2_transform_length != 10) 
                           ? config_socs_fft::log2_transform_length 
                           : config_socs_fft::max_nfft;
    
    std::cout << "\nManual macro expansion:" << std::endl;
    std::cout << "  config_socs_fft::log2_transform_length = " << config_socs_fft::log2_transform_length << std::endl;
    std::cout << "  config_socs_fft::max_nfft = " << config_socs_fft::max_nfft << std::endl;
    std::cout << "  (log2_transform_length != 10) ? log2_transform_length : max_nfft" << std::endl;
    std::cout << "  = (" << config_socs_fft::log2_transform_length << " != 10) ? " 
              << config_socs_fft::log2_transform_length << " : " << config_socs_fft::max_nfft << std::endl;
    std::cout << "  = " << manual_result << std::endl;
    
    // Expected FFT length
    unsigned expected_length = 1 << manual_result;
    std::cout << "\nExpected FFT length: 1 << " << manual_result << " = " << expected_length << std::endl;
    
    if (manual_result == 7) {
        std::cout << "\n✓ PASS: Macro should expand to 7 (128-point FFT)" << std::endl;
    } else {
        std::cout << "\n❌ FAIL: Macro expands to " << manual_result << " (expected 7)" << std::endl;
        return 1;
    }
    
    // Now test the actual FFT to see what length it uses
    // We'll use the array-based FFT API which is simpler
    std::cout << "\n==========================================" << std::endl;
    std::cout << "Testing actual FFT length" << std::endl;
    std::cout << "==========================================" << std::endl;
    
    // Create input/output arrays
    std::complex<ap_fixed<24, 1>> xn[1024];  // Max size
    std::complex<ap_fixed<24, 1>> xk[1024];  // Max size
    
    // Initialize with impulse
    for (int i = 0; i < 1024; i++) {
        if (i == 0) {
            xn[i] = std::complex<ap_fixed<24, 1>>(0.9999999, 0.0);
        } else {
            xn[i] = std::complex<ap_fixed<24, 1>>(0.0, 0.0);
        }
        xk[i] = std::complex<ap_fixed<24, 1>>(0.0, 0.0);
    }
    
    // Run IFFT
    // CRITICAL: For nfft=7 (odd), final stage scaling must be 0 or 1
    // Default 0x2AA = 0b1010101010 has final stage = 2 (INVALID)
    // Use 0x155 = 0b101010101 (each stage = 1, valid for odd nfft)
    bool ovflo;
    unsigned blk_exp;
    hls::fft<config_socs_fft>(xn, xk, true, 0x155, -1, &ovflo, &blk_exp);  // IFFT with correct scaling
    
    // Check output
    double first_val = xk[0].real().to_double();
    std::cout << "\nIFFT output (impulse at DC):" << std::endl;
    std::cout << "  xk[0] = " << first_val << std::endl;
    std::cout << "  xk[1] = " << xk[1].real().to_double() << std::endl;
    std::cout << "  xk[127] = " << xk[127].real().to_double() << std::endl;
    std::cout << "  xk[128] = " << xk[128].real().to_double() << std::endl;
    
    double expected_128 = 1.0 / 128.0;
    double expected_1024 = 1.0 / 1024.0;
    
    std::cout << "\nExpected for 128-point: " << expected_128 << std::endl;
    std::cout << "Expected for 1024-point: " << expected_1024 << std::endl;
    
    if (fabs(first_val - expected_128) < fabs(first_val - expected_1024)) {
        std::cout << "\n✓ PASS: FFT is using 128-point" << std::endl;
        return 0;
    } else {
        std::cout << "\n❌ FAIL: FFT is using 1024-point" << std::endl;
        return 1;
    }
}
