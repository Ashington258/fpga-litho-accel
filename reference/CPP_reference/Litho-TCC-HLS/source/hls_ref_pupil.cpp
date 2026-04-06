/**
 * @file hls_ref_pupil.cpp
 * @brief HLS简化参考版本 - Pupil Function计算（固定循环边界）
 * 
 * 用途：展示如何将动态循环边界转换为固定边界+条件判断
 * 
 * 关键改造点（相对于原始代码）：
 * 1. 循环边界：动态 → 固定最大值 + 条件判断
 * 2. 数学函数：sqrt/cos/sin → hls::sqrt/hls::cos/hls::sin
 * 3. 数组访问：动态索引 → 固定索引 + 边界检查
 * 
 * 参考：
 * - 原始代码：klitho_tcc.cpp:calcTCC() 第260-270行
 * - 算法公式：ALGORITHM_MATH.md 第1.3节
 */

#include "hls_ref_types.h"

/**
 * @brief 计算单个光源点的Pupil Function
 * 
 * 数学公式：
 * P(sx, sy, nx, ny) = exp(i * k * dz * sqrt(1 - rho^2 * NA^2))
 * 
 * @param sx        光源点X坐标（归一化）
 * @param sy        光源点Y坐标（归一化）
 * @param params    光学参数（NA, lambda, defocus, Lx, Ly）
 * @param pupil     输出：Pupil矩阵（尺寸：TCC_SIZE_MAX）
 * @param NX_MAX    固定：空间频率X最大值
 * @param NY_MAX    固定：空间频率Y最大值
 */
template<int NX_MAX, int NY_MAX>
void calc_pupil_single_source(
    data_t sx, data_t sy,
    const OpticalParams& params,
    cmpxData_t pupil[(2*NX_MAX+1) * (2*NY_MAX+1)]
) {
    // 归一化参数
    data_t Lx_norm = params.Lx * params.NA / params.lambda;
    data_t Ly_norm = params.Ly * params.NA / params.lambda;
    data_t dz = params.defocus / (params.NA * params.NA / params.lambda);
    data_t k = 2.0 * M_PI / params.lambda;
    
    // ============================================================================
    // HLS关键改造：固定循环边界 + 条件判断
    // ============================================================================
    // 原始代码：动态计算边界
    //   nxMinTmp = floor((-1 - sx) * Lx_norm);
    //   nxMaxTmp = floor(( 1 - sx) * Lx_norm);
    // 
    // HLS改造：遍历固定范围，内部判断是否在有效边界内
    // ============================================================================
    
    int tcc_dim_x = 2 * NX_MAX + 1;
    int tcc_dim_y = 2 * NY_MAX + 1;
    
    // 固定循环：遍历所有可能的(nx, ny)
    for (int ny_idx = 0; ny_idx < tcc_dim_y; ny_idx++) {
        for (int nx_idx = 0; nx_idx < tcc_dim_x; nx_idx++) {
#pragma HLS PIPELINE II=1
            
            // 转换为实际坐标
            int ny = ny_idx - NY_MAX;  // 范围：[-NY_MAX, +NY_MAX]
            int nx = nx_idx - NX_MAX;  // 范围：[-NX_MAX, +NX_MAX]
            
            // 计算总空间频率
            data_t fx = (data_t)nx / Lx_norm + sx;
            data_t fy = (data_t)ny / Ly_norm + sy;
            
            // 计算rho^2
            data_t rho2 = fx * fx + fy * fy;
            
            // ============================================================================
            // 条件判断：光瞳边界检查
            // ============================================================================
            // HLS中if语句会影响流水线，但此处必须保留（物理约束）
            // 可通过双路径实现优化（后续Phase）
            // ============================================================================
            
            if (rho2 <= 1.0) {
                // 计算相位
                data_t sqrt_val = hls::sqrt(1.0 - rho2 * params.NA * params.NA);
                data_t phase = k * dz * sqrt_val;
                
                // 计算复数Pupil
                data_t cos_val = hls::cos(phase);
                data_t sin_val = hls::sin(phase);
                
                pupil[ny_idx * tcc_dim_x + nx_idx] = cmpxData_t(cos_val, sin_val);
            } else {
                // 光瞳外：置零
                pupil[ny_idx * tcc_dim_x + nx_idx] = cmpxData_t(0.0, 0.0);
            }
        }
    }
}

