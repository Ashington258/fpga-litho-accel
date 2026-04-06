/**
 * @file tb_tcc.cpp
 * @brief HLS TCC模块测试平台 - Phase 0骨架
 * 
 * 用途：
 * - 验证HLS实现与CPU参考的一致性
 * - 提供统计对比功能（相对误差 < 1e-5）
 * 
 * 参考：
 * - FFT测试框架：reference/vitis_hls_ftt的实现/interface_stream/fft_tb.cpp
 * - CPU参考输出：output/reference/
 */

#include <iostream>
#include <fstream>
#include <cmath>
#include <vector>
#include <string>
#include "data_types.h"

using namespace std;

// ============================================================================
// 文件IO辅助函数
// ============================================================================
bool load_binary_file(const string& filename, vector<data_t>& buffer, int expected_size) {
    ifstream file(filename, ios::binary);
    if (!file.is_open()) {
        cerr << "ERROR: Cannot open file: " << filename << endl;
        return false;
    }
    
    // 读取所有数据
    file.seekg(0, ios::end);
    int file_size = file.tellg();
    file.seekg(0, ios::beg);
    
    int num_elements = file_size / sizeof(data_t);
    if (num_elements != expected_size) {
        cerr << "ERROR: File size mismatch. Expected " << expected_size 
             << " elements, got " << num_elements << endl;
        return false;
    }
    
    buffer.resize(num_elements);
    file.read(reinterpret_cast<char*>(buffer.data()), file_size);
    file.close();
    
    return true;
}

bool load_complex_binary(const string& filename_real, const string& filename_imag,
                         vector<cmpxData_t>& buffer, int expected_size) {
    vector<data_t> real_part, imag_part;
    
    if (!load_binary_file(filename_real, real_part, expected_size)) {
        return false;
    }
    if (!load_binary_file(filename_imag, imag_part, expected_size)) {
        return false;
    }
    
    buffer.resize(expected_size);
    for (int i = 0; i < expected_size; i++) {
        buffer[i] = cmpxData_t(real_part[i], imag_part[i]);
    }
    
    return true;
}

// ============================================================================
// 统计计算函数
// ============================================================================
data_t compute_mean(const vector<data_t>& data) {
    data_t sum = 0.0;
    for (const auto& val : data) {
        sum += val;
    }
    return sum / data.size();
}

data_t compute_stddev(const vector<data_t>& data, data_t mean) {
    data_t sum_sq = 0.0;
    for (const auto& val : data) {
        data_t diff = val - mean;
        sum_sq += diff * diff;
    }
    return sqrt(sum_sq / data.size());
}

data_t compute_max(const vector<data_t>& data) {
    data_t max_val = data[0];
    for (const auto& val : data) {
        if (val > max_val) max_val = val;
    }
    return max_val;
}

data_t compute_min(const vector<data_t>& data) {
    data_t min_val = data[0];
    for (const auto& val : data) {
        if (val < min_val) min_val = val;
    }
    return min_val;
}

// ============================================================================
// 验证函数
// ============================================================================
bool verify_results(const vector<data_t>& hls_output, const vector<data_t>& golden,
                    const string& name, data_t tolerance = 1e-5) {
    cout << "=== Verifying " << name << " ===" << endl;
    
    // 计算统计信息
    data_t hls_mean = compute_mean(hls_output);
    data_t golden_mean = compute_mean(golden);
    data_t hls_std = compute_stddev(hls_output, hls_mean);
    data_t golden_std = compute_stddev(golden, golden_mean);
    
    cout << "HLS Mean:   " << hls_mean << endl;
    cout << "Golden Mean: " << golden_mean << endl;
    cout << "Relative Error (Mean): " << abs((hls_mean - golden_mean) / golden_mean) << endl;
    cout << endl;
    cout << "HLS StdDev:   " << hls_std << endl;
    cout << "Golden StdDev: " << golden_std << endl;
    cout << "Relative Error (StdDev): " << abs((hls_std - golden_std) / golden_std) << endl;
    cout << endl;
    
    // 检查相对误差
    bool pass = true;
    data_t mean_error = abs((hls_mean - golden_mean) / golden_mean);
    data_t std_error = abs((hls_std - golden_std) / golden_std);
    
    if (mean_error > tolerance) {
        cerr << "FAIL: Mean relative error exceeds tolerance" << endl;
        pass = false;
    }
    if (std_error > tolerance) {
        cerr << "FAIL: StdDev relative error exceeds tolerance" << endl;
        pass = false;
    }
    
    if (pass) {
        cout << "✓ PASS: " << name << " validation successful" << endl;
    } else {
        cout << "✗ FAIL: " << name << " validation failed" << endl;
    }
    
    return pass;
}

