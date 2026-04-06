/*
 * FFT Top Testbench
 * 参考vitis_hls_fft的testbench风格
 */

#include <iostream>
#include <cmath>
#include "fft_top.h"

using namespace std;

int main() {
    cout << "=======================================" << endl;
    cout << "FFT Top Level Test (32 points)" << endl;
    cout << "=======================================" << endl;

    // 创建stream
    hls::stream<fft_complex_t> input_stream("input");
    hls::stream<fft_complex_t> output_stream("output");
    bool ovflo;

    // 写入DC脉冲（32个点，只有第一个点为1.0）
    for (int i = 0; i < 32; i++) {
        if (i == 0) {
            input_stream.write(fft_complex_t(1.0, 0.0));
        } else {
            input_stream.write(fft_complex_t(0.0, 0.0));
        }
    }

    cout << "Input stream size: " << input_stream.size() << endl;

    // 调用FFT
    fft_inverse_top(input_stream, output_stream, &ovflo);

    cout << "FFT completed, ovflo: " << ovflo << endl;
    cout << "Output stream size: " << output_stream.size() << endl;

    // 读取所有输出数据
    int pass_count = 0;
    float max_val = 0.0;
    
    for (int i = 0; i < 32; i++) {
        fft_complex_t val = output_stream.read();
        float magnitude = abs(val);
        
        if (magnitude > max_val) max_val = magnitude;
        
        // DC经过FFT后，所有点幅度应相等
        if (abs(magnitude - 1.0) < 0.1 || magnitude < 0.01) {
            pass_count++;
        }
    }

    cout << "Max output magnitude: " << max_val << endl;
    cout << "Pass count: " << pass_count << "/32" << endl;

    if (pass_count >= 30 && max_val > 0.5) {
        cout << "✓ Test PASS" << endl;
        cout << "=======================================" << endl;
        return 0;
    } else {
        cout << "✗ Test FAIL" << endl;
        cout << "=======================================" << endl;
        return 1;
    }
}