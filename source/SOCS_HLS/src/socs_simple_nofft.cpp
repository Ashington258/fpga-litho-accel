/*
 * SOCS核心计算函数（CoSim专用简化版本）
 * FPGA-Litho Project
 *
 * 特点：不使用任何HLS FFT库，仅做频域点乘和插值
 * 目标：通过CoSim验证基本流程
 */

#include <complex>

typedef float data_t;
typedef std::complex<data_t> cmpxData_t;

// ============================================================================
// fftshift实现
// ============================================================================

void shift_2d(
    data_t* input,
    data_t* output,
    int sizeX,
    int sizeY
) {
#pragma HLS INLINE
    
    int xh = (sizeX + 1) / 2;
    int yh = (sizeY + 1) / 2;
    
    for (int y = 0; y < sizeY; y++) {
        for (int x = 0; x < sizeX; x++) {
#pragma HLS PIPELINE
            int sy = (y + yh) % sizeY;
            int sx = (x + xh) % sizeX;
            output[sy * sizeX + sx] = input[y * sizeX + x];
        }
    }
}

// ============================================================================
// 简化插值（最近邻）
// ============================================================================

void interp_simple(
    data_t* input,
    data_t* output,
    int in_sizeX,
    int in_sizeY,
    int out_sizeX,
    int out_sizeY
) {
    for (int y = 0; y < out_sizeY; y++) {
        for (int x = 0; x < out_sizeX; x++) {
#pragma HLS PIPELINE
            int src_x = (x * in_sizeX) / out_sizeX;
            int src_y = (y * in_sizeY) / out_sizeY;
            
            if (src_x >= in_sizeX) src_x = in_sizeX - 1;
            if (src_y >= in_sizeY) src_y = in_sizeY - 1;
            
            output[y * out_sizeX + x] = input[src_y * in_sizeX + src_x];
        }
    }
}

// ============================================================================
// SOCS核心算法（无FFT简化版本）
// ============================================================================

void calc_socs_simple_hls(
    // 输入（固定尺寸数组）
    cmpxData_t mskf[512*512],       // Mask频域
    cmpxData_t krns[10*9*9],        // 10个9×9核
    data_t scales[10],              // 10个特征值
    
    // 输出
    data_t image[512*512]           // 空中像
) {
#pragma HLS DATAFLOW
#pragma HLS INTERFACE m_axi port=mskf depth=512*512
#pragma HLS INTERFACE m_axi port=krns depth=10*9*9
#pragma HLS INTERFACE m_axi port=scales depth=10
#pragma HLS INTERFACE m_axi port=image depth=512*512
#pragma HLS INTERFACE ap_ctrl_hs port=return
    
    // 固定参数
    const int Lx = 512;
    const int Ly = 512;
    const int Nx = 4;
    const int Ny = 4;
    const int nk = 10;
    
    const int krnSizeX = 9;
    const int krnSizeY = 9;
    const int sizeX = 17;
    const int sizeY = 17;
    const int Lxh = 256;
    const int Lyh = 256;
    
    // 临时缓冲
    cmpxData_t tmp_conv[17*17];
    data_t tmp_image[17*17];
    data_t tmp_shifted[17*17];
    
#pragma HLS ARRAY_PARTITION variable=tmp_conv cyclic factor=4 dim=1
#pragma HLS ARRAY_PARTITION variable=tmp_image cyclic factor=4 dim=1
    
    // 初始化
    for (int i = 0; i < 17*17; i++) {
#pragma HLS PIPELINE
        tmp_image[i] = 0.0;
        tmp_conv[i] = cmpxData_t(0.0, 0.0);
    }
    
    // Step 1: 循环10个核 - 频域点乘
    for (int k = 0; k < nk; k++) {
#pragma HLS UNROLL factor=2
        
        // 核与mask频域点乘
        for (int y = -Ny; y <= Ny; y++) {
            for (int x = -Nx; x <= Nx; x++) {
#pragma HLS PIPELINE
                
                int msk_idx = (y + Lyh) * Lx + (x + Lxh);
                int krn_idx = k * 81 + (y + Ny) * 9 + (x + Nx);
                int conv_idx = (y + Ny + 4) * 17 + (x + Nx + 4);
                
                tmp_conv[conv_idx] = mskf[msk_idx] * krns[krn_idx];
            }
        }
        
        // 计算模平方并加权累加（代替IFFT）
        for (int i = 0; i < 17*17; i++) {
#pragma HLS PIPELINE
            data_t re = tmp_conv[i].real();
            data_t im = tmp_conv[i].imag();
            data_t mag_sq = re * re + im * im;
            
            tmp_image[i] += scales[k] * mag_sq;
        }
        
        // 清零卷积缓冲
        for (int i = 0; i < 17*17; i++) {
#pragma HLS PIPELINE
            tmp_conv[i] = cmpxData_t(0.0, 0.0);
        }
    }
    
    // Step 2: fftshift
    shift_2d(tmp_image, tmp_shifted, sizeX, sizeY);
    
    // Step 3: 简化插值（17×17 → 512×512）
    interp_simple(tmp_shifted, image, sizeX, sizeY, Lx, Ly);
}