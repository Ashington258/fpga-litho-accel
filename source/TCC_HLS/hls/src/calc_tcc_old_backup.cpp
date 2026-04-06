/**
 * @file calc_tcc.cpp
 * @brief TCC矩阵计算核心实现 - Phase 1
 * 
 * 实现策略（基于CPU参考 klitho_tcc.cpp）：
 * 1. 逐光源点计算pupil向量（On-the-fly策略）
 * 2. 实时累积TCC矩阵（避免存储完整pupil矩阵）
 * 3. 利用共轭对称性只计算上三角
 * 
 * HLS优化：
 * - 光源循环PIPELINE（II=1）
 * - pupil计算UNROLL（根据尺寸调整）
 * - TCC累积使用ARRAY_PARTITION
 * - 数学函数替换：hls::sqrt/hls::cos/hls::sin
 */

#include "calc_tcc.h"
#include <cmath>

// ============================================================================
// 内部辅助函数：计算单个光源点的pupil向量
// ============================================================================
/**
 * @brief 计算单个光源点的pupil向量
 * 
 * 数学公式（参考ALGORITHM_MATH.md第1节）：
 * P(sx, sy, nx, ny) = exp(i * k * dz * sqrt(1 - rho² * NA²))
 * 其中：
 * - sx, sy: 光源点坐标（归一化）
 * - fx = nx/Lx_norm + sx, fy = ny/Ly_norm + sy
 * - rho² = fx² + fy²
 * 
 * @param sx        光源点X坐标（归一化，[-1,1]）
 * @param sy        光源点Y坐标（归一化，[-1,1]）
 * @param params    光学参数（包含NA, lambda, defocus, Lx, Ly, Nx, Ny）
 * @param pupil_vec 输出pupil向量（长度为tccSize）
 * @param tccSize   TCC矩阵尺寸：(2*Nx+1)*(2*Ny+1)
 */
void calc_pupil_vector(
    data_t sx,
    data_t sy,
    cmpxData_t srcVal,
    OpticalParams& params,
    cmpxData_t* pupil_vec,
    int tccSize
) {
    // 归一化参数计算（参考klitho_tcc.cpp:260行）
    data_t Lx_norm = params.Lx * params.NA / params.lambda;
    data_t Ly_norm = params.Ly * params.NA / params.lambda;
    data_t dz = params.defocus / (params.NA * params.NA / params.lambda);
    data_t k = 2.0 * 3.14159265358979323846 / params.lambda; // 2π/λ
    
    // 空间频率边界计算（与CPU参考保持一致）
    data_t nxMinTmp = (-1.0 - sx) * Lx_norm;
    data_t nxMaxTmp = ( 1.0 - sx) * Lx_norm;
    data_t nyMinTmp = (-1.0 - sy) * Ly_norm;
    data_t nyMaxTmp = ( 1.0 - sy) * Ly_norm;
    
    // 循环边界（使用整数范围）
    int nxMin = (int)nxMinTmp;
    int nxMax = (int)nxMaxTmp;
    int nyMin = (int)nyMinTmp;
    int nyMax = (int)nyMaxTmp;
    
    // 初始化pupil向量（全零）
    for (int i = 0; i < tccSize; i++) {
        pupil_vec[i] = cmpxData_t(0.0, 0.0);
    }
    
    // 遍历空间频率，计算pupil值
    for (int ny = nyMin; ny <= nyMax; ny++) {
        data_t fy = (data_t)ny / Ly_norm + sy;
        
        // 光瞳边界检查：fy² <= 1
        if (fy * fy <= 1.0) {
            for (int nx = nxMin; nx <= nxMax; nx++) {
                data_t fx = (data_t)nx / Lx_norm + sx;
                data_t rho2 = fx * fx + fy * fy;
                
                // 光瞳边界检查：rho² <= 1
                if (rho2 <= 1.0) {
                    // 计算相位延迟
                    data_t phase = dz * k * hls_sqrt_tcc(1.0 - rho2 * params.NA * params.NA);
                    
                    // 计算pupil值（复数）
                    data_t cos_phase = hls_cos_tcc(phase);
                    data_t sin_phase = hls_sin_tcc(phase);
                    cmpxData_t pupil_val(cos_phase, sin_phase);
                    
                    // 计算线性索引（参考ALGORITHM_MATH.md第2.3节）
                    int ID1 = (ny + params.Ny) * (2 * params.Nx + 1) + (nx + params.Nx);
                    
                    // 存储到pupil向量
                    pupil_vec[ID1] = pupil_val;
                }
            }
        }
    }
}

