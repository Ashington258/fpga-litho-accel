/*
 * 2D FFT测试平台（修正版）
 * SOCS HLS Project
 */

#include "../src/fft_2d.h"
#include <iostream>
#include <fstream>
#include <cmath>

using namespace std;

// ============================================================================
// 测试1D FFT（32点）
// ============================================================================

void test_1d_fft_32() {
    cout << "\n========================================" << endl;
    cout << "Testing 1D FFT (32 points)" << endl;
    cout << "========================================" << endl;
    
    const int SIZE = 32;
    
    hls::stream<fft_complex_t> input_stream;
    hls::stream<fft_complex_t> output_stream;
    
    // 写入测试数据（简单脉冲）
    for (int i = 0; i < SIZE; i++) {
        fft_complex_t val(i == 0 ? 1.0 : 0.0, 0.0);  // DC脉冲
        input_stream.write(val);
    }
    
    // 执行FFT
    fft_1d_forward_32(input_stream, output_stream);
    
    // 读取结果
    cout << "FFT Output (first 5 points):" << endl;
    for (int i = 0; i < min(5, SIZE); i++) {
        fft_complex_t result = output_stream.read();
        cout << "  Point " << i << ": (" << result.real() << ", " << result.imag() << ")" << endl;
    }
    
    cout << "✓ 1D FFT Test Completed" << endl;
}

// ============================================================================
// 测试2D FFT（32×32）
// ============================================================================

void test_2d_fft_32() {
    cout << "\n========================================" << endl;
    cout << "Testing 2D FFT (32×32)" << endl;
    cout << "========================================" << endl;
    
    const int SIZE = 32;
    
    // 创建测试数据（中心脉冲）
    fft_complex_t input[SIZE * SIZE];
    fft_complex_t output[SIZE * SIZE];
    fft_complex_t reconstructed[SIZE * SIZE];
    
    for (int i = 0; i < SIZE * SIZE; i++) {
        input[i] = fft_complex_t(0.0, 0.0);
    }
    
    // 中心点设置值
    int center = SIZE / 2;
    input[center * SIZE + center] = fft_complex_t(1.0, 0.0);
    
    // 执行2D FFT正向变换
    cout << "[1] Running 2D FFT Forward..." << endl;
    fft_2d_forward_32(input, output);
    
    // 检查输出
    float sum_real = 0.0;
    for (int i = 0; i < SIZE * SIZE; i++) {
        sum_real += abs(output[i].real());
    }
    cout << "  Output sum of real parts: " << sum_real << endl;
    
    // 执行2D IFFT反向变换
    cout << "[2] Running 2D IFFT..." << endl;
    fft_2d_inverse_32(output, reconstructed);
    
    // 检查重建误差（考虑IFFT的缩放因子）
    // FFT+IFFT应该恢复原始信号（scaled模式已经处理了1/N缩放）
    float max_error = 0.0;
    float error_sum = 0.0;
    for (int i = 0; i < SIZE * SIZE; i++) {
        float expected = input[i].real();
        float actual = reconstructed[i].real();
        float error = abs(actual - expected);
        if (error > max_error) max_error = error;
        error_sum += error;
    }
    
    cout << "  Max reconstruction error: " << max_error << endl;
    cout << "  Mean reconstruction error: " << error_sum / (SIZE * SIZE) << endl;
    
    // 打印一些重建后的样本点
    cout << "  Reconstructed center point: " << reconstructed[center * SIZE + center].real() << endl;
    cout << "  Reconstructed corner point [0,0]: " << reconstructed[0].real() << endl;
    
    if (max_error < 0.1) {
        cout << "✓ 2D FFT Test PASSED" << endl;
    } else {
        cout << "✗ 2D FFT Test FAILED" << endl;
    }
}

// ============================================================================
// 主函数
// ============================================================================

int main() {
    cout << "======================================" << endl;
    cout << "SOCS HLS 2D FFT Test Suite" << endl;
    cout << "======================================\n" << endl;
    
    test_1d_fft_32();
    test_2d_fft_32();
    
    cout << "\n======================================\n" << endl;
    cout << "All FFT Tests Completed" << endl;
    cout << "======================================\n" << endl;
    
    return 0;
}