/**
 * @brief HLS参考版本：完整Pupil计算（遍历所有光源点）
 * 
 * @param src       光源强度分布（srcSize×srcSize）
 * @param params    光学参数
 * @param pupil_all 输出：所有光源点的Pupil矩阵（存储策略待定）
 * @param SRC_SIZE  固定：光源网格尺寸
 * @param NX_MAX    固定：空间频率X最大值
 * @param NY_MAX    固定：空间频率Y最大值
 */
template<int SRC_SIZE, int NX_MAX, int NY_MAX>
void calc_pupil_all_sources(
    const data_t src[SRC_SIZE * SRC_SIZE],
    const OpticalParams& params,
    // 注意：实际HLS中 pupil_all 应通过AXI-Master访问DDR
    // 此处仅作示例，假设有足够存储
    cmpxData_t pupil_all[SRC_SIZE * SRC_SIZE * (2*NX_MAX+1) * (2*NY_MAX+1)]
) {
    int sh = (SRC_SIZE - 1) / 2;
    data_t outerSigma = params.outerSigma;
    int oSgm = (int)hls::ceil((data_t)sh * outerSigma);
    
    // 固定循环：遍历光源网格
    for (int q_idx = 0; q_idx < SRC_SIZE; q_idx++) {
        for (int p_idx = 0; p_idx < SRC_SIZE; p_idx++) {
#pragma HLS LOOP_FLATTEN
            
            // 转换为实际坐标
            int q = q_idx - sh;  // 范围：[-sh, +sh]
            int p = p_idx - sh;
            
            // 光源点归一化坐标
            data_t sx = (data_t)p / (data_t)sh;
            data_t sy = (data_t)q / (data_t)sh;
            
            // 条件判断：光源有效区域（Annular）
            data_t dist_sq = sx * sx + sy * sy;
            data_t innerSigma = 0.0;  // Annular内径（示例）
            
            if (dist_sq >= innerSigma * innerSigma && dist_sq <= outerSigma * outerSigma) {
                // 计算该光源点的Pupil
                calc_pupil_single_source<NX_MAX, NY_MAX>(sx, sy, params, 
                    &pupil_all[(q_idx * SRC_SIZE + p_idx) * (2*NX_MAX+1) * (2*NY_MAX+1)]);
            }
        }
    }
}

// ============================================================================
// 测试函数（用于验证固定边界实现）
// ============================================================================
#ifdef HLS_REF_TEST

#include <iostream>

void test_pupil_calculation() {
    // 测试参数（最小配置）
    OpticalParams params;
    params.NA = 0.8;
    params.lambda = 193.0;  // nm
    params.defocus = 0.0;
    params.Lx = 128.0;
    params.Ly = 128.0;
    params.outerSigma = 0.9;
    
    // 固定数组
    const int SRC_SIZE = 33;
    const int NX_MAX = 64;
    const int NY_MAX = 64;
    
    data_t src[SRC_SIZE * SRC_SIZE];
    cmpxData_t pupil_all[SRC_SIZE * SRC_SIZE * (2*NX_MAX+1) * (2*NY_MAX+1)];
    
    // 初始化光源（Annular）
    int sh = (SRC_SIZE - 1) / 2;
    for (int q = 0; q < SRC_SIZE; q++) {
        for (int p = 0; p < SRC_SIZE; p++) {
            data_t sx = (data_t)(p - sh) / sh;
            data_t sy = (data_t)(q - sh) / sh;
            data_t dist = hls::sqrt(sx*sx + sy*sy);
            if (dist >= 0.6 && dist <= 0.9) {
                src[q * SRC_SIZE + p] = 1.0;
            } else {
                src[q * SRC_SIZE + p] = 0.0;
            }
        }
    }
    
    // 计算Pupil
    calc_pupil_all_sources<SRC_SIZE, NX_MAX, NY_MAX>(src, params, pupil_all);
    
    // 输出样本（验证）
    std::cout << "Pupil samples (center):" << std::endl;
    int center_idx = (SRC_SIZE/2 * SRC_SIZE + SRC_SIZE/2) * (2*NX_MAX+1) * (2*NY_MAX+1);
    int pupil_center = (NY_MAX * (2*NX_MAX+1) + NX_MAX);
    cmpxData_t sample = pupil_all[center_idx + pupil_center];
    std::cout << "  Pupil[center, center]: " << sample.real() << " + j" << sample.imag() << std::endl;
}

#endif // HLS_REF_TEST