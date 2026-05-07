// Compile-time test to verify FFT configuration
#include <iostream>
#include "../src/hls_fft_config_2048_corrected.h"

int main() {
    std::cout << "==========================================" << std::endl;
    std::cout << "FFT Configuration Verification" << std::endl;
    std::cout << "==========================================" << std::endl;
    
    // Check config_socs_fft parameters
    std::cout << "\nconfig_socs_fft parameters:" << std::endl;
    std::cout << "  log2_transform_length = " << config_socs_fft::log2_transform_length << std::endl;
    std::cout << "  max_nfft (inherited) = " << config_socs_fft::max_nfft << std::endl;
    std::cout << "  input_width = " << config_socs_fft::input_width << std::endl;
    std::cout << "  output_width = " << config_socs_fft::output_width << std::endl;
    
    // Check what _CONFIG_T_max_nfft macro expands to
    // This is the critical value that determines FFT length
    std::cout << "\nExpected FFT length:" << std::endl;
    std::cout << "  1 << log2_transform_length = " << (1 << config_socs_fft::log2_transform_length) << std::endl;
    std::cout << "  1 << max_nfft = " << (1 << config_socs_fft::max_nfft) << std::endl;
    
    // Check if log2_transform_length is set correctly
    if (config_socs_fft::log2_transform_length == 7) {
        std::cout << "\n✓ PASS: log2_transform_length = 7 (128-point FFT)" << std::endl;
    } else {
        std::cout << "\n❌ FAIL: log2_transform_length = " << config_socs_fft::log2_transform_length 
                  << " (expected 7)" << std::endl;
        return 1;
    }
    
    // Check if max_nfft is still default (10)
    if (config_socs_fft::max_nfft == 10) {
        std::cout << "  Note: max_nfft is still 10 (inherited from params_t)" << std::endl;
        std::cout << "  This is OK if log2_transform_length is set correctly" << std::endl;
    }
    
    return 0;
}
