/*
 * SOCS HLS 数据类型定义（简化版本）
 * FPGA-Litho Project
 */

#ifndef DATA_TYPES_H
#define DATA_TYPES_H

// C仿真兼容性：非综合环境下使用标准库
#ifdef __SYNTHESIS__
    #include "hls_math.h"
    #include "ap_int.h"
    #include "ap_fixed.h"
#else
    #include <cmath>
    #include <cstdint>
#endif

#include <complex>

// ============================================================================
// 基础数据类型
// ============================================================================

typedef float data_t;
typedef std::complex<data_t> cmpxData_t;

// ============================================================================
// 尺寸约束常量
// ============================================================================

const int MAX_NK = 16;
const int MAX_NXY = 16;
const int MAX_IMG_SIZE = 512;
const int MAX_CONV_SIZE = 4*16 + 1;

const int TEST_LX = 512;
const int TEST_LY = 512;
const int TEST_NX = 4;
const int TEST_NY = 4;
const int TEST_NK = 10;
const int TEST_SIZE_X = 17;
const int TEST_SIZE_Y = 17;
const int TEST_KRN_SIZE_X = 9;
const int TEST_KRN_SIZE_Y = 9;

#endif
