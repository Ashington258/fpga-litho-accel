/*
 * 简单1D FFT测试（只测试HLS FFT IP核心功能）
 */

#include "../src/fft_2d.h"
#include <iostream>

using namespace std;

int main() {
    cout << "Testing 1D FFT (32 points) - Simple Test\n" << endl;
    
    const int SIZE = 32;
    
    // 创建stream
    hls::stream<fft_complex_t> input_stream("input");
    hls::stream<fft_complex_t> output_stream("output");
    
    // 写入简单测试数据
    cout << "Writing input data..." << endl;
    for (int i = 0; i < SIZE; i++) {
        fft_complex_t val(1.0, 0.0);  // 全1序列（DC测试）
        input_stream.write(val);
    }
    
    // 执行FFT
    cout << "Running FFT..." << endl;
    fft_1d_forward_32(input_stream, output_stream);
    
    // 读取输出（预期：全部32点）
    cout << "Reading output (expecting 32 points)..." << endl;
    int count = 0;
    float sum = 0.0;
    
    while (!output_stream.empty() && count < SIZE) {
        fft_complex_t result = output_stream.read();
        sum += abs(result.real());
        count++;
        
        if (count <= 5) {
            cout << "  Point " << count << ": (" << result.real() << ", " << result.imag() << ")" << endl;
        }
    }
    
    cout << "\nTotal points read: " << count << endl;
    cout << "Sum of real parts: " << sum << endl;
    
    if (count == SIZE) {
        cout << "\n✓✓✓ TEST PASSED ✓✓✓" << endl;
        return 0;
    } else {
        cout << "\n✗✗✗ TEST FAILED (expected 32 points, got " << count << ") ✗✗✗" << endl;
        return 1;
    }
}
