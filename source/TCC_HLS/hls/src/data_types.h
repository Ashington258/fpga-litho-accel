/**
 * @file data_types.h
 * @brief HLS数据类型定义 - Phase 0-4浮点版本
 * 
 * 用途：为TCC重构提供标准数据类型和参数结构
 * 
 * 关键约束：
 * - Phase 0-4全用浮点，Phase 5可选定点优化
 * - 所有参数通过AXI-Lite传入，不硬编码
 * - TCC矩阵使用AXI-Master访问DDR/BRAM
 * 
 * 参考：
 * - CPU算法：reference/CPP_reference/Litho-TCC/klitho_tcc.cpp
 * - 公式：reference/CPP_reference/Litho-TCC/ALGORITHM_MATH.md
 * - 尺寸：reference/CPP_reference/Litho-TCC/ARRAY_SIZE_CONSTRAINTS.md
 */

#ifndef DATA_TYPES_H
#define DATA_TYPES_H

#include <complex>
#include <cmath>
#include <cstdint>  // 用于uint32_t

// HLS数学函数替换（Phase 1时启用）
#ifdef __HLS__
#include "hls_math.h"
#include "ap_int.h"  // HLS特有类型
#else
// 普通C++环境（testbench编译）使用标准类型
typedef uint32_t ap_uint32;
#endif

// ============================================================================
// 数据类型定义
// ============================================================================
typedef float data_t;                        // 主数据类型（浮点）
typedef std::complex<data_t> cmpxData_t;     // 复数类型

// ============================================================================
// 数组尺寸常量（CoSim最小配置）
// ============================================================================
// 注意：实际HLS代码中，这些参数通过AXI-Lite传入，此处仅作参考

// 最小配置（CoSim验证）
const int SRC_SIZE_MIN = 33;                 // 光源网格尺寸
const int LX_MIN = 128;                      // Mask周期X
const int LY_MIN = 128;                      // Mask周期Y
const int NX_MAX_MIN = 64;                   // TCC空间频率维度X
const int NY_MAX_MIN = 64;                   // TCC空间频率维度Y
const int TCC_SIZE_MIN = (2 * NX_MAX_MIN + 1) * (2 * NY_MAX_MIN + 1); // 129×129=16641

// 中等配置（功能验证）
const int SRC_SIZE_MED = 51;
const int LX_MED = 256;
const int LY_MED = 256;
const int NX_MAX_MED = 128;
const int NY_MAX_MED = 128;
const int TCC_SIZE_MED = (2 * NX_MAX_MED + 1) * (2 * NY_MAX_MED + 1); // 257×257=66049

// ============================================================================
// 光学参数结构体（通过AXI-Lite传入）
// ============================================================================
struct OpticalParams {
    data_t NA;           // 数值孔径（典型值：0.33-0.8）
    data_t lambda;       // 波长 (nm)（典型值：193nm ArF）
    data_t defocus;      // 离焦量 (nm)
    data_t Lx;           // Mask周期X (nm)
    data_t Ly;           // Mask周期Y (nm)
    data_t outerSigma;   // 光源外缘相对半径
    int srcSize;         // 光源网格尺寸（必须为奇数）
    int Nx;              // TCC维度X（由 NA×Lx/λ 推导）
    int Ny;              // TCC维度Y（由 NA×Ly/λ 推导）
    
    // AXI-Lite寄存器映射（参考TODO.md）
    // 偏移地址：
    // 0x10: NA (float)
    // 0x14: lambda (float)
    // 0x18: defocus (float)
    // 0x20: Lx (float)
    // 0x24: Ly (float)
    // 0x28: outerSigma (float)
    // 0x30: srcSize (int)
    // 0x34: Nx (int)
    // 0x38: Ny (int)
};

// ============================================================================
// AXI-Lite控制寄存器结构（32位对齐）
// ============================================================================
struct CtrlReg {
    // AP_CTRL寄存器（偏移0x00）
#ifdef __HLS__
    volatile ap_uint<32> ap_ctrl;   // bit0=start, bit1=done, bit2=idle
#else
    volatile ap_uint32 ap_ctrl;     // 普通C++环境
#endif
    
    // 光学参数（偏移0x10-0x38）
    volatile data_t NA;             // 0x10
    volatile data_t lambda;         // 0x14
    volatile data_t defocus;        // 0x18
    volatile data_t Lx;             // 0x20
    volatile data_t Ly;             // 0x24
    volatile data_t outerSigma;     // 0x28
    volatile int srcSize;           // 0x30
    volatile int Nx;                // 0x34
    volatile int Ny;                // 0x38
    
    // 数据地址（偏移0x40-0x50）
#ifdef __HLS__
    volatile ap_uint<32> src_addr;  // 0x40 - Source数据地址
    volatile ap_uint<32> msk_addr;  // 0x44 - Mask数据地址
    volatile ap_uint<32> tcc_addr;  // 0x48 - TCC矩阵输出地址
    volatile ap_uint<32> img_addr;  // 0x4C - 空中像输出地址
#else
    volatile ap_uint32 src_addr;    // 普通C++环境
    volatile ap_uint32 msk_addr;    // 普通C++环境
    volatile ap_uint32 tcc_addr;    // 普通C++环境
    volatile ap_uint32 img_addr;    // 普通C++环境
#endif
};

// ============================================================================
// 辅助宏定义
// ============================================================================
// TCC矩阵索引计算（2D索引转1D）
#define TCC_INDEX(ny, ny_offset, nx, nx_offset, nx_max) \
    ((ny + ny_offset) * (2 * nx_max + 1) + (nx + nx_offset))

// Mask索引计算（考虑fftshift）
#define MASK_INDEX(y, x, Lx, Ly, Lxh, Lyh) \
    (((y + Lyh) % Ly) * Lx + ((x + Lxh) % Lx))

// ============================================================================
// 数学常量
// ============================================================================
#ifndef M_PI
#define M_PI 3.14159265358979323846  // 如果cmath未定义，则定义
#endif
const data_t PI = M_PI;  // 使用M_PI作为基础

// ============================================================================
// 共轭对称性相关
// ============================================================================
// TCC矩阵是Hermitian矩阵：TCC(n2, n1) = conj(TCC(n1, n2))
// 因此只计算上三角，下三角通过共轭填充

inline cmpxData_t conj_complex(const cmpxData_t& val) {
    return cmpxData_t(val.real(), -val.imag());
}

// ============================================================================
// 验证辅助函数（用于testbench）
// ============================================================================
inline bool verify_relative_error(data_t expected, data_t actual, data_t tolerance = 1e-5) {
    if (std::abs(expected) < tolerance) {
        return std::abs(actual - expected) < tolerance;
    }
    data_t rel_error = std::abs((actual - expected) / expected);
    return rel_error < tolerance;
}

#endif // DATA_TYPES_H