/**
 * @file calc_tcc.h
 * @brief TCC矩阵计算模块 - Phase 1核心实现
 * 
 * 功能：
 * - 计算Pupil Function（每个光源点的相位延迟）
 * - 计算TCC矩阵（传输交叉系数）
 * - 利用共轭对称性减少计算量
 * 
 * 数学公式（参考ALGORITHM_MATH.md）：
 * - Pupil: P(sx,sy,nx,ny) = exp(i * k * dz * sqrt(1 - rho² * NA²))
 * - TCC: TCC(n1,n2) = Σ S(p,q) * P(p,q,n1) * P*(p,q,n2)
 * 
 * HLS优化策略：
 * - 光源循环pipeline（最外层）
 * - pupil矩阵on-the-fly计算（不存储完整矩阵）
 * - TCC矩阵通过AXI-Master存DDR
 * - 三角函数替换：hls::sin/hls::cos/hls::sqrt
 * 
 * 参考：
 * - CPU算法：klitho_tcc.cpp calcTCC函数
 * - 数组约束：ARRAY_SIZE_CONSTRAINTS.md
 */

#ifndef CALC_TCC_H
#define CALC_TCC_H

#include "data_types.h"

// ============================================================================
// 函数声明
// ============================================================================

/**
 * @brief 计算单个光源点的pupil向量
 * 
 * @param sx        光源点X坐标（归一化，范围[-1,1]）
 * @param sy        光源点Y坐标（归一化，范围[-1,1]）
 * @param srcVal    光源强度值（复数形式，实部为强度）
 * @param params    光学参数（NA, lambda, defocus, Lx, Ly, Nx, Ny）
 * @param pupil_vec 输出pupil向量（长度为tccSize）
 * @param tccSize   TCC矩阵尺寸：(2*Nx+1)*(2*Ny+1)
 * 
 * HLS优化：
 * - 使用hls::sqrt/hls::cos/hls::sin
 * - 循环unroll因子根据tccSize调整
 */
void calc_pupil_vector(
    data_t sx,
    data_t sy,
    cmpxData_t srcVal,
    OpticalParams& params,
    cmpxData_t* pupil_vec,  // 输出：长度为tccSize
    int tccSize
);

/**
 * @brief TCC矩阵计算主函数
 * 
 * @param src           光源分布（srcSize×srcSize）
 * @param srcSize       光源网格尺寸
 * @param tcc           输出TCC矩阵（tccSize×tccSize）
 * @param params        光学参数
 * @param tccSize       TCC矩阵尺寸
 * @param sh            光源中心偏移：(srcSize-1)/2
 * @param oSgm          光源有效半径：ceil(sh * outerSigma)
 * 
 * 实现策略：
 * - 逐光源点计算pupil向量
 * - On-the-fly累积TCC（避免存储完整pupil矩阵）
 * - 共轭对称性填充下三角
 * 
 * HLS接口：
 * - src: AXI-Stream输入（或AXI-Master读取）
 * - tcc: AXI-Master写入DDR
 * - params: AXI-Lite传入
 */
void calc_tcc(
    data_t* src,            // 光源分布（归一化，总和=1）
    int srcSize,            // 光源网格尺寸
    cmpxData_t* tcc,        // 输出TCC矩阵（存储在DDR）
    OpticalParams& params,  // 光学参数
    int tccSize,            // TCC矩阵尺寸：(2*Nx+1)*(2*Ny+1)
    int sh,                 // 光源中心偏移
    int oSgm                // 光源有效半径
);

/**
 * @brief 填充TCC矩阵下三角（利用共轭对称性）
 * 
 * @param tcc       TCC矩阵（已计算上三角）
 * @param tccSize   矩阵尺寸
 * 
 * 公式：TCC(j,i) = conj(TCC(i,j))
 */
void fill_tcc_conjugate(
    cmpxData_t* tcc,
    int tccSize
);

// ============================================================================
// HLS内联函数：数学运算替换
// ============================================================================

#ifdef __HLS__
// HLS环境：使用hls_math库
#include "hls_math.h"

inline data_t hls_sqrt_tcc(data_t x) {
    return hls::sqrt(x);
}

inline data_t hls_cos_tcc(data_t x) {
    return hls::cos(x);
}

inline data_t hls_sin_tcc(data_t x) {
    return hls::sin(x);
}

inline data_t hls_fabs_tcc(data_t x) {
    return hls::fabs(x);
}

#else
// Testbench环境：使用标准库
inline data_t hls_sqrt_tcc(data_t x) {
    return std::sqrt(x);
}

inline data_t hls_cos_tcc(data_t x) {
    return std::cos(x);
}

inline data_t hls_sin_tcc(data_t x) {
    return std::sin(x);
}

inline data_t hls_fabs_tcc(data_t x) {
    return std::fabs(x);
}

#endif

#endif // CALC_TCC_H