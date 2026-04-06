/*
 * SOCS HLS 顶层函数（修复接口版本）
 * FPGA-Litho Project
 * 
 * 功能：calcSOCS算法简化版本（无FFT库），修复AXI接口类型转换问题
 */

#include "data_types.h"
#include <cmath>

// ============================================================================
// fftshift实现
// ============================================================================

void shift_2d_opt(
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
// 简化插值（用于快速验证）
// ============================================================================

void interp_opt(
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
// SOCS核心算法（修复版本）
// ============================================================================

void calc_socs_core_fixed(
    cmpxData_t* mskf,
    cmpxData_t* krns,
    data_t* scales,
    data_t* image,
    int Lx, int Ly,
    int Nx, int Ny,
    int nk
) {
#pragma HLS DATAFLOW
    
    // 计算尺寸参数
    int krnSizeX = 2 * Nx + 1;
    int krnSizeY = 2 * Ny + 1;
    int sizeX = 4 * Nx + 1;
    int sizeY = 4 * Ny + 1;
    int Lxh = Lx / 2;
    int Lyh = Ly / 2;
    
    // 临时缓冲
    cmpxData_t tmp_conv[17*17];
    data_t tmp_image[17*17];
    data_t tmp_shifted[17*17];
    
#pragma HLS ARRAY_PARTITION variable=tmp_conv cyclic factor=4 dim=1
#pragma HLS ARRAY_PARTITION variable=tmp_image cyclic factor=4 dim=1
    
    // 初始化
    init_loop: for (int i = 0; i < 17*17; i++) {
#pragma HLS PIPELINE
        tmp_image[i] = 0.0;
        tmp_conv[i] = cmpxData_t(0.0, 0.0);
    }
    
    // Step 1: 循环nk个核
    kernel_loop: for (int k = 0; k < nk; k++) {
#pragma HLS UNROLL factor=2
        
        // 核与mask频域点乘
        conv_loop_y: for (int y = -Ny; y <= Ny; y++) {
            conv_loop_x: for (int x = -Nx; x <= Nx; x++) {
#pragma HLS PIPELINE
                
                int msk_idx = (y + Lyh) * Lx + (x + Lxh);
                int krn_idx = k * (krnSizeX * krnSizeY) + (y + Ny) * krnSizeX + (x + Nx);
                int conv_idx = (y + Ny + 4) * sizeX + (x + Nx + 4);
                
                tmp_conv[conv_idx] = mskf[msk_idx] * krns[krn_idx];
            }
        }
        
        // 计算模平方并累加
        accum_loop: for (int i = 0; i < 17*17; i++) {
#pragma HLS PIPELINE
            data_t re = tmp_conv[i].real();
            data_t im = tmp_conv[i].imag();
            data_t mag_sq = re * re + im * im;
            
            tmp_image[i] += scales[k] * mag_sq;
        }
        
        // 清零卷积缓冲
        clear_loop: for (int i = 0; i < 17*17; i++) {
#pragma HLS PIPELINE
            tmp_conv[i] = cmpxData_t(0.0, 0.0);
        }
    }
    
    // Step 2: fftshift
    shift_2d_opt(tmp_image, tmp_shifted, sizeX, sizeY);
    
    // Step 3: 简化插值
    interp_opt(tmp_shifted, image, sizeX, sizeY, Lx, Ly);
}

// ============================================================================
// 顶层函数（修复接口）
// ============================================================================

extern "C" void socs_top_fixed(
    // AXI4-Master 输入
    cmpxData_t* m_axi_mskf,
    cmpxData_t* m_axi_krns,
    data_t* m_axi_scales,
    
    // AXI4-Master 输出
    data_t* m_axi_image,
    
    // AXI-Lite 控制参数（使用unsigned int避免volatile问题）
    unsigned int Lx,
    unsigned int Ly,
    unsigned int Nx,
    unsigned int Ny,
    unsigned int nk
) {
#pragma HLS INTERFACE m_axi port=m_axi_mskf depth=262144
#pragma HLS INTERFACE m_axi port=m_axi_krns depth=810
#pragma HLS INTERFACE m_axi port=m_axi_scales depth=16
#pragma HLS INTERFACE m_axi port=m_axi_image depth=262144
#pragma HLS INTERFACE s_axilite port=Lx
#pragma HLS INTERFACE s_axilite port=Ly
#pragma HLS INTERFACE s_axilite port=Nx
#pragma HLS INTERFACE s_axilite port=Ny
#pragma HLS INTERFACE s_axilite port=nk
#pragma HLS INTERFACE s_axilite port=return
    
    // 内部缓冲
    cmpxData_t mskf[512*512];
    cmpxData_t krns[10*9*9];
    data_t scales[16];
    data_t image[512*512];
    
#pragma HLS ARRAY_PARTITION variable=mskf cyclic factor=16 dim=1
#pragma HLS ARRAY_PARTITION variable=krns cyclic factor=4 dim=1
#pragma HLS ARRAY_PARTITION variable=scales cyclic factor=4 dim=1
    
    // 从AXI-MM读取
    read_mskf: for (int i = 0; i < Lx * Ly; i++) {
#pragma HLS PIPELINE
        mskf[i] = m_axi_mskf[i];
    }
    
    read_krns: for (int i = 0; i < nk * (2*Nx+1) * (2*Ny+1); i++) {
#pragma HLS PIPELINE
        krns[i] = m_axi_krns[i];
    }
    
    read_scales: for (int i = 0; i < nk; i++) {
#pragma HLS PIPELINE
        scales[i] = m_axi_scales[i];
    }
    
    // 核心计算
    calc_socs_core_fixed(mskf, krns, scales, image, 
                         (int)Lx, (int)Ly, (int)Nx, (int)Ny, (int)nk);
    
    // 输出到AXI-MM
    write_image: for (int i = 0; i < Lx * Ly; i++) {
#pragma HLS PIPELINE
        m_axi_image[i] = image[i];
    }
}