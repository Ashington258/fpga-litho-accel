/**
 * @file socs_2048_stream_refactored_v18_hls_fft.cpp
 * @brief SOCS 核心计算模块实现 —— 基于 HLS FFT IP 的高精度实现
 * 
 * 本文件实现了 SOCS 算法的五个核心计算阶段：
 * 1. 频域嵌入（Embed）：本征核与掩模频谱的逐点复数乘法
 * 2. 二维 IFFT：基于行-列分解的逆傅里叶变换
 * 3. 强度累加（Accumulate）：加权强度计算与累加
 * 4. 频移居中（FFTshift）：零频分量位置调整
 * 5. DDR 写回（Output）：结果通过 AXI-MM 接口输出
 * 
 * 关键设计决策：
 * - 采用编译时常量循环边界，确保综合时延确定性
 * - 块浮点缩放模式，在精度与资源间取得平衡
 * - 多端口 AXI-MM 架构，最大化 DDR 带宽利用率
 * 
 * @author FPGA-Litho Project
 * @version 18.0
 */

#include "socs_2048_stream_refactored_v18_hls_fft.h"
#include "hls_fft_config_2048_v16.h"
#include <hls_stream.h>
#include <cmath>

// ============================================================================
// 辅助函数实现
// ============================================================================

/**
 * @brief 频域嵌入模块 —— 本征核与掩模频谱的逐点复数乘法
 * 
 * 该模块实现 SOCS 公式中的频域点乘操作：
 * \f[
 * \text{fft\_input}(\nu_x, \nu_y) = K_k(\nu_x, \nu_y) \cdot M(\nu_x, \nu_y)
 * \f]
 * 
 * 并将结果零填充至 128×128 的 FFT 网格。
 * 
 * @param[in] mskf_r      掩模频谱实部
 * @param[in] mskf_i      掩模频谱虚部
 * @param[in] krn_r       本征核实部
 * @param[in] krn_i       本征核虚部
 * @param[out] fft_input  FFT 输入缓冲（零填充后）
 * @param[in] nx_actual   频域采样点数（x 方向）
 * @param[in] ny_actual   频域采样点数（y 方向）
 * @param[in] kernel_idx  当前处理的核索引
 * @param[in] Lx          掩模尺寸（x 方向）
 * @param[in] Ly          掩模尺寸（y 方向）
 * 
 * @note 关键设计：采用编译时常量 MAX_KERNEL_SIZE 作为循环边界，
 *       在循环体内通过运行时条件判断处理实际核尺寸，
 *       确保 HLS 综合时延可精确推断。
 */
