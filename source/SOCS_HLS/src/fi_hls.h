/*
 * 傅里叶插值模块头文件
 * SOCS HLS Project
 */

#ifndef FI_HLS_H
#define FI_HLS_H

#include "data_types.h"
#include "fft_2d.h"

// C仿真兼容性
#ifdef __SYNTHESIS__
    #include "hls_stream.h"
#else
    // 已在fft_2d.h中定义模拟版本
#endif

// C仿真兼容性
#ifdef __SYNTHESIS__
    #include "hls_stream.h"
#else
    // 已在fft_2d.h中定义模拟版本
#endif

// ============================================================================
// 最大尺寸常量
// ============================================================================

const int MAX_CONV_SIZE_PADDED = 32;  // 支持17×17填充到32×32
const int MAX_IMG_SIZE_PADDED = 512;  // 最大图像尺寸

// ============================================================================
// 傅里叶插值函数声明
// ============================================================================

/**
 * 傅里叶插值（完整版本）
 * 
 * @param input      输入空域小图 (sizeX×sizeY)
 * @param output     输出空域大图 (Lx×Ly)
 * @param in_sizeX   输入宽度
 * @param in_sizeY   输入高度
 * @param out_sizeX  输出宽度
 * @param out_sizeY  输出高度
 * 
 * 流程：
 *   1. fftshift (中心化)
 *   2. R2C FFT (小图)
 *   3. 频域 zero-padding (扩展到大图)
 *   4. C2R IFFT (大图)
 *   5. fftshift (恢复)
 */
void fi_hls(
    data_t* input,
    data_t* output,
    int in_sizeX,
    int in_sizeY,
    int out_sizeX,
    int out_sizeY
);

/**
 * 傅里叶插值（简化版本，仅用于C仿真）
 * 
 * 使用最近邻插值，不执行真实FFT变换
 */
void fi_hls_simple(
    data_t* input,
    data_t* output,
    int in_sizeX,
    int in_sizeY,
    int out_sizeX,
    int out_sizeY
);

#endif // FI_HLS_H