/*
 * SOCS完整流程测试平台
 * SOCS HLS Project
 * 
 * 测试目标：
 *   1. 验证真实2D FFT集成
 *   2. 验证完整calcSOCS流程
 *   3. 对比golden数据
 */

#include "../src/socs_top_full.cpp"
#include <iostream>
#include <fstream>
#include <cmath>
#include <vector>

using namespace std;

// ============================================================================
// 数据加载函数
// ============================================================================

bool load_binary_data(const string& filename, vector<data_t>& data, int expected_size) {
    ifstream file(filename, ios::binary);
    if (!file) {
        cerr << "ERROR: Cannot open " << filename << endl;
        return false;
    }
    
    data.resize(expected_size);
    file.read(reinterpret_cast<char*>(data.data()), expected_size * sizeof(data_t));
    
    if (file.gcount() != expected_size * sizeof(data_t)) {
        cerr << "ERROR: File size mismatch: " << filename << endl;
        return false;
    }
    
    return true;
}

bool load_complex_data(const string& re_file, const string& im_file, 
                       vector<cmpxData_t>& data, int expected_size) {
    vector<data_t> re_data, im_data;
    
    if (!load_binary_data(re_file, re_data, expected_size) ||
        !load_binary_data(im_file, im_data, expected_size)) {
        return false;
    }
    
    data.resize(expected_size);
    for (int i = 0; i < expected_size; i++) {
        data[i] = cmpxData_t(re_data[i], im_data[i]);
    }
    
    return true;
}

// ============================================================================
// 主测试函数
// ============================================================================

