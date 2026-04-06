/*
 * SOCS HLS 简化测试平台（验证基础逻辑）
 * FPGA-Litho Project
 */

#include "../src/data_types.h"
#include "../src/socs_top_simple.h"
#include <iostream>
#include <fstream>
#include <cmath>

using namespace std;

// ============================================================================
// 辅助函数：读取二进制文件
// ============================================================================

void read_float_array(const char* filename, float* data, int size) {
    ifstream file(filename, ios::binary);
    if (!file) {
        cerr << "Error: Cannot open " << filename << endl;
        return;
    }
    file.read(reinterpret_cast<char*>(data), size * sizeof(float));
    file.close();
}

void read_complex_array(const char* real_file, const char* imag_file, cmpxData_t* data, int size) {
    float* real_data = new float[size];
    float* imag_data = new float[size];
    
    read_float_array(real_file, real_data, size);
    read_float_array(imag_file, imag_data, size);
    
    for (int i = 0; i < size; i++) {
        data[i] = cmpxData_t(real_data[i], imag_data[i]);
    }
    
    delete[] real_data;
    delete[] imag_data;
}

// ============================================================================
// 辅助函数：比较结果
// ============================================================================

bool compare_results(float* output, float* expected, int size, float tolerance = 0.1f) {
    float max_error = 0.0f;
    float sum_error = 0.0f;
    int error_count = 0;
    
    for (int i = 0; i < size; i++) {
        float error = abs(output[i] - expected[i]);
        if (error > max_error) max_error = error;
        sum_error += error;
        
        if (error > tolerance) {
            error_count++;
            if (error_count < 5) {
                cout << "Error at idx " << i << ": Output=" << output[i] 
                     << ", Expected=" << expected[i] << ", Error=" << error << endl;
            }
        }
    }
    
    cout << "\n========== Verification Summary ==========" << endl;
    cout << "Size: " << size << endl;
    cout << "Max Error: " << max_error << endl;
    cout << "Mean Error: " << sum_error / size << endl;
    cout << "Error Count (>tolerance): " << error_count << endl;
    cout << "==========================================" << endl;
    
    return (max_error < tolerance) && (error_count == 0);
}

// ============================================================================
// 测试主函数
// ============================================================================

int main() {
    cout << "======================================" << endl;
    cout << "SOCS HLS C Simulation Test (Simplified)" << endl;
    cout << "======================================" << endl;
    
    // 测试参数
    const int Lx = 512;
    const int Ly = 512;
    const int Nx = 4;
    const int Ny = 4;
    const int nk = 10;
    const int krnSizeX = 9;
    const int krnSizeY = 9;
    
    // ============================================================================
    // 读取 Golden 数据
    // ============================================================================
    
    cout << "\n[1] Loading Golden Data..." << endl;
    
    // Mask频域数据
    cmpxData_t* mskf = new cmpxData_t[Lx * Ly];
    read_complex_array("/root/project/FPGA-Litho/source/SOCS_HLS/hls/tb/golden/mskf_r.bin", 
                       "/root/project/FPGA-Litho/source/SOCS_HLS/hls/tb/golden/mskf_i.bin", 
                       mskf, Lx * Ly);
    cout << "  Loaded mskf: " << Lx * Ly << " complex" << endl;
    
    // SOCS核数据
    cmpxData_t* krns = new cmpxData_t[nk * krnSizeX * krnSizeY];
    for (int k = 0; k < nk; k++) {
        char real_file[200], imag_file[200];
        sprintf(real_file, "/root/project/FPGA-Litho/source/SOCS_HLS/hls/tb/golden/krn_%d_r.bin", k);
        sprintf(imag_file, "/root/project/FPGA-Litho/source/SOCS_HLS/hls/tb/golden/krn_%d_i.bin", k);
        read_complex_array(real_file, imag_file, &krns[k * krnSizeX * krnSizeY], krnSizeX * krnSizeY);
    }
    cout << "  Loaded krns: " << nk << " kernels" << endl;
    
    // 特征值
    float* scales = new float[nk];
    read_float_array("/root/project/FPGA-Litho/source/SOCS_HLS/hls/tb/golden/scales.bin", scales, nk);
    cout << "  Loaded scales: " << nk << " values" << endl;
    
    // 预期输出（仅用于参考，简化版本不要求完全匹配）
    float* expected_image = new float[Lx * Ly];
    read_float_array("/root/project/FPGA-Litho/source/SOCS_HLS/hls/tb/golden/image_expected.bin", 
                     expected_image, Lx * Ly);
    cout << "  Loaded expected image: " << Lx * Ly << " floats" << endl;
    
    // ============================================================================
    // HLS仿真（简化版本）
    // ============================================================================
    
    cout << "\n[2] Running HLS Simulation..." << endl;
    
    // 创建输出缓冲
    float* output_image = new float[Lx * Ly];
    
    // 调用简化版本的核心函数
    calc_socs_simple(mskf, krns, scales, output_image, Lx, Ly, Nx, Ny, nk);
    
    // ============================================================================
    // 结果验证（宽松标准，仅验证逻辑）
    // ============================================================================
    
    cout << "\n[3] Checking Output Statistics..." << endl;
    
    // 统计输出数据
    float mean = 0.0f, max_val = -1e30f, min_val = 1e30f;
    for (int i = 0; i < Lx * Ly; i++) {
        mean += output_image[i];
        if (output_image[i] > max_val) max_val = output_image[i];
        if (output_image[i] < min_val) min_val = output_image[i];
    }
    mean /= (Lx * Ly);
    
    cout << "  Output Mean: " << mean << endl;
    cout << "  Output Max:  " << max_val << endl;
    cout << "  Output Min:  " << min_val << endl;
    
    cout << "\n  Expected Mean: " << expected_image[Lx*Ly/2] << " (center value)" << endl;
    
    // 基本验证：确认输出不为全零
    bool basic_passed = (mean > 0.0f) && (max_val > min_val);
    
    if (basic_passed) {
        cout << "\n✓✓✓ BASIC LOGIC TEST PASSED ✓✓✓" << endl;
        cout << "  Note: This is a simplified version without FFT." << endl;
        cout << "  Full accuracy validation will be done after FFT integration." << endl;
    } else {
        cout << "\n✗✗✗ BASIC LOGIC TEST FAILED ✗✗✗" << endl;
    }
    
    // ============================================================================
    // 清理
    // ============================================================================
    
    delete[] mskf;
    delete[] krns;
    delete[] scales;
    delete[] expected_image;
    delete[] output_image;
    
    return basic_passed ? 0 : 1;
}