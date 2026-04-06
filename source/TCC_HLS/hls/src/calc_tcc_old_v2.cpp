/**
 * @file calc_tcc.cpp
 * @brief TCC矩阵计算核心实现 - Phase 1（HLS优化版）
 * 
 * 实现策略（与CPU参考完全一致）：
 * 1. 第一次循环：计算并存储pupil矩阵（每个光源点的pupil向量）
 * 2. 第二次循环：从pupil矩阵读取并累积TCC（上三角）
 * 3. 第三步：利用共轭对称性填充下三角
 * 
 * HLS优化策略：
 * - 内层循环PIPELINE II=1
 * - pupil数组ARRAY_PARTITION cyclic
 * - tcc数组通过AXI-Master访问DDR
 * - 固定大小数组替代动态分配
 * 
 * 参考：klitho_tcc.cpp calcTCC函数（第210-335行）
 */

#include "calc_tcc.h"
#include <cmath>

// ============================================================================
// 最大尺寸常量（用于固定数组声明）
// ============================================================================
// 最小配置（CoSim用）：Nx=Ny=4, srcSize=101
const int MAX_SRC_SIZE = 101;
const int MAX_TCC_SIZE = 81;  // (2*4+1)*(2*4+1)
const int MAX_OSGM = 45;      // ceil(50 * 0.9)

