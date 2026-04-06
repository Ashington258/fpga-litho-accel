/*
 * SOCS HLS 顶层函数
 * FPGA-Litho Project
 * 
 * 功能: 实现 calcSOCS 算法
 *   1. nk个核与mask频域点乘
 *   2. IFFT变换到空域
 *   3. |z|²加权累加
 *   4. fftshift
 *   5. 傅里叶插值放大
 */

#include "data_types.h"
#include "hls_stream.h"
#include "hls_fft.h"

// ============================================================================
// FFT 配置（2D FFT需要自定义）
// ============================================================================

// FFT IP 参数配置（基于参考代码）
struct config1 {
    static const unsigned FFT_INPUT_WIDTH = 32;   // float32
    static const unsigned FFT_OUTPUT_WIDTH = 32;
    static const unsigned FFT_TWIDDLE_WIDTH = 32;
    static const unsigned FFT_NFFT_MAX = 12;      // log2(4096)
    static const unsigned FFT_CHANNELS = 1;
    
    static const bool FFT_HAS_NFFT = false;       // 固定长度
    static const bool FFT_CYCLIC_PREFIX_INSERTION = false;
    static const bool FFT_XK_INDEX = false;
    static const bool FFT_OVFLO = true;
    
    static const hls::ip_fft::arch FFT_ARCH = hls::ip_fft::pipelined_streaming_io;
    static const hls::ip_fft::ordering FFT_OUTPUT_ORDER = hls::ip_fft::natural_order;
    static const hls::ip_fft::scaling FFT_SCALING = hls::ip_fft::scale;
    static const hls::ip_fft::rounding FFT_ROUNDING = hls::ip_fft::truncate;
};

// ============================================================================
// 辅助函数：fftshift (myShift)
// ============================================================================

template<typename T>
void my_shift_2d(
    T* in,
    T* out,
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
// 核心计算函数：calcSOCS
// ============================================================================

void calc_socs_core(
    // 输入参数
    cmpxData_t* mskf,           // Mask频域数据 (Lx×Ly)
    cmpxData_t* krns,           // SOCS核数组 (nk × krnSizeX×krnSizeY)
    data_t* scales,             // 特征值数组 (nk)
    
    // 输出
    data_t* image,              // 空中像 (Lx×Ly)
    
    // 参数（AXI-Lite传入）
    int Lx,
    int Ly,
    int Nx,
    int Ny,
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
    int difX = sizeX - krnSizeX;  // = 2*Nx
    int difY = sizeY - krnSizeY;  // = 2*Ny
    
    // 临时缓冲（分配固定最大尺寸）
    cmpxData_t tmp_conv[MAX_CONV_SIZE * MAX_CONV_SIZE];
    data_t tmp_image[MAX_CONV_SIZE * MAX_CONV_SIZE];
    data_t tmp_shifted[MAX_CONV_SIZE * MAX_CONV_SIZE];
    
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
#pragma HLS UNROLL factor=4  // 并行处理多个核
        
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
        
        // Step 1b: IFFT变换到空域 (sizeX×sizeY)
        // 注意：HLS不支持直接2D IFFT，需要后续实现 row-FFT + col-FFT
        
        // TODO: 集成2D IFFT
        // 当前简化版本：直接填充零频分量（等待FFT集成）
        for (int i = 0; i < sizeX * sizeY; i++) {
#pragma HLS PIPELINE
            // 简化处理：取模值（实际需IFFT）
            data_t re = tmp_conv[i].real();
            data_t im = tmp_conv[i].imag();
            data_t magnitude = re * re + im * im;
            
            // Step 1c: 加权累加 scales[k] × |z|²
            tmp_image[i] += scales[k] * magnitude;
        }
    }
    
    // ============================================================================
    // Step 2: fftshift
    // ============================================================================
    
    my_shift_2d<data_t>(tmp_image, tmp_shifted, sizeX, sizeY, true, true);
    
    // ============================================================================
    // Step 3: 傅里叶插值放大 (sizeX×sizeY → Lx×Ly)
    // ============================================================================
    
    // TODO: 实现完整的傅里叶插值
    // 当前简化版本：直接复制中心区域（等待FFT集成）
    for (int y = 0; y < Ly; y++) {
        for (int x = 0; x < Lx; x++) {
#pragma HLS PIPELINE
            
            // 简化处理：将小图放大到大图（需实现真实插值）
            int src_x = x % sizeX;
            int src_y = y % sizeY;
            image[y * Lx + x] = tmp_shifted[src_y * sizeX + src_x];
        }
    }
}