// ============================================================================
// 主测试函数
// ============================================================================
int main() {
    cout << "========================================" << endl;
    cout << "TCC HLS Testbench - Phase 0 Skeleton" << endl;
    cout << "========================================" << endl;
    cout << endl;
    
    // ========================================================================
    // Phase 0: 加载Golden数据（验证文件读取）
    // ========================================================================
    cout << "[Phase 0] Loading Golden Data..." << endl;
    
    // 1. 加载image.bin（512×512配置）
    vector<data_t> golden_image;
    int Lx = 512, Ly = 512;
    int image_size = Lx * Ly;
    
    if (!load_binary_file("golden/image_expected_512x512.bin", golden_image, image_size)) {
        cout << "⚠ Warning: Using placeholder data (Phase 0 skeleton)" << endl;
        // 创建占位符数据
        golden_image.resize(image_size, 0.015f);  // 使用参考均值
    } else {
        cout << "✓ Loaded image.bin: " << image_size << " elements" << endl;
    }
    
    // 2. 加载统计信息
    ifstream stats_file("golden/golden_stats.txt");
    if (stats_file.is_open()) {
        cout << "✓ Loaded golden_stats.txt" << endl;
        stats_file.close();
    } else {
        cout << "⚠ Warning: golden_stats.txt not found" << endl;
    }
    
    // ========================================================================
    // 输出统计信息（用于HLS验证对比）
    // ========================================================================
    cout << endl;
    cout << "=== Golden Statistics ===" << endl;
    data_t img_mean = compute_mean(golden_image);
    data_t img_std = compute_stddev(golden_image, img_mean);
    data_t img_max = compute_max(golden_image);
    data_t img_min = compute_min(golden_image);
    
    cout << "Image Mean:   " << img_mean << endl;
    cout << "Image StdDev: " << img_std << endl;
    cout << "Image Max:    " << img_max << endl;
    cout << "Image Min:    " << img_min << endl;
    cout << endl;
    
    // ========================================================================
    // Phase 1-4: HLS函数调用（占位符）
    // ========================================================================
    cout << "[Phase 1-4] HLS Function Call Placeholder..." << endl;
    cout << "⚠ Note: tcc_top() will be implemented in Phase 1-4" << endl;
    cout << endl;
    
    // 占位符：创建模拟输出（Phase 0仅验证框架）
    vector<data_t> hls_image = golden_image;  // 假设完美匹配
    
    // ========================================================================
    // 验证（Phase 0仅验证框架，Phase 1-4验证实际HLS输出）
    // ========================================================================
    bool all_pass = verify_results(hls_image, golden_image, "Aerial Image");
    
    cout << endl;
    cout << "========================================" << endl;
    cout << "Test Summary" << endl;
    cout << "========================================" << endl;
    
    if (all_pass) {
        cout << "✓ Phase 0 Skeleton Test PASS" << endl;
        cout << "Ready for Phase 1 implementation" << endl;
        return 0;
    } else {
        cout << "✗ Phase 0 Skeleton Test FAIL" << endl;
        cout << "Check golden data and testbench setup" << endl;
        return 1;
    }
}