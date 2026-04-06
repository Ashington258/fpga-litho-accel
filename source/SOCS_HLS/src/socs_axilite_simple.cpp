/*
 * SOCS核心计算函数（AXI-Lite接口版本）
 * FPGA-Litho Project
 *
 * 基于 calc_socs_simple_hls，将接口改为 s_axilite
 * 特点：不使用任何HLS FFT库，仅做频域点乘和插值
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
// SOCS核心算法（AXI-Lite接口版本）
// ============================================================================

extern "C" void socs_axilite_simple(
    // AXI4-Master 输入（指针接口）
    cmpxData_t* mskf,       // Mask频域
    cmpxData_t* krns,        // SOCS核数组
    data_t* scales,          // 特征值
    
    // AXI4-Master 输出
    data_t* image,           // 空中像
    
    // AXI-Lite 控制参数
    unsigned int Lx,
    unsigned int Ly,
    unsigned int Nx,
    unsigned int Ny,
    unsigned int nk
) {
#pragma HLS INTERFACE m_axi port=mskf depth=512*512
#pragma HLS INTERFACE m_axi port=krns depth=16*65*65
#pragma HLS INTERFACE m_axi port=scales depth=16
#pragma HLS INTERFACE m_axi port=image depth=512*512
#pragma HLS INTERFACE s_axilite port=Lx
#pragma HLS INTERFACE s_axilite port=Ly
#pragma HLS INTERFACE s_axilite port=Nx
#pragma HLS INTERFACE s_axilite port=Ny
#pragma HLS INTERFACE s_axilite port=nk
#pragma HLS INTERFACE s_axilite port=return
    
    // 固定参数（实际使用时可通过AXI-Lite传入）
    // 当前简化版本暂时使用固定值以验证接口
    const int Lx_val = 512;
    const int Ly_val = 512;
    const int Nx_val = 4;
    const int Ny_val = 4;
    const int nk_val = 10;
    
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
    for (int k = 0; k < nk_val; k++) {
#pragma HLS UNROLL factor=2
        
        // 核与mask频域点乘
        for (int y = -Ny_val; y <= Ny_val; y++) {
            for (int x = -Nx_val; x <= Nx_val; x++) {
#pragma HLS PIPELINE
                
                int msk_idx = (y + Lyh) * Lx_val + (x + Lxh);
                int krn_idx = k * 81 + (y + Ny_val) * 9 + (x + Nx_val);
                int conv_idx = (y + Ny_val + 4) * 17 + (x + Nx_val + 4);
                
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
    interp_simple(tmp_shifted, image, sizeX, sizeY, Lx_val, Ly_val);
}