void embed_kernel_mask_padded_2048_v18(
    float* mskf_r, float* mskf_i,
    float* krn_r, float* krn_i,
    cmpx_2048_t fft_input[MAX_FFT_Y][MAX_FFT_X],
    int nx_actual, int ny_actual,
    int kernel_idx,
    int Lx, int Ly
) {
    #pragma HLS INLINE off
    
    // 计算嵌入位置（与 CPU Golden 参考实现保持一致）
    // 嵌入位置计算公式：embed_x = (FFT_SIZE × (64 - kerX)) / 64
    // 对于 Nx=8：kerX=17, embed_x = (128×(64-17))/64 = 94
    int kerX_actual = 2 * nx_actual + 1;  ///< 本征核实际尺寸（x 方向）
    int kerY_actual = 2 * ny_actual + 1;  ///< 本征核实际尺寸（y 方向）
    int embed_x = (MAX_FFT_X * (64 - kerX_actual)) / 64;  ///< FFT 网格中的嵌入起始位置（x）
    int embed_y = (MAX_FFT_Y * (64 - kerY_actual)) / 64;  ///< FFT 网格中的嵌入起始位置（y）
    
    // 掩模频谱中心坐标（频域零频位置）
    int Lxh = Lx / 2;  ///< 掩模频谱中心 x 坐标
    int Lyh = Ly / 2;  ///< 掩模频谱中心 y 坐标
    
    // 本征核在 DDR 存储中的偏移量
    int kernel_offset = kernel_idx * kerX_actual * kerY_actual;
    
    // 步骤 1：清零 FFT 输入数组（实现零填充）
    // 采用编译时常量边界配合 PIPELINE 指令，确保综合时延确定性
    clear_fft_input:
    for (int y = 0; y < MAX_FFT_Y; y++) {
        for (int x = 0; x < MAX_FFT_X; x++) {
            #pragma HLS PIPELINE II=1
            fft_input[y][x] = cmpx_2048_t(0.0f, 0.0f);
        }
    }
    
    // 步骤 2：计算并嵌入本征核与掩模频谱的乘积
    // 关键设计：使用编译时常量 MAX_KERNEL_SIZE 作为循环边界
    // 运行时条件判断：仅处理实际核尺寸范围内的数据，其余保持零值
    embed_kernel:
    for (int ky = 0; ky < MAX_KERNEL_SIZE; ky++) {
        for (int kx = 0; kx < MAX_KERNEL_SIZE; kx++) {
            #pragma HLS PIPELINE II=1
            
            // 边界检查：跳过超出实际核尺寸的迭代
            if (ky < kerY_actual && kx < kerX_actual) {
                // 将核索引转换为空间域偏移量
                int y_offset = ky - ny_actual;
                int x_offset = kx - nx_actual;
                
                // FFT 网格中的目标位置
                int fft_y = embed_y + ky;
                int fft_x = embed_x + kx;
                
                // 掩模频谱中的源位置
                int mask_y = Lyh + y_offset;
                int mask_x = Lxh + x_offset;
                
                // 边界检查：确保掩模频谱访问不越界
                if (mask_y >= 0 && mask_y < Ly &&
                    mask_x >= 0 && mask_x < Lx) {
                    
                    float kr_r = krn_r[kernel_offset + ky * kerX_actual + kx];
                    float kr_i = krn_i[kernel_offset + ky * kerX_actual + kx];
                    float ms_r = mskf_r[mask_y * Lx + mask_x];
                    float ms_i = mskf_i[mask_y * Lx + mask_x];
                    
                    float prod_r = kr_r * ms_r - kr_i * ms_i;
                    float prod_i = kr_r * ms_i + kr_i * ms_r;
                    
                    fft_input[fft_y][fft_x] = cmpx_2048_t(prod_r, prod_i);
                }
            }
        }
    }
}

/**
 * @brief 强度累加模块 —— 加权强度计算与累加
 * 
 * 该模块实现 SOCS 公式中的强度计算与累加操作：
 * \f[
 * \text{tmpImg}(x,y) \leftarrow \text{tmpImg}(x,y) + \sigma_k \times |E_k(x,y)|^2
 * \f]
 * 
 * 其中 \f$|E_k|^2 = \text{re}^2 + \text{im}^2\f$ 为电场强度，
 * \f$\sigma_k\f$ 为第 k 个本征核对应的特征值权重。
 * 
 * @param[in] fft_output  IFFT 输出（复数电场）
 * @param[in,out] tmpImg  累加缓冲区（跨核累加）
 * @param[in] scale       特征值权重 \f$\sigma_k\f$
 * 
 * @note 该模块是整个流水线中唯一存在跨核数据依赖的模块，
 *       各核的强度贡献需累加至同一临时图像缓冲区。
 */
void accumulate_intensity_2048(
    cmpx_2048_t fft_output[MAX_FFT_Y][MAX_FFT_X],
    float tmpImg[MAX_FFT_Y][MAX_FFT_X],
    float scale
) {
    #pragma HLS INLINE off
    
    for (int y = 0; y < MAX_FFT_Y; y++) {
        for (int x = 0; x < MAX_FFT_X; x++) {
            #pragma HLS PIPELINE II=1
            
            // 计算电场强度：|E|^2 = re^2 + im^2
            float re = fft_output[y][x].real();
            float im = fft_output[y][x].imag();
            float intensity = re * re + im * im;
            
            // 加权累加：tmpImg += σ_k × |E|^2
            tmpImg[y][x] += scale * intensity;
        }
    }
}

/**
 * @brief 频移居中模块 —— 零频分量位置调整
 * 
 * 该模块实现 FFTshift 操作，将 IFFT 输出中的零频分量从频谱角落
 * 移动至中心位置，对应数学上的 \f$\pi\f$ 相移操作。
 * 
 * 实现方式：四个象限的对角交换
 * - 左上象限 ↔ 右下象限
 * - 右上象限 ↔ 左下象限
 * 
 * @param[in] input   输入图像（零频在角落）
 * @param[out] output 输出图像（零频在中心）
 * 
 * @note 采用基于模运算的源地址映射，通过 PIPELINE II=1 实现
 *       每时钟周期处理一个像素的高吞吐率。
 */
