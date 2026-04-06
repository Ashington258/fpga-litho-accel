/**
 * @file calc_tcc_optimized.cpp
 * @brief TCC矩阵计算核心实现 - Phase 1（HLS优化版）
 * 
 * 实现策略（与CPU参考完全一致）：
 * 1. 第一次循环：计算并存储pupil矩阵（每个光源点的pupil向量）
 * 2. 第二次循环：从pupil矩阵读取并累积TCC（上三角）
 * 3. 第三步：利用共轭对称性填充下三角
 * 
 * HLS优化策略：
 * - 内层循环PIPELINE II=1
 * - pupil数组使用BRAM存储
 * - tcc数组通过AXI-Master访问DDR
 * - 固定大小数组替代动态分配
 * - TRIPCOUNT指导综合器估算循环次数
 * 
 * 参考：klitho_tcc.cpp calcTCC函数（第210-335行）
 */

#include "calc_tcc.h"
#include <cmath>

// ============================================================================
// 最大尺寸常量（用于固定数组声明，最小CoSim配置）
// ============================================================================
const int MAX_SRC_SIZE = 101;
const int MAX_TCC_SIZE = 81;  // (2*4+1)*(2*4+1)
const int MAX_OSGM = 45;      // ceil(50 * 0.9)
const int MAX_NXY = 4;        // Nx=Ny最大值

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
    // 注意：对于大尺寸配置，需要改用DDR存储
    // ========================================================================
    cmpxData_t pupil_storage[MAX_TCC_SIZE * MAX_SRC_SIZE * MAX_SRC_SIZE];
    #pragma HLS BIND_STORAGE variable=pupil_storage type=RAM_2P impl=BRAM
    
    // 初始化pupil矩阵
    init_pupil_loop:
    for (int i = 0; i < tccSize * srcSize * srcSize; i++) {
        #pragma HLS LOOP TRIPCOUNT min=0 max=MAX_TCC_SIZE*MAX_SRC_SIZE*MAX_SRC_SIZE
        #pragma HLS PIPELINE II=1
        pupil_storage[i] = cmpxData_t(0.0, 0.0);
    }
    
    // 初始化TCC矩阵
    init_tcc_loop:
    for (int i = 0; i < tccSize * tccSize; i++) {
        #pragma HLS LOOP TRIPCOUNT min=0 max=MAX_TCC_SIZE*MAX_TCC_SIZE
        #pragma HLS PIPELINE II=1
        tcc[i] = cmpxData_t(0.0, 0.0);
    }
    
    // ========================================================================
    // 第一次循环：计算pupil矩阵
    // 循环结构：q(光源Y) → p(光源X) → ny(空间频率Y) → nx(空间频率X)
    // ========================================================================
    pupil_outer_q:
    for (int q = -oSgm; q <= oSgm; q++) {
        #pragma HLS LOOP TRIPCOUNT min=1 max=2*MAX_OSGM+1 avg=91
        pupil_outer_p:
        for (int p = -oSgm; p <= oSgm; p++) {
            #pragma HLS LOOP TRIPCOUNT min=1 max=2*MAX_OSGM+1 avg=91
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
                pupil_inner_ny:
                for (int ny = nyMinTmp; ny <= nyMaxTmp; ny++) {
                    #pragma HLS LOOP TRIPCOUNT min=1 max=9 avg=5
                    data_t fy = (data_t)ny / Ly_norm + sy;
                    
                    if (fy * fy <= 1.0) {
                        pupil_inner_nx:
                        for (int nx = nxMinTmp; nx <= nxMaxTmp; nx++) {
                            #pragma HLS LOOP TRIPCOUNT min=1 max=9 avg=5
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
    tcc_outer_q:
    for (int q = -oSgm; q <= oSgm; q++) {
        #pragma HLS LOOP TRIPCOUNT min=1 max=2*MAX_OSGM+1 avg=91
        tcc_outer_p:
        for (int p = -oSgm; p <= oSgm; p++) {
            #pragma HLS LOOP TRIPCOUNT min=1 max=2*MAX_OSGM+1 avg=91
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
                tcc_inner_ny:
                for (int ny = nyMinTmp; ny <= nyMaxTmp; ny++) {
                    #pragma HLS LOOP TRIPCOUNT min=1 max=9 avg=5
                    tcc_inner_nx:
                    for (int nx = nxMinTmp; nx <= nxMaxTmp; nx++) {
                        #pragma HLS LOOP TRIPCOUNT min=1 max=9 avg=5
                        #pragma HLS PIPELINE II=1
                        int ID1 = (ny + params.Ny) * (2 * params.Nx + 1) + (nx + params.Nx);
                        cmpxData_t pupil_n1 = pupil_storage[ID1 * srcSize * srcSize + srcID];
                        
                        if (pupil_n1 != cmpxData_t(0.0, 0.0)) {
                            cmpxData_t tmpValComplex = pupil_n1 * srcVal;
                            
                            // 累积TCC上三角
                            tcc_accum_my:
                            for (int my = ny; my <= nyMaxTmp; my++) {
                                #pragma HLS LOOP TRIPCOUNT min=1 max=9 avg=5
                                int startmx = (ny == my) ? nx : nxMinTmp;
                                
                                tcc_accum_mx:
                                for (int mx = startmx; mx <= nxMaxTmp; mx++) {
                                    #pragma HLS LOOP TRIPCOUNT min=1 max=9 avg=5
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
    fill_conjugate_i:
    for (int i = 0; i < tccSize; i++) {
        #pragma HLS LOOP TRIPCOUNT min=0 max=MAX_TCC_SIZE
        #pragma HLS PIPELINE II=1
        fill_conjugate_j:
        for (int j = i + 1; j < tccSize; j++) {
            tcc[j * tccSize + i] = std::conj(tcc[i * tccSize + j]);
        }
    }
}

// ============================================================================
// 共轭对称性填充（独立函数，供外部调用）
// ============================================================================
void fill_tcc_conjugate(
    cmpxData_t* tcc,
    int tccSize
) {
    fill_i:
    for (int i = 0; i < tccSize; i++) {
        #pragma HLS LOOP TRIPCOUNT min=0 max=MAX_TCC_SIZE
        #pragma HLS PIPELINE II=1
        fill_j:
        for (int j = i + 1; j < tccSize; j++) {
            tcc[j * tccSize + i] = std::conj(tcc[i * tccSize + j]);
        }
    }
}