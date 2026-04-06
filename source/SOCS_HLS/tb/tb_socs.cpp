/*
 * SOCS HLS 测试平台
 * FPGA-Litho Project
 */

#include "../src/data_types.h"
#include "../src/socs_top.cpp"
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

bool compare_results(float* hls_output, float* golden, int size, float tolerance = 1e-5f) {
    float max_error = 0.0f;
    float sum_error = 0.0f;
    int error_count = 0;
    
    for (int i = 0; i < size; i++) {
        float error = abs(hls_output[i] - golden[i]);
        if (error > max_error) max_error = error;
        sum_error += error;
        
        if (error > tolerance) {
            error_count++;
            if (error_count < 10) {
                cout << "Error at idx " << i << ": HLS=" << hls_output[i] 
                     << ", Golden=" << golden[i] << ", Error=" << error << endl;
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
    cout << "SOCS HLS C Simulation Test" << endl;
    cout << "======================================" << endl;
    
    // 测试参数（从 verification output）
    const int Lx = 512;
    const int Ly = 512;
    const int Nx = 4;
    const int Ny = 4;
    const int nk = 10;
    const int krnSizeX = 9;
    const int krnSizeY = 9;
    
    int sizeX = 4 * Nx + 1;  // 17
    int sizeY = 4 * Ny + 1;  // 17
    
    // ============================================================================
    // 读取 Golden 数据
    // ============================================================================
    
    cout << "\n[1] Loading Golden Data..." << endl;
    
    // Mask频域数据
    cmpxData_t* mskf = new cmpxData_t[Lx * Ly];
    read_complex_array("hls/tb/golden/mskf_r.bin", "hls/tb/golden/mskf_i.bin", mskf, Lx * Ly);
    cout << "  Loaded mskf: " << Lx * Ly << " complex" << endl;
    
    // SOCS核数据
    cmpxData_t* krns = new cmpxData_t[nk * krnSizeX * krnSizeY];
    for (int k = 0; k < nk; k++) {
        char real_file[100], imag_file[100];
        sprintf(real_file, "hls/tb/golden/krn_%d_r.bin", k);
        sprintf(imag_file, "hls/tb/golden/krn_%d_i.bin", k);
        read_complex_array(real_file, imag_file, &krns[k * krnSizeX * krnSizeY], krnSizeX * krnSizeY);
    }
    cout << "  Loaded krns: " << nk << " kernels, each " << krnSizeX << "x" << krnSizeY << endl;
    
    // 特征值
    float* scales = new float[nk];
    read_float_array("hls/tb/golden/scales.bin", scales, nk);
    cout << "  Loaded scales: " << nk << " values" << endl;
    
    // 预期输出
    float* golden_image = new float[Lx * Ly];
    read_float_array("hls/tb/golden/image_expected.bin", golden_image, Lx * Ly);
    cout << "  Loaded golden image: " << Lx * Ly << " floats" << endl;
    
    // ============================================================================
    // HLS仿真
    // ============================================================================
    
    cout << "\n[2] Running HLS Simulation..." << endl;
    
    // 创建控制参数
    ap_uint<32> ctrl_reg[6];
    ctrl_reg[0] = Lx;
    ctrl_reg[1] = Ly;
    ctrl_reg[2] = Nx;
    ctrl_reg[3] = Ny;
    ctrl_reg[4] = nk;
    ctrl_reg[5] = 1;  // ap_start
    
    // 创建输入缓冲（模拟AXI-MM）
    cmpxData_t* m_axi_mskf = mskf;
    cmpxData_t* m_axi_krns = krns;
    float* m_axi_scales = scales;
    
    // 创建输出流（模拟AXI-Stream）
    hls::stream<float> m_axis_image;
    
    // 调用HLS顶层函数
    socs_top(ctrl_reg, m_axi_mskf, m_axi_krns, m_axi_scales, m_axis_image);
    
    // 从流读取结果
    float* hls_output = new float[Lx * Ly];
    for (int i = 0; i < Lx * Ly; i++) {
        hls_output[i] = m_axis_image.read();
    }
    
    // ============================================================================
    // 结果验证
    // ============================================================================
    
    cout << "\n[3] Verifying Results..." << endl;
    
    bool passed = compare_results(hls_output, golden_image, Lx * Ly, 1e-5f);
    
    if (passed) {
        cout << "\n✓✓✓ TEST PASSED ✓✓✓" << endl;
    } else {
        cout << "\n✗✗✗ TEST FAILED ✗✗✗" << endl;
    }
    
    // ============================================================================
    // 清理
    // ============================================================================
    
    delete[] mskf;
    delete[] krns;
    delete[] scales;
    delete[] golden_image;
    delete[] hls_output;
    
    return passed ? 0 : 1;
}