int main() {
    cout << "\n========================================" << endl;
    cout << "SOCS Full Flow Test" << endl;
    cout << "========================================\n" << endl;
    
    // 测试参数
    const int Lx = 512;
    const int Ly = 512;
    const int Nx = 4;
    const int Ny = 4;
    const int nk = 10;
    
    const int krnSizeX = 2 * Nx + 1;  // 9
    const int krnSizeY = 2 * Ny + 1;  // 9
    const int sizeX = 4 * Nx + 1;     // 17
    const int sizeY = 4 * Ny + 1;     // 17
    
    // 数据目录
    const string DATA_DIR = "/root/project/FPGA-Litho/output/verification/";
    
    // 加载输入数据
    vector<cmpxData_t> mskf;
    vector<cmpxData_t> krns;
    vector<data_t> scales;
    vector<data_t> expected_image;
    
    cout << "[1] Loading input data..." << endl;
    
    // Mask频域
    if (!load_complex_data(DATA_DIR + "mskf_r.bin", DATA_DIR + "mskf_i.bin", 
                           mskf, Lx * Ly)) {
        cout << "  ✗ Failed to load mskf" << endl;
        return 1;
    }
    cout << "  ✓ mskf loaded: " << mskf.size() << " complex elements" << endl;
    
    // SOCS核（10个9×9核）
    // 注意：实际文件结构是 kernels/krn_0_r.bin, krn_0_i.bin 等
    for (int k = 0; k < nk; k++) {
        string re_file = DATA_DIR + "kernels/krn_" + to_string(k) + "_r.bin";
        string im_file = DATA_DIR + "kernels/krn_" + to_string(k) + "_i.bin";
        
        vector<data_t> re_data, im_data;
        if (!load_binary_data(re_file, re_data, krnSizeX * krnSizeY) ||
            !load_binary_data(im_file, im_data, krnSizeX * krnSizeY)) {
            cout << "  ✗ Failed to load kernel " << k << endl;
            return 1;
        }
        
        for (int i = 0; i < krnSizeX * krnSizeY; i++) {
            krns.push_back(cmpxData_t(re_data[i], im_data[i]));
        }
    }
    cout << "  ✓ krns loaded: " << krns.size() << " complex elements" << endl;
    
    // 特征值
    if (!load_binary_data(DATA_DIR + "scales.bin", scales, nk)) {
        cout << "  ✗ Failed to load scales" << endl;
        return 1;
    }
    cout << "  ✓ scales loaded: " << scales.size() << " elements" << endl;
    
    // 预期输出
    if (!load_binary_data(DATA_DIR + "image.bin", expected_image, Lx * Ly)) {
        cout << "  ✗ Failed to load expected image" << endl;
        return 1;
    }
    cout << "  ✓ expected image loaded: " << expected_image.size() << " elements" << endl;
    
    // ============================================================================
    // 执行SOCS计算
    // ============================================================================
    
    cout << "\n[2] Running SOCS Full Flow..." << endl;
    
    vector<data_t> output_image(Lx * Ly);
    
    // 转换为数组格式（HLS要求）
    cmpxData_t mskf_arr[Lx * Ly];
    cmpxData_t krns_arr[nk * krnSizeX * krnSizeY];
    data_t scales_arr[nk];
    data_t image_arr[Lx * Ly];
    
    for (int i = 0; i < Lx * Ly; i++) mskf_arr[i] = mskf[i];
    for (int i = 0; i < krns.size(); i++) krns_arr[i] = krns[i];
    for (int i = 0; i < nk; i++) scales_arr[i] = scales[i];
    
    // 调用核心计算函数
    calc_socs_full(mskf_arr, krns_arr, scales_arr, image_arr, Lx, Ly, Nx, Ny, nk);
    
    for (int i = 0; i < Lx * Ly; i++) output_image[i] = image_arr[i];
    
    cout << "  ✓ SOCS calculation completed" << endl;
    
    // ============================================================================
    // 结果验证
    // ============================================================================
    
    cout << "\n[3] Verification..." << endl;
    
    // 统计分析
    data_t max_err = 0.0;
    data_t mean_err = 0.0;
    data_t max_rel_err = 0.0;
    
    data_t out_mean = 0.0;
    data_t out_max = 0.0;
    data_t out_min = 1e30;
    
    for (int i = 0; i < Lx * Ly; i++) {
        data_t err = abs(output_image[i] - expected_image[i]);
        mean_err += err;
        
        if (err > max_err) max_err = err;
        
        if (expected_image[i] != 0.0) {
            data_t rel_err = err / abs(expected_image[i]);
            if (rel_err > max_rel_err) max_rel_err = rel_err;
        }
        
        out_mean += output_image[i];
        if (output_image[i] > out_max) out_max = output_image[i];
        if (output_image[i] < out_min) out_min = output_image[i];
    }
    
    mean_err /= (Lx * Ly);
    out_mean /= (Lx * Ly);
    
    cout << "\n  HLS Output Statistics:" << endl;
    cout << "    Mean:      " << out_mean << endl;
    cout << "    Max:       " << out_max << endl;
    cout << "    Min:       " << out_min << endl;
    
    cout << "\n  Error Analysis:" << endl;
    cout << "    Max Absolute Error:  " << max_err << endl;
    cout << "    Mean Absolute Error: " << mean_err << endl;
    cout << "    Max Relative Error:  " << max_rel_err << endl;
    
    // 验收标准
    const data_t TOLERANCE = 1e-4;  // 简化版本的容忍度
    
    bool passed = true;
    if (max_err > TOLERANCE) {
        cout << "\n  ✗ ERROR: Max error exceeds tolerance (" << TOLERANCE << ")" << endl;
        passed = false;
    }
    
    if (passed) {
        cout << "\n  ✓✓✓ SOCS FULL FLOW TEST PASSED ✓✓✓" << endl;
    } else {
        cout << "\n  ✗✗✗ SOCS FULL FLOW TEST FAILED ✗✗✗" << endl;
    }
    
    // 输出关键值对比
    cout << "\n  Sample Points Comparison (center region):" << endl;
    int center_y = Ly / 2;
    int center_x = Lx / 2;
    
    for (int dy = -2; dy <= 2; dy++) {
        for (int dx = -2; dx <= 2; dx++) {
            int y = center_y + dy;
            int x = center_x + dx;
            int idx = y * Lx + x;
            
            cout << "    [" << y << "," << x << "] HLS: " 
                 << output_image[idx] << "  Expected: " 
                 << expected_image[idx] << endl;
        }
        cout << endl;
    }
    
    return passed ? 0 : 1;
}