void fftshift_2d_2048(
    float input[MAX_FFT_Y][MAX_FFT_X],
    float output[MAX_FFT_Y][MAX_FFT_X]
) {
    #pragma HLS INLINE off
    
    int half_y = MAX_FFT_Y / 2;
    int half_x = MAX_FFT_X / 2;
    
    for (int y = 0; y < MAX_FFT_Y; y++) {
        for (int x = 0; x < MAX_FFT_X; x++) {
            #pragma HLS PIPELINE II=1
            int src_y = (y + half_y) % MAX_FFT_Y;
            int src_x = (x + half_x) % MAX_FFT_X;
            output[y][x] = input[src_y][src_x];
        }
    }
}

// ============================================================================
// 数据类型转换函数（块浮点缩放模式）
// ============================================================================

void convert_float_to_apfixed_v18(
    cmpx_2048_t input[MAX_FFT_Y][MAX_FFT_X],
    cmpx_fft_in_t output[128][128]
) {
    #pragma HLS INLINE off
    
    for (int y = 0; y < MAX_FFT_Y; y++) {
        for (int x = 0; x < MAX_FFT_X; x++) {
            #pragma HLS PIPELINE II=1
            float r = input[y][x].real();
            float i = input[y][x].imag();
            output[y][x] = cmpx_fft_in_t(data_fft_in_t(r), data_fft_in_t(i));
        }
    }
}

void convert_apfixed_to_float_v18(
    cmpx_fft_out_t input[128][128],
    cmpx_2048_t output[MAX_FFT_Y][MAX_FFT_X],
    bool is_inverse,
    unsigned blk_exp
) {
    #pragma HLS INLINE off
    
    // 块浮点缩放补偿：output × 2^(blk_exp)
    // blk_exp 为 FFT IP 内部累计的移位量
    float compensation = powf(2.0f, (float)blk_exp);
    
    #ifndef __SYNTHESIS__
    printf("[DEBUG COMPENSATION] is_inverse=%d, blk_exp=%u, compensation=%.1f\n", 
           is_inverse, blk_exp, compensation);
    #endif
    
    for (int y = 0; y < MAX_FFT_Y; y++) {
        for (int x = 0; x < MAX_FFT_X; x++) {
            #pragma HLS PIPELINE II=1
            float r = (float)input[y][x].real() * compensation;
            float i = (float)input[y][x].imag() * compensation;
            output[y][x] = cmpx_2048_t(r, i);
        }
    }
}

// ============================================================================
// 二维 FFT 包装函数（顺序执行模式）
// ============================================================================

void fft_2d_full_2048_hls_fft_v18(
    cmpx_2048_t input[MAX_FFT_Y][MAX_FFT_X],
    cmpx_2048_t output[MAX_FFT_Y][MAX_FFT_X],
    bool is_inverse
) {
    // 关键设计：外层不启用 DATAFLOW，采用顺序执行模式
    // 确保联合仿真中 FIFO 深度可精确推断，延迟确定性高
    // 各子函数内部通过 PIPELINE II=1 实现指令级流水，吞吐率仍有保障
    
    cmpx_fft_in_t input_fixed[128][128];
    cmpx_fft_out_t output_fixed[128][128];
    
    #pragma HLS RESOURCE variable=input_fixed core=RAM_2P_BRAM
    #pragma HLS RESOURCE variable=output_fixed core=RAM_2P_BRAM
    
    // Step 1: Convert float to ap_fixed
    convert_float_to_apfixed_v18(input, input_fixed);
    
    // Step 2: Call HLS FFT IP
    unsigned blk_exp;
    fft_2d_hls_128_v16(input_fixed, output_fixed, is_inverse, &blk_exp);
    
    // Step 3: Convert ap_fixed back to float (with block floating point compensation)
    convert_apfixed_to_float_v18(output_fixed, output, is_inverse, blk_exp);
}

