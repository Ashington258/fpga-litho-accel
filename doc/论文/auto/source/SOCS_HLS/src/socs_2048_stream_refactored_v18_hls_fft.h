/**
 * @file socs_2048_stream_refactored_v18_hls_fft.h
 * @brief SOCS 核心计算模块 —— 基于 HLS FFT IP 的高精度实现
 * 
 * 本模块实现了 SOCS（Sum of Coherent Sources）算法的核心计算流程，
 * 包括频域嵌入、二维逆傅里叶变换、强度累加及频移居中等关键步骤。
 * 
 * 设计特点：
 * 1. 采用编译时常量循环边界，确保综合时延确定性
 * 2. 集成 Xilinx HLS FFT IP 核，支持块浮点缩放模式
 * 3. 配置 7 个独立 AXI-MM Master 接口，最大化 DDR 带宽利用率
 * 4. 通过 AXI-Lite Slave 接口实现运行时参数动态配置
 * 
 * @author FPGA-Litho Project
 * @version 18.0
 */

#ifndef SOCS_2048_STREAM_REFACTORED_V18_HLS_FFT_H
#define SOCS_2048_STREAM_REFACTORED_V18_HLS_FFT_H

#include <complex>
#include <ap_fixed.h>

// ============================================================================
// 系统参数配置
// ============================================================================

/**
 * @brief FFT 网格尺寸
 * 
 * 固定为 128×128，通过零填充机制支持 Nx ∈ [2, 24] 的广泛取值范围。
 * 该尺寸在计算复杂度与成像分辨率之间取得了良好平衡。
 */
const int MAX_FFT_X = 128;  ///< FFT 网格宽度
const int MAX_FFT_Y = 128;  ///< FFT 网格高度

/**
 * @brief 本征核最大尺寸
 * 
 * 定义为 2×Nx_max+1，其中 Nx_max=8 对应 golden_1024 配置。
 * 采用编译时常量作为循环边界，确保 HLS 综合时延可精确推断。
 */
const int MAX_KERNEL_SIZE = 17;  ///< 本征核最大尺寸 (2×8+1)

// ============================================================================
// 数据类型定义
// ============================================================================

/**
 * @brief 复数浮点类型
 * 
 * 用于表示频域复数数据，包括掩模频谱、本征核及 IFFT 输出。
 * 采用单精度浮点以满足光刻仿真精度要求。
 */
typedef std::complex<float> cmpx_2048_t;

// ============================================================================
// 核心计算函数
// ============================================================================

/**
 * @brief SOCS 核心计算主函数
 * 
 * 实现完整的 SOCS 在线成像计算流程：
 * 
 * \f[
 * I(x,y) = \sum_{k=1}^{N_k} \sigma_k \left| \mathcal{F}^{-1}\{K_k(\nu_x,\nu_y) \cdot M(\nu_x,\nu_y)\} \right|^2
 * \f]
 * 
 * @param[in] mskf_r  掩模频谱实部（DDR 存储，尺寸 Lx×Ly）
 * @param[in] mskf_i  掩模频谱虚部（DDR 存储，尺寸 Lx×Ly）
 * @param[in] krn_r   本征核实部（DDR 存储，尺寸 nk×kerX×kerY）
 * @param[in] krn_i   本征核虚部（DDR 存储，尺寸 nk×kerX×kerY）
 * @param[in] scales  特征值权重（DDR 存储，尺寸 nk）
 * @param[out] tmpImg_ddr  中间结果缓冲（DDR 存储，尺寸 128×128）
 * @param[out] output  最终空中像输出（DDR 存储，尺寸 128×128）
 * @param[in] nk       本征核数量（运行时配置，典型值 10）
 * @param[in] nx_actual  频域采样点数（x 方向，运行时配置）
 * @param[in] ny_actual  频域采样点数（y 方向，运行时配置）
 * @param[in] Lx       掩模尺寸（x 方向，运行时配置）
 * @param[in] Ly       掩模尺寸（y 方向，运行时配置）
 * 
 * @note 所有指针参数通过 AXI-MM Master 接口访问 DDR；
 *       标量参数通过 AXI-Lite Slave 接口由主机动态配置。
 */
void calc_socs_2048_hls_stream_refactored_v18(
    float* mskf_r, float* mskf_i,
    float* krn_r, float* krn_i,
    float* scales,
    float* tmpImg_ddr,
    float* output,
    int nk,
    int nx_actual,
    int ny_actual,
    int Lx,
    int Ly
);

#endif // SOCS_2048_STREAM_REFACTORED_V18_HLS_FFT_H
