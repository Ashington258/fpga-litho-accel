/**
 * @file tb_calc_tcc.cpp
 * @brief calcTCC模块测试平台 - Phase 1验证
 * 
 * 测试目标：
 * - 验证calc_tcc函数与CPU参考的一致性
 * - 统计验证：相对误差 < 1e-5
 * - 支持小尺寸配置（Nx=Ny=4, tccSize=81）
 * 
 * 测试数据：
 * - 光源：source_annular.bin（Annular光源，outerRadius=0.9）
 * - TCC期望：tcc_expected_r.bin, tcc_expected_i.bin（81×81）
 * 
 * 参数配置（参考config.json）：
 * - NA=0.8, lambda=193nm, defocus=0.2
 * - Lx=Ly=512, Nx=Ny=4, srcSize=101
 * - outerSigma=0.9
 */

#include <iostream>
#include <fstream>
#include <cmath>
#include <vector>
#include <string>
#include <complex>
#include "../src/data_types.h"
#include "../src/calc_tcc.h"

using namespace std;

// ============================================================================
// 测试配置参数（参考config.json）
// ============================================================================
const int LX = 512;
const int LY = 512;
const int SRC_SIZE = 101;
const data_t NA = 0.8;
const data_t LAMBDA = 193.0;
const data_t DEFOCUS = 0.2;
const data_t OUTER_SIGMA = 0.9;  // Annular光源outerRadius（从config.json）
const int NX = 4;  // floor(NA * Lx * (1+outerSigma) / lambda)
const int NY = 4;
const int TCC_SIZE = (2 * NX + 1) * (2 * NY + 1);  // 81

