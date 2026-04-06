/*
 * SOCS HLS 顶层函数（简化版本，用于测试基础逻辑）
 * FPGA-Litho Project
 */

#include "data_types.h"
#include "hls_stream.h"

// ============================================================================
// 辅助函数：fftshift
// ============================================================================

void my_shift_2d(
    data_t* in,
    data_t* out,
    int sizeX,
    int sizeY,
    bool shiftTypeX,
    bool shiftTypeY
) {
#pragma HLS INLINE
    
    int sx, sy;
    int xh = shiftTypeX ? sizeX / 2 : (sizeX + 1) / 2;
    int yh = shiftTypeY ? sizeY / 2 : (sizeY + 1) / 2;
    
    for (int y = 0; y < sizeY; y++) {
        for (int x = 0; x < sizeX; x++) {
#pragma HLS PIPELINE
            sy = (y + yh) % sizeY;
            sx = (x + xh) % sizeX;
            out[sy * sizeX + sx] = in[y * sizeX + x];
        }
    }
}

// ============================================================================
// 核心计算函数：calcSOCS（简化版本）
// ============================================================================

void calc_socs_simple(
    // 输入参数
    cmpxData_t* mskf,           // Mask频域数据
    cmpxData_t* krns,           // SOCS核数组
    data_t* scales,             // 特征值数组
    
    // 输出
    data_t* image,              // 空中像
    
    // 参数
    int Lx,
    int Ly,
    int Nx,
    int Ny,
    int nk
) {
    // 计算尺寸参数
    int krnSizeX = 2 * Nx + 1;
    int krnSizeY = 2 * Ny + 1;
    int sizeX = 4 * Nx + 1;
    int sizeY = 4 * Ny + 1;
    int Lxh = Lx / 2;
    int Lyh = Ly / 2;
    int difX = sizeX - krnSizeX;  // = 2*Nx
    int difY = sizeY - krnSizeY;  // = 2*Ny
    
    // 临时缓冲
    cmpxData_t tmp_conv[65 * 65];  // 最大尺寸
    data_t tmp_image[65 * 65];
    data_t tmp_shifted[65 * 65];
    
#pragma HLS ARRAY_PARTITION variable=tmp_conv cyclic factor=4 dim=1
#pragma HLS ARRAY_PARTITION variable=tmp_image cyclic factor=4 dim=1
    
    // 初始化累加器
    for (int i = 0; i < sizeX * sizeY; i++) {
#pragma HLS PIPELINE
        tmp_image[i] = 0.0;
    }
    
    // ============================================================================
    // Step 1: 循环nk个核，进行点乘、IFFT、加权累加
    // ============================================================================
    
    for (int k = 0; k < nk; k++) {
        // Step 1a: 核与mask频域点乘（中心区域）
        for (int y = -Ny; y <= Ny; y++) {
            for (int x = -Nx; x <= Nx; x++) {
#pragma HLS PIPELINE
                
                int msk_idx = (y + Lyh) * Lx + (x + Lxh);
                int krn_idx = k * (krnSizeX * krnSizeY) + (y + Ny) * krnSizeX + (x + Nx);
                int conv_idx = (difY + y + Ny) * sizeX + (difX + x + Nx);
                
                // 点乘：mskf * krn
                tmp_conv[conv_idx] = mskf[msk_idx] * krns[krn_idx];
            }
        }
        
        // Step 1b: IFFT变换到空域 (简化版本：直接计算模平方)
        // TODO: 后续集成真实FFT
        for (int i = 0; i < sizeX * sizeY; i++) {
#pragma HLS PIPELINE
            // 简化处理：取模值（实际需IFFT）
            data_t re = tmp_conv[i].real();
            data_t im = tmp_conv[i].imag();
            data_t magnitude_sq = re * re + im * im;
            
            // Step 1c: 加权累加 scales[k] × |z|²
            tmp_image[i] += scales[k] * magnitude_sq;
        }
    }
    
    // ============================================================================
    // Step 2: fftshift
    // ============================================================================
    
    my_shift_2d(tmp_image, tmp_shifted, sizeX, sizeY, true, true);
    
    // ============================================================================
    // Step 3: 傅里叶插值放大（简化版本）
    // ============================================================================
    
    // TODO: 实现完整的傅里叶插值
    // 当前简化版本：直接复制（需验证FFT集成）
    for (int y = 0; y < Ly; y++) {
        for (int x = 0; x < Lx; x++) {
#pragma HLS PIPELINE
            int src_x = x % sizeX;
            int src_y = y % sizeY;
            image[y * Lx + x] = tmp_shifted[src_y * sizeX + src_x];
        }
    }
}