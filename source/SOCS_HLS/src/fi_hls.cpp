/*
 * 傅里叶插值模块 (Fourier Interpolation)
 * SOCS HLS Project
 * 
 * 功能: 将小图 (sizeX×sizeY) 插值放大到大图 (Lx×Ly)
 * 步骤:
 *   1. R2C FFT (小图)
 *   2. 频域 zero-padding (扩展到大图)
 *   3. C2R IFFT (大图)
 *   4. fftshift
 */

#include "fi_hls.h"
#include "fft_2d.h"
#include <cmath>

// ============================================================================
// fftshift 实现
// ============================================================================

void fi_shift_2d(
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
// 频域 zero-padding（复杂部分）
// ============================================================================

void zero_padding_freq_domain(
    fft_complex_t* small_freq,    // 输入：小图频域 (sizeX×sizeY)
    fft_complex_t* large_freq,    // 输出：大图频域 (Lx×Ly)
    int in_sizeX,
    int in_sizeY,
    int out_sizeX,
    int out_sizeY
) {
#pragma HLS INLINE
    
    int difX = out_sizeX - in_sizeX;
    int difY = out_sizeY - in_sizeY;
    
    // 清零大图频域
    for (int i = 0; i < out_sizeX * out_sizeY; i++) {
#pragma HLS PIPELINE
        large_freq[i] = fft_complex_t(0.0, 0.0);
    }
    
    // 复制低频部分（上半部分）
    int in_halfY = in_sizeY / 2 + 1;
    for (int y = 0; y < in_halfY; y++) {
        for (int x = 0; x < in_sizeX; x++) {
#pragma HLS PIPELINE
            large_freq[y * out_sizeX + x] = small_freq[y * in_sizeX + x];
        }
    }
    
    // 复制低频部分（下半部分，偏移到末尾）
    for (int y = in_halfY; y < in_sizeY; y++) {
        for (int x = 0; x < in_sizeX; x++) {
#pragma HLS PIPELINE
            int target_y = y + difY;
            large_freq[target_y * out_sizeX + x] = small_freq[y * in_sizeX + x];
        }
    }
}

// ============================================================================
// 傅里叶插值主函数（完整实现）
// ============================================================================

void fi_hls(
    data_t* input,          // 输入：空域小图 (sizeX×sizeY)
    data_t* output,         // 输出：空域大图 (Lx×Ly)
    int in_sizeX,           // 输入尺寸 (如 17×17)
    int in_sizeY,
    int out_sizeX,          // 输出尺寸 (如 512×512)
    int out_sizeY
) {
#pragma HLS DATAFLOW
    
    // ========================================================================
    // Step 1: fftshift (移到频域中心)
    // ========================================================================
    
    data_t shifted_small[MAX_CONV_SIZE * MAX_CONV_SIZE];
#pragma HLS ARRAY_PARTITION variable=shifted_small cyclic factor=4 dim=1
    
    // 将输入移位到频率中心
    int xh_in = (in_sizeX + 1) / 2;
    int yh_in = (in_sizeY + 1) / 2;
    
    for (int y = 0; y < in_sizeY; y++) {
        for (int x = 0; x < in_sizeX; x++) {
#pragma HLS PIPELINE
            int sy = (y + yh_in) % in_sizeY;
            int sx = (x + xh_in) % in_sizeX;
            shifted_small[sy * in_sizeX + sx] = input[y * in_sizeX + x];
        }
    }
    
    // ========================================================================
    // Step 2: R2C FFT (小图)
    // ========================================================================
    
    // 注意：HLS目前实现的是C2C FFT，需要适配
    // 简化方案：将实数数据转换为复数（虚部为0）
    
    fft_complex_t small_real[MAX_CONV_SIZE_PADDED * MAX_CONV_SIZE_PADDED];
    fft_complex_t small_freq[MAX_CONV_SIZE_PADDED * MAX_CONV_SIZE_PADDED];
#pragma HLS ARRAY_PARTITION variable=small_real cyclic factor=4 dim=1
#pragma HLS ARRAY_PARTITION variable=small_freq cyclic factor=4 dim=1
    
    // 零填充到FFT尺寸（如17×17填充到32×32）
    const int FFT_SMALL_SIZE = FFT_SIZE_17_PADDED;  // 32
    
    for (int i = 0; i < FFT_SMALL_SIZE * FFT_SMALL_SIZE; i++) {
#pragma HLS PIPELINE
        small_real[i] = fft_complex_t(0.0, 0.0);
    }
    
    // 复制数据到FFT缓冲（中心对齐）
    int offset_x = (FFT_SMALL_SIZE - in_sizeX) / 2;
    int offset_y = (FFT_SMALL_SIZE - in_sizeY) / 2;
    
    for (int y = 0; y < in_sizeY; y++) {
        for (int x = 0; x < in_sizeX; x++) {
#pragma HLS PIPELINE
            int idx = (y + offset_y) * FFT_SMALL_SIZE + (x + offset_x);
            small_real[idx] = fft_complex_t(shifted_small[y * in_sizeX + x], 0.0);
        }
    }
    
    // 执行2D FFT
    fft_2d_forward_32(small_real, small_freq);
    
    // ========================================================================
    // Step 3: 频域 zero-padding (扩展到大图)
    // ========================================================================
    
    // 注意：512×512需要FFT_SIZE_512
    
    fft_complex_t large_freq[MAX_IMG_SIZE * MAX_IMG_SIZE];
#pragma HLS ARRAY_PARTITION variable=large_freq cyclic factor=8 dim=1
    
    // 清零大图频域
    for (int i = 0; i < out_sizeX * out_sizeY; i++) {
#pragma HLS PIPELINE
        large_freq[i] = fft_complex_t(0.0, 0.0);
    }
    
    // 复制低频分量（Hermitian对称处理）
    int in_freq_centerY = FFT_SMALL_SIZE / 2;
    int out_freq_centerY = out_sizeY / 2;
    
    // 上半部分（低频）
    for (int y = 0; y <= in_freq_centerY; y++) {
        for (int x = 0; x < FFT_SMALL_SIZE; x++) {
#pragma HLS PIPELINE
            // 映射到大图频域
            int target_y = y;
            int target_x = x;
            
            // 中心对齐
            if (x > FFT_SMALL_SIZE / 2) {
                target_x = out_sizeX - (FFT_SMALL_SIZE - x);
            }
            
            large_freq[target_y * out_sizeX + target_x] = small_freq[y * FFT_SMALL_SIZE + x];
        }
    }
    
    // 下半部分（高频，映射到末尾）
    for (int y = in_freq_centerY + 1; y < FFT_SMALL_SIZE; y++) {
        for (int x = 0; x < FFT_SMALL_SIZE; x++) {
#pragma HLS PIPELINE
            int target_y = out_sizeY - (FFT_SMALL_SIZE - y);
            int target_x = x;
            
            if (x > FFT_SMALL_SIZE / 2) {
                target_x = out_sizeX - (FFT_SMALL_SIZE - x);
            }
            
            large_freq[target_y * out_sizeX + target_x] = small_freq[y * FFT_SMALL_SIZE + x];
        }
    }
    
    // ========================================================================
    // Step 4: C2R IFFT (大图)
    // ========================================================================
    
    fft_complex_t large_real[MAX_IMG_SIZE * MAX_IMG_SIZE];
#pragma HLS ARRAY_PARTITION variable=large_real cyclic factor=8 dim=1
    
    // 执行2D IFFT
    fft_2d_inverse_512(large_freq, large_real);
    
    // ========================================================================
    // Step 5: fftshift (恢复空域)
    // ========================================================================
    
    // 提取实部并移位
    int xh_out = (out_sizeX + 1) / 2;
    int yh_out = (out_sizeY + 1) / 2;
    
    for (int y = 0; y < out_sizeY; y++) {
        for (int x = 0; x < out_sizeX; x++) {
#pragma HLS PIPELINE
            int sy = (y + yh_out) % out_sizeY;
            int sx = (x + xh_out) % out_sizeX;
            output[sy * out_sizeX + sx] = large_real[y * out_sizeX + x].real();
        }
    }
}

// ============================================================================
// 简化版本（用于C仿真快速验证）
// ============================================================================

void fi_hls_simple(
    data_t* input,
    data_t* output,
    int in_sizeX,
    int in_sizeY,
    int out_sizeX,
    int out_sizeY
) {
    // 简化方案：最近邻插值（仅用于测试）
    for (int y = 0; y < out_sizeY; y++) {
        for (int x = 0; x < out_sizeX; x++) {
#pragma HLS PIPELINE
            // 映射到小图坐标
            int src_x = (x * in_sizeX) / out_sizeX;
            int src_y = (y * in_sizeY) / out_sizeY;
            
            // 约束范围
            if (src_x >= in_sizeX) src_x = in_sizeX - 1;
            if (src_y >= in_sizeY) src_y = in_sizeY - 1;
            
            output[y * out_sizeX + x] = input[src_y * in_sizeX + src_x];
        }
    }
}