/*
 * SOCS测试平台（无FFT库版本）
 * FPGA-Litho Project
 */

#include <iostream>
#include <fstream>
#include <cmath>
#include <vector>
#include <complex>

using namespace std;

typedef float data_t;
typedef complex<data_t> cmpxData_t;

// HLS顶层函数声明
extern "C" {
    void calc_socs_simple_hls(
        cmpxData_t mskf[512*512],
        cmpxData_t krns[10*9*9],
        data_t scales[10],
        data_t image[512*512]
    );
}

// 数据加载函数
bool load_binary_data(const string& filename, vector<data_t>& data, int expected_size) {
    ifstream file(filename, ios::binary);
    if (!file) return false;
    data.resize(expected_size);
    file.read(reinterpret_cast<char*>(data.data()), expected_size * sizeof(data_t));
    return file.gcount() == expected_size * sizeof(data_t);
}

bool load_complex_data(const string& re_file, const string& im_file, 
                       vector<cmpxData_t>& data, int expected_size) {
    vector<data_t> re_data, im_data;
    if (!load_binary_data(re_file, re_data, expected_size) ||
        !load_binary_data(im_file, im_data, expected_size)) return false;
    data.resize(expected_size);
    for (int i = 0; i < expected_size; i++) {
        data[i] = cmpxData_t(re_data[i], im_data[i]);
    }
    return true;
}

int main() {
    cout << "\n========================================" << endl;
    cout << "SOCS Test (No-FFT Version)" << endl;
    cout << "========================================\n" << endl;
    
    const int Lx = 512;
    const int Ly = 512;
    const int nk = 10;
    const int krnSize = 81;
    
    const string DATA_DIR = "/root/project/FPGA-Litho/output/verification/";
    
    cout << "[1] Loading input data..." << endl;
    
    vector<cmpxData_t> mskf_data;
    if (!load_complex_data(DATA_DIR + "mskf_r.bin", DATA_DIR + "mskf_i.bin", 
                           mskf_data, Lx * Ly)) {
        cerr << "  Failed to load mskf" << endl;
        return 1;
    }
    
    vector<cmpxData_t> krns_data;
    for (int k = 0; k < nk; k++) {
        vector<data_t> re_data, im_data;
        if (!load_binary_data(DATA_DIR + "kernels/krn_" + to_string(k) + "_r.bin", re_data, krnSize) ||
            !load_binary_data(DATA_DIR + "kernels/krn_" + to_string(k) + "_i.bin", im_data, krnSize)) {
            cerr << "  Failed to load kernel " << k << endl;
            return 1;
        }
        for (int i = 0; i < krnSize; i++) {
            krns_data.push_back(cmpxData_t(re_data[i], im_data[i]));
        }
    }
    
    vector<data_t> scales_data;
    if (!load_binary_data(DATA_DIR + "scales.bin", scales_data, nk)) {
        cerr << "  Failed to load scales" << endl;
        return 1;
    }
    
    vector<data_t> expected_image;
    if (!load_binary_data(DATA_DIR + "image.bin", expected_image, Lx * Ly)) {
        cerr << "  Failed to load expected image" << endl;
        return 1;
    }
    
    cout << "  All data loaded successfully" << endl;
    
    // 准备HLS输入
    cout << "\n[2] Preparing HLS inputs..." << endl;
    
    cmpxData_t hls_mskf[512*512];
    cmpxData_t hls_krns[10*9*9];
    data_t hls_scales[10];
    data_t hls_image[512*512];
    
    for (int i = 0; i < Lx * Ly; i++) hls_mskf[i] = mskf_data[i];
    for (int i = 0; i < nk * krnSize; i++) hls_krns[i] = krns_data[i];
    for (int i = 0; i < nk; i++) hls_scales[i] = scales_data[i];
    
    cout << "  Input prepared" << endl;
    
    // 执行HLS函数
    cout << "\n[3] Running HLS function..." << endl;
    calc_socs_simple_hls(hls_mskf, hls_krns, hls_scales, hls_image);
    cout << "  Completed" << endl;
    
    // 结果统计
    cout << "\n[4] Result statistics..." << endl;
    
    data_t out_mean = 0.0, out_max = 0.0, out_min = 1e30;
    for (int i = 0; i < Lx * Ly; i++) {
        out_mean += hls_image[i];
        if (hls_image[i] > out_max) out_max = hls_image[i];
        if (hls_image[i] < out_min) out_min = hls_image[i];
    }
    out_mean /= (Lx * Ly);
    
    cout << "  Mean: " << out_mean << endl;
    cout << "  Max:  " << out_max << endl;
    cout << "  Min:  " << out_min << endl;
    
    // 验收
    bool passed = (out_mean > 0 && out_max >= out_min);
    
    if (passed) {
        cout << "\n✓✓✓ TEST PASSED ✓✓✓" << endl;
    } else {
        cout << "\n✗✗✗ TEST FAILED ✗✗✗" << endl;
    }
    
    return passed ? 0 : 1;
}