// ============================================================================
// TCC矩阵计算主函数（HLS优化版）
// ============================================================================
void calc_tcc(
    data_t* src,
    int srcSize,
    cmpxData_t* tcc,
    OpticalParams& params,
    int tccSize,
    int sh,
    int oSgm
) {
    // 归一化参数计算
    data_t Lx_norm = params.Lx * params.NA / params.lambda;
    data_t Ly_norm = params.Ly * params.NA / params.lambda;
    data_t dz = params.defocus / (params.NA * params.NA / params.lambda);
    data_t k = 2.0 * 3.14159265358979323846 / params.lambda;
    
    // ========================================================================
    // 固定大小pupil矩阵存储（替代动态分配）
    // HLS约束：使用最大尺寸声明，实际访问受tccSize限制
    // ========================================================================
    #pragma HLS BIND_STORAGE variable=pupil_storage type=RAM_2P impl=BRAM
    cmpxData_t pupil_storage[MAX_TCC_SIZE * MAX_SRC_SIZE * MAX_SRC_SIZE];
    
    // 初始化pupil矩阵
    #pragma HLS LOOP TRIPCOUNT min=0 max=MAX_TCC_SIZE*MAX_SRC_SIZE*MAX_SRC_SIZE
    for (int i = 0; i < tccSize * srcSize * srcSize; i++) {
        #pragma HLS PIPELINE II=1
        pupil_storage[i] = cmpxData_t(0.0, 0.0);
    }
    
    // 初始化TCC矩阵
    #pragma HLS LOOP TRIPCOUNT min=0 max=MAX_TCC_SIZE*MAX_TCC_SIZE
    for (int i = 0; i < tccSize * tccSize; i++) {
        #pragma HLS PIPELINE II=1
        tcc[i] = cmpxData_t(0.0, 0.0);
    }
    
    // ========================================================================
    // 第一次循环：计算pupil矩阵
    // 循环结构：q(光源Y) → p(光源X) → ny(空间频率Y) → nx(空间频率X)
    // ========================================================================
    // 外层光源循环：动态边界取决于oSgm
    #pragma HLS LOOP TRIPCOUNT min=1 max=2*MAX_OSGM+1 avg=91
    for (int q = -oSgm; q <= oSgm; q++) {
        #pragma HLS LOOP TRIPCOUNT min=1 max=2*MAX_OSGM+1 avg=91
        for (int p = -oSgm; p <= oSgm; p++) {
            int srcID = (q + sh) * srcSize + (p + sh);
            
            // 光源强度检查
            if (src[srcID] != 0.0 && (p * p + q * q) <= sh * sh) {
                data_t sx = (data_t)p / (data_t)sh;
                data_t sy = (data_t)q / (data_t)sh;
                
                // 空间频率边界
                int nxMinTmp = (-1 - sx) * Lx_norm;
                int nxMaxTmp = (1 - sx) * Lx_norm;
                int nyMinTmp = (-1 - sy) * Ly_norm;
                int nyMaxTmp = (1 - sy) * Ly_norm;
                
                // 内层空间频率循环
                #pragma HLS LOOP TRIPCOUNT min=1 max=9 avg=5
                for (int ny = nyMinTmp; ny <= nyMaxTmp; ny++) {
                    data_t fy = (data_t)ny / Ly_norm + sy;
                    
                    if (fy * fy <= 1.0) {
                        #pragma HLS LOOP TRIPCOUNT min=1 max=9 avg=5
                        for (int nx = nxMinTmp; nx <= nxMaxTmp; nx++) {
                            #pragma HLS PIPELINE II=1
                            data_t fx = (data_t)nx / Lx_norm + sx;
                            data_t rho2 = fx * fx + fy * fy;
                            
                            if (rho2 <= 1.0) {
                                data_t phase = dz * k * hls_sqrt_tcc(1.0 - rho2 * params.NA * params.NA);
                                cmpxData_t pupil_val(hls_cos_tcc(phase), hls_sin_tcc(phase));
                                int ID1 = (ny + params.Ny) * (2 * params.Nx + 1) + (nx + params.Nx);
                                pupil_storage[ID1 * srcSize * srcSize + srcID] = pupil_val;
                            }
                        }
                    }
                }
            }
        }
    }
    
    // ========================================================================
    // 第二次循环：从pupil矩阵读取并累积TCC上三角
    // ========================================================================
    #pragma HLS LOOP TRIPCOUNT min=1 max=2*MAX_OSGM+1 avg=91
    for (int q = -oSgm; q <= oSgm; q++) {
        #pragma HLS LOOP TRIPCOUNT min=1 max=2*MAX_OSGM+1 avg=91
        for (int p = -oSgm; p <= oSgm; p++) {
            int srcID = (q + sh) * srcSize + (p + sh);
            
            if (src[srcID] != 0.0 && (p * p + q * q) <= sh * sh) {
                cmpxData_t srcVal(src[srcID], 0.0);
                data_t sx = (data_t)p / (data_t)sh;
                data_t sy = (data_t)q / (data_t)sh;
                
                int nxMinTmp = (-1 - sx) * Lx_norm;
                int nxMaxTmp = (1 - sx) * Lx_norm;
                int nyMinTmp = (-1 - sy) * Ly_norm;
                int nyMaxTmp = (1 - sy) * Ly_norm;
                
                // 遍历空间频率n1
                #pragma HLS LOOP TRIPCOUNT min=1 max=9 avg=5
                for (int ny = nyMinTmp; ny <= nyMaxTmp; ny++) {
                    #pragma HLS LOOP TRIPCOUNT min=1 max=9 avg=5
                    for (int nx = nxMinTmp; nx <= nxMaxTmp; nx++) {
                        #pragma HLS PIPELINE II=1
                        int ID1 = (ny + params.Ny) * (2 * params.Nx + 1) + (nx + params.Nx);
                        cmpxData_t pupil_n1 = pupil_storage[ID1 * srcSize * srcSize + srcID];
                        
                        if (pupil_n1 != cmpxData_t(0.0, 0.0)) {
                            cmpxData_t tmpValComplex = pupil_n1 * srcVal;
                            
                            // 累积TCC上三角
                            #pragma HLS LOOP TRIPCOUNT min=1 max=9 avg=5
                            for (int my = ny; my <= nyMaxTmp; my++) {
                                int startmx = (ny == my) ? nx : nxMinTmp;
                                
                                #pragma HLS LOOP TRIPCOUNT min=1 max=9 avg=5
                                for (int mx = startmx; mx <= nxMaxTmp; mx++) {
                                    #pragma HLS PIPELINE II=1 rewind
                                    int ID2 = (my + params.Ny) * (2 * params.Nx + 1) + (mx + params.Nx);
                                    cmpxData_t pupil_n2 = pupil_storage[ID2 * srcSize * srcSize + srcID];
                                    tcc[ID1 * tccSize + ID2] += tmpValComplex * std::conj(pupil_n2);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    
    // ========================================================================
    // 第三步：填充TCC矩阵下三角（共轭对称性）
    // ========================================================================
    #pragma HLS LOOP TRIPCOUNT min=0 max=MAX_TCC_SIZE
    for (int i = 0; i < tccSize; i++) {
        #pragma HLS PIPELINE II=1
        for (int j = i + 1; j < tccSize; j++) {
            tcc[j * tccSize + i] = std::conj(tcc[i * tccSize + j]);
        }
    }
}

// ============================================================================
// 辅助函数：共轭对称性填充（独立实现）
// ============================================================================
void fill_tcc_conjugate(
    cmpxData_t* tcc,
    int tccSize
) {
    #pragma HLS LOOP TRIPCOUNT min=0 max=MAX_TCC_SIZE
    for (int i = 0; i < tccSize; i++) {
        #pragma HLS PIPELINE II=1
        for (int j = i + 1; j < tccSize; j++) {
            tcc[j * tccSize + i] = std::conj(tcc[i * tccSize + j]);
        }
    }
}
                
                // 空间频率边界（与CPU参考完全一致）
                int nxMinTmp = (-1 - sx) * Lx_norm;
                int nxMaxTmp = (1 - sx) * Lx_norm;
                int nyMinTmp = (-1 - sy) * Ly_norm;
                int nyMaxTmp = (1 - sy) * Ly_norm;
                
                }

// ============================================================================
// HLS顶层函数（AXI接口封装）
// ============================================================================
#ifdef __SYNTHESIS__
void calc_tcc_top(
    data_t* src,
    cmpxData_t* tcc,
    volatile ap_uint<32>* ctrl_reg,
    int srcSize,
    int tccSize,
    int sh,
    int oSgm
) {
    // AXI接口pragma
    #pragma HLS INTERFACE m_axi port=src offset=direct bundle=SRC_MEM
    #pragma HLS INTERFACE m_axi port=tcc offset=direct bundle=TCC_MEM
    #pragma HLS INTERFACE s_axilite port=ctrl_reg bundle=CTRL
    #pragma HLS INTERFACE s_axilite port=srcSize bundle=CTRL
    #pragma HLS INTERFACE s_axilite port=tccSize bundle=CTRL
    #pragma HLS INTERFACE s_axilite port=sh bundle=CTRL
    #pragma HLS INTERFACE s_axilite port=oSgm bundle=CTRL
    #pragma HLS INTERFACE s_axilite port=return bundle=CTRL
    
    // 从AXI-Lite寄存器读取光学参数
    OpticalParams params;
    params.NA = *((data_t*)(ctrl_reg + 4));
    params.lambda = *((data_t*)(ctrl_reg + 5));
    params.defocus = *((data_t*)(ctrl_reg + 6));
    params.Lx = *((int*)(ctrl_reg + 8));
    params.Ly = *((int*)(ctrl_reg + 9));
    params.Nx = *((int*)(ctrl_reg + 10));
    params.Ny = *((int*)(ctrl_reg + 11));
    
    calc_tcc(src, srcSize, tcc, params, tccSize, sh, oSgm);
}
#endif
    
    // 调用核心计算函数
    calc_tcc(src, srcSize, tcc, params, tccSize, sh, oSgm);
}
#endif