// ============================================================================
// HLS顶层函数（接口设计）
// ============================================================================

void socs_top(
    // AXI-Lite 控制接口（参数传入）
    volatile ap_uint<32>* ctrl_reg,  // [0]: Lx, [1]: Ly, [2]: Nx, [3]: Ny, [4]: nk, [5]: ap_start
    
    // AXI4-Master 输入（大数组高效传输）
    cmpxData_t* m_axi_mskf,          // mask频域数据 (Lx×Ly)
    cmpxData_t* m_axi_krns,          // SOCS核数组 (nk × krnSize)
    data_t* m_axi_scales,            // 特征值数组 (nk)
    
    // AXI4-Stream 输出（高吞吐）
    hls::stream<data_t>& m_axis_image  // 空中像输出 (Lx×Ly)
) {
#pragma HLS INTERFACE m_axi port=m_axi_mskf depth=512*512
#pragma HLS INTERFACE m_axi port=m_axi_krns depth=16*65*65
#pragma HLS INTERFACE m_axi port=m_axi_scales depth=16
#pragma HLS INTERFACE axis port=m_axis_image depth=512*512
#pragma HLS INTERFACE s_axilite port=ctrl_reg bundle=control
#pragma HLS INTERFACE ap_ctrl_none port=return
    
    // 解析控制参数
    int Lx = ctrl_reg[0];
    int Ly = ctrl_reg[1];
    int Nx = ctrl_reg[2];
    int Ny = ctrl_reg[3];
    int nk = ctrl_reg[4];
    
    // 检查启动信号
    if (ctrl_reg[5] != 1) {
        return;  // 等待ap_start
    }
    
    // 内部缓冲（从AXI-MM读取）
    cmpxData_t mskf[MAX_IMG_SIZE * MAX_IMG_SIZE];
    cmpxData_t krns[MAX_NK * MAX_CONV_SIZE * MAX_CONV_SIZE];
    data_t scales[MAX_NK];
    data_t image[MAX_IMG_SIZE * MAX_IMG_SIZE];
    
#pragma HLS ARRAY_PARTITION variable=mskf cyclic factor=16 dim=1
#pragma HLS ARRAY_PARTITION variable=krns cyclic factor=4 dim=1
#pragma HLS ARRAY_PARTITION variable=scales cyclic factor=4 dim=1
    
    // 从AXI-MM读取输入数据（burst传输）
    int mskf_size = Lx * Ly;
    int krn_size = nk * (2*Nx+1) * (2*Ny+1);
    
    for (int i = 0; i < mskf_size; i++) {
#pragma HLS PIPELINE
        mskf[i] = m_axi_mskf[i];
    }
    
    for (int i = 0; i < krn_size; i++) {
#pragma HLS PIPELINE
        krns[i] = m_axi_krns[i];
    }
    
    for (int i = 0; i < nk; i++) {
#pragma HLS PIPELINE
        scales[i] = m_axi_scales[i];
    }
    
    // 调用核心计算
    calc_socs_core(mskf, krns, scales, image, Lx, Ly, Nx, Ny, nk);
    
    // 输出到AXI-Stream
    for (int i = 0; i < Lx * Ly; i++) {
#pragma HLS PIPELINE
        m_axis_image.write(image[i]);
    }
}