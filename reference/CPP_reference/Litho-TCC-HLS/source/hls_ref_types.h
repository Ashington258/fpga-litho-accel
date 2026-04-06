/**
 * @file hls_ref_types.h
 * @brief HLS简化参考版本 - 固定数组类型定义
 * 
 * 用途：为HLS重构提供固定数组尺寸参考，便于后续迁移到实际HLS代码
 * 
 * 关键约束：
 * - 所有数组尺寸固定，符合HLS要求
 * - 使用float类型（Phase 0-4），暂不用定点
 * - TCC矩阵使用指针（AXI-Master访问DDR）
 * 
 * 参考：
 * - 原始代码：reference/CPP_reference/Litho-TCC/klitho_tcc.cpp
 * - 算法公式：reference/CPP_reference/Litho-TCC/ALGORITHM_MATH.md
 * - 尺寸约束：reference/CPP_reference/Litho-TCC/ARRAY_SIZE_CONSTRAINTS.md
 */

#ifndef HLS_REF_TYPES_H
#define HLS_REF_TYPES_H

#include "hls_math.h"
#include <complex>

// ============================================================================
// 最小配置参数（CoSim验证）
// ============================================================================
// 注意：实际HLS代码中，这些参数应通过AXI-Lite传入，此处仅作参考

const int SRC_SIZE_MIN = 33;       // 光源网格最小尺寸
const int LX_MIN = 128;            // Mask周期X最小值
const int LY_MIN = 128;            // Mask周期Y最小值
const int NX_MAX_MIN = 64;         // TCC空间频率维度X最大值（最小配置）
const int NY_MAX_MIN = 64;         // TCC空间频率维度Y最大值

// TCC矩阵最大尺寸（最小配置）
const int TCC_SIZE_MAX_MIN = (2 * NX_MAX_MIN + 1) * (2 * NY_MAX_MIN + 1); // 16641

// ============================================================================
// 中等配置参数（功能验证）
// ============================================================================
const int SRC_SIZE_MED = 51;
const int LX_MED = 256;
const int LY_MED = 256;
const int NX_MAX_MED = 128;
const int NY_MAX_MED = 128;
const int TCC_SIZE_MAX_MED = (2 * NX_MAX_MED + 1) * (2 * NY_MAX_MED + 1); // 66049

// ============================================================================
// 数据类型定义
// ============================================================================
typedef float data_t;
typedef std::complex<data_t> cmpxData_t;

// ============================================================================
// 固定数组结构体（便于参数化）
// ============================================================================
template<int SRC_SIZE, int LX, int LY, int NX_MAX, int NY_MAX>
struct FixedArrays {
    // 输入数组（可放入BRAM）
    data_t src[SRC_SIZE * SRC_SIZE];           // 光源强度分布
    data_t msk[LX * LY];                        // Mask透射率
    
    // TCC矩阵（必须使用AXI-Master访问DDR）
    // 注意：HLS中应使用指针，此处仅作结构示例
    cmpxData_t tcc[(2*NX_MAX+1) * (2*NY_MAX+1) * (2*NX_MAX+1) * (2*NY_MAX+1)]; // 超大，仅示例
    
    // Pupil临时存储（每个光源点）
    cmpxData_t pupil[(2*NX_MAX+1) * (2*NY_MAX+1)];
    
    // FFT相关数组
    cmpxData_t fft_mask[LX * LY];
    data_t image[LX * LY];
};

// ============================================================================
// 光学参数结构体（通过AXI-Lite传入）
// ============================================================================
struct OpticalParams {
    data_t NA;          // 数值孔径
    data_t lambda;      // 波长 (nm)
    data_t defocus;     // 离焦量（归一化）
    data_t Lx;          // Mask周期X (nm)
    data_t Ly;          // Mask周期Y (nm)
    data_t outerSigma;  // 光源外缘相对半径
    int srcSize;        // 光源网格尺寸
    int Nx;             // TCC维度X
    int Ny;             // TCC维度Y
};

// ============================================================================
// 辅助宏定义
// ============================================================================
#define TCC_INDEX(ny, ny_offset, nx, nx_offset, nx_max) \
    ((ny + ny_offset) * (2 * nx_max + 1) + (nx + nx_offset))

#endif // HLS_REF_TYPES_H