// ============================================================================
// SOCS 核心计算主函数实现
// ============================================================================

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
) {
    // AXI-MM Master 接口配置（7 个独立端口，最大化 DDR 带宽利用率）
    #pragma HLS INTERFACE m_axi port=mskf_r offset=slave bundle=gmem0 \
        depth=1048576 latency=32 num_read_outstanding=8 max_read_burst_length=64
    #pragma HLS INTERFACE m_axi port=mskf_i offset=slave bundle=gmem1 \
        depth=1048576 latency=32 num_read_outstanding=8 max_read_burst_length=64
    #pragma HLS INTERFACE m_axi port=scales offset=slave bundle=gmem2 \
        depth=32 latency=16 num_read_outstanding=2 max_read_burst_length=4
    #pragma HLS INTERFACE m_axi port=krn_r offset=slave bundle=gmem3 \
        depth=76832 latency=16 num_read_outstanding=4 max_read_burst_length=32
    #pragma HLS INTERFACE m_axi port=krn_i offset=slave bundle=gmem4 \
        depth=76832 latency=16 num_read_outstanding=4 max_read_burst_length=32
    #pragma HLS INTERFACE m_axi port=tmpImg_ddr offset=slave bundle=gmem5 \
        depth=16384 latency=8 num_write_outstanding=4 max_write_burst_length=64
    #pragma HLS INTERFACE m_axi port=output offset=slave bundle=gmem6 \
        depth=16384 latency=8 num_write_outstanding=4 max_write_burst_length=64
    
    // AXI-Lite Slave 接口配置（运行时参数动态配置）
    #pragma HLS INTERFACE s_axilite port=return bundle=control
    #pragma HLS INTERFACE s_axilite port=nk bundle=control
    #pragma HLS INTERFACE s_axilite port=nx_actual bundle=control
    #pragma HLS INTERFACE s_axilite port=ny_actual bundle=control
    #pragma HLS INTERFACE s_axilite port=Lx bundle=control
    #pragma HLS INTERFACE s_axilite port=Ly bundle=control
    
    // 片上 BRAM 缓冲区声明
    cmpx_2048_t fft_input[MAX_FFT_Y][MAX_FFT_X];
    cmpx_2048_t fft_output[MAX_FFT_Y][MAX_FFT_X];
    float tmpImg[MAX_FFT_Y][MAX_FFT_X];
    
    #pragma HLS RESOURCE variable=fft_input core=RAM_2P_BRAM
    #pragma HLS RESOURCE variable=fft_output core=RAM_2P_BRAM
    #pragma HLS RESOURCE variable=tmpImg core=RAM_2P_BRAM
    
    // 初始化累加缓冲区
    init_tmpimg:
    for (int y = 0; y < MAX_FFT_Y; y++) {
        for (int x = 0; x < MAX_FFT_X; x++) {
            #pragma HLS PIPELINE II=1
            tmpImg[y][x] = 0.0f;
        }
    }
    
    // 核迭代循环：顺序执行各本征核的计算
    // 关键设计：顶层采用顺序执行模式，确保联合仿真延迟确定性
    // LOOP_TRIPCOUNT 指令告知 HLS 预期迭代次数，用于延迟估算
    kernel_loop:
    for (int k = 0; k < nk; k++) {
        #pragma HLS LOOP_TRIPCOUNT min=10 max=10
        
        // 阶段 1：频域嵌入 —— 本征核与掩模频谱点乘
        embed_kernel_mask_padded_2048_v18(
            mskf_r, mskf_i,
            krn_r, krn_i,
            fft_input,
            nx_actual, ny_actual,
            k,  // kernel_idx
            Lx, Ly
        );
        
        // 阶段 2：二维 IFFT —— 基于 HLS FFT IP 的逆变换
        fft_2d_full_2048_hls_fft_v18(
            fft_input,
            fft_output,
            true  // is_inverse = true for IFFT
        );
        
        // 阶段 3：强度累加 —— 加权强度计算与累加
        accumulate_intensity_2048(
            fft_output,
            tmpImg,
            scales[k]
        );
    }
    
    // 阶段 4：频移居中 —— 零频分量位置调整
    float tmpImgp[MAX_FFT_Y][MAX_FFT_X];
    #pragma HLS RESOURCE variable=tmpImgp core=RAM_2P_BRAM
    
    fftshift_2d_2048(tmpImg, tmpImgp);
    
    // 阶段 5：DDR 写回 —— 结果通过 AXI-MM 接口输出
    copy_to_ddr:
    for (int y = 0; y < MAX_FFT_Y; y++) {
        for (int x = 0; x < MAX_FFT_X; x++) {
            #pragma HLS PIPELINE II=1
            tmpImg_ddr[y * MAX_FFT_X + x] = tmpImgp[y][x];
            output[y * MAX_FFT_X + x] = tmpImgp[y][x];
        }
    }
}