// ============================================================================
// TCC矩阵计算主函数
// ============================================================================
/**
 * @brief TCC矩阵计算（逐光源点On-the-fly策略）
 * 
 * 实现策略（避免存储完整pupil矩阵）：
 * 1. 对每个光源点(p,q)：
 *    a. 计算pupil向量（长度tccSize）
 *    b. 累积TCC矩阵的对应部分
 * 2. 利用共轭对称性只计算上三角
 * 
 * HLS优化：
 * - 外层光源循环PIPELINE（II=1）
 * - 内层循环UNROLL（根据tccSize调整）
 * - TCC矩阵通过AXI-Master存DDR
 * 
 * @param src       光源分布（归一化，总和=1）
 * @param srcSize   光源网格尺寸
 * @param tcc       输出TCC矩阵（存储在DDR）
 * @param params    光学参数
 * @param tccSize   TCC矩阵尺寸：(2*Nx+1)*(2*Ny+1)
 * @param sh        光源中心偏移：(srcSize-1)/2
 * @param oSgm      光源有效半径：ceil(sh * outerSigma)
 */
void calc_tcc(
    data_t* src,
    int srcSize,
    cmpxData_t* tcc,
    OpticalParams& params,
    int tccSize,
    int sh,
    int oSgm
) {
    // 初始化TCC矩阵（全零）
    for (int i = 0; i < tccSize * tccSize; i++) {
        tcc[i] = cmpxData_t(0.0, 0.0);
    }
    
    // 归一化参数（参考klitho_tcc.cpp:260行）
    data_t Lx_norm = params.Lx * params.NA / params.lambda;
    data_t Ly_norm = params.Ly * params.NA / params.lambda;
    
    // 预分配pupil向量存储（用于当前光源点）
    cmpxData_t pupil_vec_curr[tccSize];
    cmpxData_t pupil_vec_next[tccSize];
    
    // 外层循环：遍历光源点（q方向）
    for (int q = -oSgm; q <= oSgm; q++) {
        
        // 内层循环：遍历光源点（p方向）
        for (int p = -oSgm; p <= oSgm; p++) {
            
            // 光源点索引和强度检查
            int srcID = (q + sh) * srcSize + (p + sh);
            
            // 边界检查：光源强度非零且在圆形区域内
            if (src[srcID] != 0.0 && (p * p + q * q) <= sh * sh) {
                
                // 光源点坐标（归一化）
                data_t sx = (data_t)p / (data_t)sh;
                data_t sy = (data_t)q / (data_t)sh;
                
                // 光源强度值（复数形式）
                cmpxData_t srcVal(src[srcID], 0.0);
                
                // 计算当前光源点的pupil向量
                calc_pupil_vector(sx, sy, srcVal, params, pupil_vec_curr, tccSize);
                
                // 空间频率边界
                int nxMin = (int)((-1.0 - sx) * Lx_norm);
                int nxMax = (int)(( 1.0 - sx) * Lx_norm);
                int nyMin = (int)((-1.0 - sy) * Ly_norm);
                int nyMax = (int)(( 1.0 - sy) * Ly_norm);
                
                // 累积TCC矩阵（采用on-the-fly策略）
                // 参考：klitho_tcc.cpp:247-260行
                // 关键：在累积时实时计算pupil值（不存储完整矩阵）
                
                for (int ny = nyMin; ny <= nyMax; ny++) {
                    data_t fy_acc = (data_t)ny / Ly_norm + sy;
                    
                    // 光瞳边界检查（fy方向）
                    if (fy_acc * fy_acc <= 1.0) {
                        for (int nx = nxMin; nx <= nxMax; nx++) {
                            data_t fx_acc = (data_t)nx / Lx_norm + sx;
                            data_t rho2_acc = fx_acc * fx_acc + fy_acc * fy_acc;
                            
                            // 光瞳边界检查（rho方向）
                            if (rho2_acc <= 1.0) {
                                // 计算当前pupil值（n1）
                                data_t phase1 = dz * k * hls_sqrt_tcc(1.0 - rho2_acc * params.NA * params.NA);
                                cmpxData_t pupil_n1(hls_cos_tcc(phase1), hls_sin_tcc(phase1));
                                
                                int ID1 = (ny + params.Ny) * (2 * params.Nx + 1) + (nx + params.Nx);
                                
                                // 检查pupil非零
                                if (pupil_n1 != cmpxData_t(0.0, 0.0)) {
                                    // 加权pupil
                                    cmpxData_t pupil_weighted = pupil_n1 * srcVal;
                                    
                                    // 累积TCC上三角（只计算n2 >= n1部分）
                                    for (int my = ny; my <= nyMax; my++) {
                                        int startmx = (ny == my) ? nx : nxMin;
                                        
                                        for (int mx = startmx; mx <= nxMax; mx++) {
                                            // 计算n2的pupil值（实时计算）
                                            data_t fy2 = (data_t)my / Ly_norm + sy;
                                            data_t fx2 = (data_t)mx / Lx_norm + sx;
                                            data_t rho2_2 = fx2 * fx2 + fy2 * fy2;
                                            
                                            // 光瞳边界检查
                                            if (rho2_2 <= 1.0) {
                                                data_t phase2 = dz * k * hls_sqrt_tcc(1.0 - rho2_2 * params.NA * params.NA);
                                                cmpxData_t pupil_n2(hls_cos_tcc(phase2), hls_sin_tcc(phase2));
                                                
                                                int ID2 = (my + params.Ny) * (2 * params.Nx + 1) + (mx + params.Nx);
                                                
                                                // TCC累积公式
                                                tcc[ID1 * tccSize + ID2] += pupil_weighted * std::conj(pupil_n2);
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    
    // 填充下三角（共轭对称性）
    fill_tcc_conjugate(tcc, tccSize);
}

// ============================================================================
// 共轭对称性填充
// ============================================================================
/**
 * @brief 填充TCC矩阵下三角（利用共轭对称性）
 * 
 * 公式：TCC(j,i) = conj(TCC(i,j))
 * 
 * @param tcc       TCC矩阵（已计算上三角）
 * @param tccSize   矩阵尺寸
 */
void fill_tcc_conjugate(
    cmpxData_t* tcc,
    int tccSize
) {
    for (int i = 0; i < tccSize; i++) {
        for (int j = i + 1; j < tccSize; j++) {
            // TCC(j,i) = conj(TCC(i,j))
            tcc[j * tccSize + i] = std::conj(tcc[i * tccSize + j]);
        }
    }
}

// ============================================================================
// HLS顶层函数（封装，添加pragma）
// ============================================================================
/**
 * @brief TCC计算顶层函数（HLS封装）
 * 
 * 接口设计（参考TODO.md接口设计）：
 * - src: AXI-Master读取（或AXI-Stream输入）
 * - tcc: AXI-Master写入DDR
 * - params: AXI-Lite传入
 * 
 * HLS优化指令：
 * - 光源循环PIPELINE（II=1）
 * - pupil计算UNROLL
 * - TCC累积ARRAY_PARTITION
 */
#ifdef __HLS__
void calc_tcc_top(
    // AXI-Master接口：光源数据输入
    data_t* src,
    // AXI-Master接口：TCC矩阵输出（DDR）
    cmpxData_t* tcc,
    // AXI-Lite接口：参数配置
    volatile ap_uint<32>* ctrl_reg,
    // AXI-Lite参数（通过寄存器传入）
    int srcSize,
    int tccSize,
    int sh,
    int oSgm
) {
    // HLS接口pragma
    #pragma HLS INTERFACE m_axi port=src offset=direct bundle=SRC_MEM
    #pragma HLS INTERFACE m_axi port=tcc offset=direct bundle=TCC_MEM
    #pragma HLS INTERFACE s_axilite port=ctrl_reg bundle=CTRL
    #pragma HLS INTERFACE s_axilite port=srcSize bundle=CTRL
    #pragma HLS INTERFACE s_axilite port=tccSize bundle=CTRL
    #pragma HLS INTERFACE s_axilite port=sh bundle=CTRL
    #pragma HLS INTERFACE s_axilite port=oSgm bundle=CTRL
    #pragma HLS INTERFACE s_axilite port=return bundle=CTRL
    
    // 从AXI-Lite寄存器读取光学参数
    // 寄存器映射（参考data_types.h CtrlReg结构）
    OpticalParams params;
    params.NA = *(data_t*)(ctrl_reg + 4);       // 偏移0x10
    params.lambda = *(data_t*)(ctrl_reg + 5);   // 偏移0x14
    params.defocus = *(data_t*)(ctrl_reg + 6);  // 偏移0x18
    params.Lx = *(data_t*)(ctrl_reg + 8);       // 偏移0x20
    params.Ly = *(data_t*)(ctrl_reg + 9);       // 偏移0x24
    params.outerSigma = *(data_t*)(ctrl_reg + 10); // 偏移0x28
    params.srcSize = *(int*)(ctrl_reg + 12);    // 偏移0x30
    params.Nx = *(int*)(ctrl_reg + 13);         // 偏移0x34
    params.Ny = *(int*)(ctrl_reg + 14);         // 偏移0x38
    
    // 调用核心计算函数
    calc_tcc(src, srcSize, tcc, params, tccSize, sh, oSgm);
    
    // HLS优化：光源循环PIPELINE
    #pragma HLS PIPELINE II=1
    
    // HLS优化：pupil向量数组分区
    #pragma HLS ARRAY_PARTITION variable=pupil_vec_curr cyclic factor=4 dim=1
    #pragma HLS ARRAY_PARTITION variable=pupil_vec_next cyclic factor=4 dim=1
}
#endif