// ============================================================================
// 文件IO辅助函数
// ============================================================================
bool load_binary_file(const string& filename, vector<data_t>& buffer, int expected_size) {
    ifstream file(filename, ios::binary);
    if (!file.is_open()) {
        cerr << "ERROR: Cannot open file: " << filename << endl;
        return false;
    }
    
    file.seekg(0, ios::end);
    int file_size = file.tellg();
    file.seekg(0, ios::beg);
    
    int num_elements = file_size / sizeof(data_t);
    if (num_elements != expected_size) {
        cerr << "ERROR: File size mismatch in " << filename << endl;
        cerr << "  Expected " << expected_size << " elements, got " << num_elements << endl;
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

bool save_complex_binary(const string& filename_real, const string& filename_imag,
                         vector<cmpxData_t>& buffer, int size) {
    vector<data_t> real_part(size), imag_part(size);
    
    for (int i = 0; i < size; i++) {
        real_part[i] = buffer[i].real();
        imag_part[i] = buffer[i].imag();
    }
    
    ofstream file_r(filename_real, ios::binary);
    ofstream file_i(filename_imag, ios::binary);
    
    if (!file_r.is_open() || !file_i.is_open()) {
        cerr << "ERROR: Cannot open output files" << endl;
        return false;
    }
    
    file_r.write(reinterpret_cast<char*>(real_part.data()), size * sizeof(data_t));
    file_i.write(reinterpret_cast<char*>(imag_part.data()), size * sizeof(data_t));
    
    file_r.close();
    file_i.close();
    
    return true;
}

// ============================================================================
// 统计计算函数
// ============================================================================
data_t compute_mean_complex(const vector<cmpxData_t>& data, bool real_part) {
    data_t sum = 0.0;
    for (const auto& val : data) {
        sum += real_part ? val.real() : val.imag();
    }
    return sum / data.size();
}

data_t compute_stddev_complex(const vector<cmpxData_t>& data, data_t mean, bool real_part) {
    data_t sum_sq = 0.0;
    for (const auto& val : data) {
        data_t diff = (real_part ? val.real() : val.imag()) - mean;
        sum_sq += diff * diff;
    }
    return sqrt(sum_sq / data.size());
}

// ============================================================================
// TCC验证函数
// ============================================================================
bool verify_tcc(const vector<cmpxData_t>& hls_tcc, const vector<cmpxData_t>& golden_tcc,
                const string& name, data_t tolerance = 1e-5) {
    cout << "=== Verifying " << name << " ===" << endl;
    
    // 计算统计信息（实部）
    data_t hls_mean_r = compute_mean_complex(hls_tcc, true);
    data_t golden_mean_r = compute_mean_complex(golden_tcc, true);
    data_t hls_std_r = compute_stddev_complex(hls_tcc, hls_mean_r, true);
    data_t golden_std_r = compute_stddev_complex(golden_tcc, golden_mean_r, true);
    
    cout << "Real Part Statistics:" << endl;
    cout << "  HLS Mean:   " << hls_mean_r << endl;
    cout << "  Golden Mean: " << golden_mean_r << endl;
    cout << "  Relative Error (Mean): " << abs((hls_mean_r - golden_mean_r) / golden_mean_r) << endl;
    cout << endl;
    cout << "  HLS StdDev:   " << hls_std_r << endl;
    cout << "  Golden StdDev: " << golden_std_r << endl;
    cout << "  Relative Error (StdDev): " << abs((hls_std_r - golden_std_r) / golden_std_r) << endl;
    cout << endl;
    
    // 计算统计信息（虚部）
    data_t hls_mean_i = compute_mean_complex(hls_tcc, false);
    data_t golden_mean_i = compute_mean_complex(golden_tcc, false);
    data_t hls_std_i = compute_stddev_complex(hls_tcc, hls_mean_i, false);
    data_t golden_std_i = compute_stddev_complex(golden_tcc, golden_mean_i, false);
    
    cout << "Imaginary Part Statistics:" << endl;
    cout << "  HLS Mean:   " << hls_mean_i << endl;
    cout << "  Golden Mean: " << golden_mean_i << endl;
    cout << "  Relative Error (Mean): " << abs((hls_mean_i - golden_mean_i) / (golden_mean_i + 1e-10)) << endl;
    cout << endl;
    cout << "  HLS StdDev:   " << hls_std_i << endl;
    cout << "  Golden StdDev: " << golden_std_i << endl;
    cout << "  Relative Error (StdDev): " << abs((hls_std_i - golden_std_i) / (golden_std_i + 1e-10)) << endl;
    cout << endl;
    
    // 检查相对误差
    bool pass = true;
    data_t mean_error_r = abs((hls_mean_r - golden_mean_r) / golden_mean_r);
    data_t std_error_r = abs((hls_std_r - golden_std_r) / golden_std_r);
    
    if (mean_error_r > tolerance) {
        cerr << "FAIL: Real part mean error exceeds tolerance (" << mean_error_r << " > " << tolerance << ")" << endl;
        pass = false;
    }
    if (std_error_r > tolerance) {
        cerr << "FAIL: Real part StdDev error exceeds tolerance (" << std_error_r << " > " << tolerance << ")" << endl;
        pass = false;
    }
    
    // 虚部验证（如果golden虚部均值非零）
    if (abs(golden_mean_i) > 1e-10) {
        data_t mean_error_i = abs((hls_mean_i - golden_mean_i) / golden_mean_i);
        data_t std_error_i = abs((hls_std_i - golden_std_i) / golden_std_i);
        
        if (mean_error_i > tolerance) {
            cerr << "FAIL: Imag part mean error exceeds tolerance (" << mean_error_i << " > " << tolerance << ")" << endl;
            pass = false;
        }
        if (std_error_i > tolerance) {
            cerr << "FAIL: Imag part StdDev error exceeds tolerance (" << std_error_i << " > " << tolerance << ")" << endl;
            pass = false;
        }
    }
    
    // 输出样本对比
    cout << "Sample Comparison (diagonal elements):" << endl;
    for (int i = 0; i < 5 && i < TCC_SIZE; i++) {
        cout << "  HLS TCC[" << i << "," << i << "] = (" << hls_tcc[i * TCC_SIZE + i].real() 
             << ", " << hls_tcc[i * TCC_SIZE + i].imag() << ")" << endl;
        cout << "  Golden TCC[" << i << "," << i << "] = (" << golden_tcc[i * TCC_SIZE + i].real() 
             << ", " << golden_tcc[i * TCC_SIZE + i].imag() << ")" << endl;
    }
    cout << endl;
    
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
    cout << "calcTCC HLS Testbench - Phase 1" << endl;
    cout << "========================================" << endl;
    cout << endl;
    
    // ========================================================================
    // 配置信息输出
    // ========================================================================
    cout << "=== Configuration ===" << endl;
    cout << "Lx = " << LX << ", Ly = " << LY << endl;
    cout << "Nx = " << NX << ", Ny = " << NY << endl;
    cout << "TCC Size = " << TCC_SIZE << " x " << TCC_SIZE << " = " << TCC_SIZE * TCC_SIZE << endl;
    cout << "NA = " << NA << ", lambda = " << LAMBDA << " nm" << endl;
    cout << "defocus = " << DEFOCUS << ", outerSigma = " << OUTER_SIGMA << endl;
    cout << "srcSize = " << SRC_SIZE << endl;
    cout << endl;
    
    // ========================================================================
    // Step 1: 加载测试数据
    // ========================================================================
    cout << "[Step 1] Loading Test Data..." << endl;
    
    // 1.1 加载光源数据
    vector<data_t> src;
    if (!load_binary_file("golden/source_annular.bin", src, SRC_SIZE * SRC_SIZE)) {
        cerr << "ERROR: Failed to load source data" << endl;
        return 1;
    }
    cout << "✓ Loaded source data: " << src.size() << " elements" << endl;
    
    // 1.2 加载TCC期望值
    vector<cmpxData_t> golden_tcc;
    if (!load_complex_binary("golden/tcc_expected_r.bin", "golden/tcc_expected_i.bin",
                             golden_tcc, TCC_SIZE * TCC_SIZE)) {
        cerr << "ERROR: Failed to load golden TCC data" << endl;
        return 1;
    }
    cout << "✓ Loaded golden TCC: " << golden_tcc.size() << " elements" << endl;
    
    // ========================================================================
    // Step 2: 初始化光学参数
    // ========================================================================
    cout << endl << "[Step 2] Initializing Optical Parameters..." << endl;
    
    OpticalParams params;
    params.NA = NA;
    params.lambda = LAMBDA;
    params.defocus = DEFOCUS;
    params.Lx = LX;
    params.Ly = LY;
    params.outerSigma = OUTER_SIGMA;
    params.srcSize = SRC_SIZE;
    params.Nx = NX;
    params.Ny = NY;
    
    int sh = (SRC_SIZE - 1) / 2;  // 光源中心偏移 = 50
    int oSgm = ceil(sh * OUTER_SIGMA);  // 光源有效半径 = 45
    
    cout << "✓ Parameters initialized" << endl;
    cout << "  sh (center offset) = " << sh << endl;
    cout << "  oSgm (effective radius) = " << oSgm << endl;
    cout << endl;
    
    // ========================================================================
    // Step 3: 调用HLS calcTCC函数
    // ========================================================================
    cout << "[Step 3] Running HLS calcTCC..." << endl;
    
    // 分配TCC矩阵内存
    vector<cmpxData_t> hls_tcc(TCC_SIZE * TCC_SIZE, cmpxData_t(0.0, 0.0));
    
    // 调用calcTCC核心函数
    calc_tcc(src.data(), SRC_SIZE, hls_tcc.data(), params, TCC_SIZE, sh, oSgm);
    
    cout << "✓ calcTCC completed" << endl;
    cout << endl;
    
    // ========================================================================
    // Step 4: 验证结果
    // ========================================================================
    cout << "[Step 4] Verifying Results..." << endl;
    
    bool pass = verify_tcc(hls_tcc, golden_tcc, "TCC Matrix", 1e-5);
    
    // ========================================================================
    // Step 5: 保存HLS输出（用于后续分析）
    // ========================================================================
    cout << endl << "[Step 5] Saving HLS Output..." << endl;
    
    if (save_complex_binary("hls_tcc_r.bin", "hls_tcc_i.bin", hls_tcc, TCC_SIZE * TCC_SIZE)) {
        cout << "✓ Saved HLS TCC output to hls_tcc_r.bin, hls_tcc_i.bin" << endl;
    }
    
    // ========================================================================
    // 测试总结
    // ========================================================================
    cout << endl;
    cout << "========================================" << endl;
    cout << "Test Summary" << endl;
    cout << "========================================" << endl;
    
    if (pass) {
        cout << "✓ Phase 1 calcTCC Test PASS" << endl;
        cout << "Ready for HLS optimization (pragma tuning)" << endl;
        return 0;
    } else {
        cout << "✗ Phase 1 calcTCC Test FAIL" << endl;
        cout << "Check algorithm implementation and golden data" << endl;
        return 1;
    }
}