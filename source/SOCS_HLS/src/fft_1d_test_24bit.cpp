/**
 * @file fft_1d_test_24bit.cpp
 * @brief Minimal 1D FFT test with ap_fixed<24,1> to validate precision
 * 
 * Purpose: Test if ap_fixed<24,1> works correctly in Co-Simulation
 * Expected: If this fails, the precision is the issue; if it passes, the issue is in v11's usage
 */

#include <hls_stream.h>
#include <hls_fft.h>
#include <ap_fixed.h>
#include <iostream>

using namespace std;

// Test configuration: ap_fixed<24,1> (same as v11)
const int FFT_LENGTH_24 = 128;
const char FFT_INPUT_WIDTH_24 = 24;
const char FFT_OUTPUT_WIDTH_24 = 24;

typedef ap_fixed<FFT_INPUT_WIDTH_24, 1> data_in_24_t;
typedef ap_fixed<FFT_OUTPUT_WIDTH_24, 1> data_out_24_t;
typedef complex<data_in_24_t> cmpxDataIn24;
typedef complex<data_out_24_t> cmpxDataOut24;

// FFT configuration struct (must inherit from hls::ip_fft::params_t)
struct config_24bit : hls::ip_fft::params_t {
    // CRITICAL: Set both old and new parameter names to ensure compatibility
    static const unsigned max_nfft = 7;  // Old parameter name
    static const unsigned log2_transform_length = 7;  // New parameter name (log2(128) = 7)
    static const unsigned input_width = 24;
    static const unsigned output_width = 24;
    static const unsigned output_ordering = hls::ip_fft::natural_order;
    static const unsigned implementation_options = hls::ip_fft::pipelined_streaming_io;
    static const unsigned scaling_options = hls::ip_fft::scaled;
    static const bool has_nfft = false;  // Old parameter name
    static const bool run_time_configurable_transform_length = false;  // New parameter name
};

/**
 * @brief 1D FFT test function with ap_fixed<24,1>
 */
void fft_1d_test_24bit(
    hls::stream<cmpxDataIn24> &xn,
    hls::stream<cmpxDataOut24> &xk,
    bool is_inverse
) {
    #pragma HLS INTERFACE axis port=xn
    #pragma HLS INTERFACE axis port=xk
    
    ap_uint<1> dir = is_inverse ? 1 : 0;
    ap_uint<15> scaling = 0x7F;  // Scaled mode: divide by 2^7 = 128
    bool status;
    
    hls::fft<config_24bit>(xn, xk, dir, scaling, -1, &status);
}

/**
 * @brief Testbench for 24-bit FFT
 */
int main() {
    hls::stream<cmpxDataIn24> input_stream("input_stream");
    hls::stream<cmpxDataOut24> output_stream("output_stream");
    
    // Test 1: Forward FFT with impulse input
    cout << "Test 1: Forward FFT (impulse input)" << endl;
    
    // Create impulse: [1, 0, 0, ..., 0]
    for (int i = 0; i < FFT_LENGTH_24; i++) {
        cmpxDataIn24 sample;
        if (i == 0) {
            sample.real(data_in_24_t(1.0));  // Impulse at DC
            sample.imag(data_in_24_t(0.0));
        } else {
            sample.real(data_in_24_t(0.0));
            sample.imag(data_in_24_t(0.0));
        }
        input_stream.write(sample);
    }
    
    // Run FFT
    fft_1d_test_24bit(input_stream, output_stream, false);
    
    // Check output (should be all 1/128 = 0.0078125 for scaled mode)
    cout << "Output samples (first 10):" << endl;
    int nan_count = 0;
    for (int i = 0; i < 10; i++) {
        cmpxDataOut24 sample = output_stream.read();
        float real_val = sample.real().to_float();
        float imag_val = sample.imag().to_float();
        
        cout << "  [" << i << "] real=" << real_val << ", imag=" << imag_val;
        
        if (isnan(real_val) || isnan(imag_val)) {
            cout << " [NaN DETECTED!]";
            nan_count++;
        }
        cout << endl;
    }
    
    // Read remaining samples
    for (int i = 10; i < FFT_LENGTH_24; i++) {
        cmpxDataOut24 sample = output_stream.read();
        if (isnan(sample.real().to_float()) || isnan(sample.imag().to_float())) {
            nan_count++;
        }
    }
    
    cout << endl;
    cout << "========================================" << endl;
    cout << "Test Results:" << endl;
    cout << "========================================" << endl;
    cout << "Total NaN count: " << nan_count << " / " << FFT_LENGTH_24 << endl;
    
    if (nan_count > 0) {
        cout << "[FAIL] NaN detected in output!" << endl;
        return 1;
    } else {
        cout << "[PASS] No NaN detected" << endl;
        return 0;
    }
}
